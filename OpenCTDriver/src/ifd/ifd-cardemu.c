#include "internal.h"

#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <sys/poll.h>


static struct ifd_driver_ops cardemu_driver = { 0 };
static int cardemu_recv(ifd_reader_t* reader, unsigned int dad, unsigned char *buffer, size_t len, long timeout);

//fast crc8 algo taken from wikipedia
static const uint8_t Crc8Table[256] = {
    0x00, 0x31, 0x62, 0x53, 0xC4, 0xF5, 0xA6, 0x97,
    0xB9, 0x88, 0xDB, 0xEA, 0x7D, 0x4C, 0x1F, 0x2E,
    0x43, 0x72, 0x21, 0x10, 0x87, 0xB6, 0xE5, 0xD4,
    0xFA, 0xCB, 0x98, 0xA9, 0x3E, 0x0F, 0x5C, 0x6D,
    0x86, 0xB7, 0xE4, 0xD5, 0x42, 0x73, 0x20, 0x11,
    0x3F, 0x0E, 0x5D, 0x6C, 0xFB, 0xCA, 0x99, 0xA8,
    0xC5, 0xF4, 0xA7, 0x96, 0x01, 0x30, 0x63, 0x52,
    0x7C, 0x4D, 0x1E, 0x2F, 0xB8, 0x89, 0xDA, 0xEB,
    0x3D, 0x0C, 0x5F, 0x6E, 0xF9, 0xC8, 0x9B, 0xAA,
    0x84, 0xB5, 0xE6, 0xD7, 0x40, 0x71, 0x22, 0x13,
    0x7E, 0x4F, 0x1C, 0x2D, 0xBA, 0x8B, 0xD8, 0xE9,
    0xC7, 0xF6, 0xA5, 0x94, 0x03, 0x32, 0x61, 0x50,
    0xBB, 0x8A, 0xD9, 0xE8, 0x7F, 0x4E, 0x1D, 0x2C,
    0x02, 0x33, 0x60, 0x51, 0xC6, 0xF7, 0xA4, 0x95,
    0xF8, 0xC9, 0x9A, 0xAB, 0x3C, 0x0D, 0x5E, 0x6F,
    0x41, 0x70, 0x23, 0x12, 0x85, 0xB4, 0xE7, 0xD6,
    0x7A, 0x4B, 0x18, 0x29, 0xBE, 0x8F, 0xDC, 0xED,
    0xC3, 0xF2, 0xA1, 0x90, 0x07, 0x36, 0x65, 0x54,
    0x39, 0x08, 0x5B, 0x6A, 0xFD, 0xCC, 0x9F, 0xAE,
    0x80, 0xB1, 0xE2, 0xD3, 0x44, 0x75, 0x26, 0x17,
    0xFC, 0xCD, 0x9E, 0xAF, 0x38, 0x09, 0x5A, 0x6B,
    0x45, 0x74, 0x27, 0x16, 0x81, 0xB0, 0xE3, 0xD2,
    0xBF, 0x8E, 0xDD, 0xEC, 0x7B, 0x4A, 0x19, 0x28,
    0x06, 0x37, 0x64, 0x55, 0xC2, 0xF3, 0xA0, 0x91,
    0x47, 0x76, 0x25, 0x14, 0x83, 0xB2, 0xE1, 0xD0,
    0xFE, 0xCF, 0x9C, 0xAD, 0x3A, 0x0B, 0x58, 0x69,
    0x04, 0x35, 0x66, 0x57, 0xC0, 0xF1, 0xA2, 0x93,
    0xBD, 0x8C, 0xDF, 0xEE, 0x79, 0x48, 0x1B, 0x2A,
    0xC1, 0xF0, 0xA3, 0x92, 0x05, 0x34, 0x67, 0x56,
    0x78, 0x49, 0x1A, 0x2B, 0xBC, 0x8D, 0xDE, 0xEF,
    0x82, 0xB3, 0xE0, 0xD1, 0x46, 0x77, 0x24, 0x15,
    0x3B, 0x0A, 0x59, 0x68, 0xFF, 0xCE, 0x9D, 0xAC
};

static unsigned char CRC8(const uint8_t *source, uint8_t len)
{
    unsigned char crc = 0xFF;
    while (len--)
        crc = Crc8Table[crc ^ *source++];
    return crc;
}

// command buffer sizes
#define CMD_HDR_SIZE 1
#define CMD_HDR_SIZE_IS_1
#define CMD_CRC_SIZE 1
#define CMD_CRC_SIZE_IS_1
#define CMD_BUFF_SIZE 16
#define CMD_TIMEOUT 500
#define MAX_RESYNC_TRIES 10
#define MAX_RESYNC_GARBAGE 32

#define CMD_SIZE_MASK 0x1F
#define CMD_MAX_REMSZ 15
#define CMD_MIN_REMSZ 1
#define CMD_MAX_PLSZ  14

// requests (masks)
#define REQ_ALL_MASK 0xE0
#define REQ_CARD_STATUS 0x20
#define REQ_CARD_RESET 0x40
#define REQ_CARD_DEACTIVATE 0x60
#define REQ_CARD_SEND 0x80
#define REQ_CARD_RESPOND 0xA0
#define REQ_RESYNC_COMPLETE 0xC0
#define REQ_RESYNC 0xE0

// answers (masks)
#define ANS_ALL_MASK 0xE0
#define ANS_CARD_ABSENT 0x20
#define ANS_CARD_PRESENT 0x40
#define ANS_CARD_INACTIVE 0x60
#define ANS_CARD_DATA 0x80
#define ANS_CARD_EOD 0xA0
#define ANS_OK 0xC0
#define ANS_RESYNC 0xE0

// payload size
#define comm_get_payload_size(totalSize) ((uint8_t)(totalSize>0 ? totalSize-CMD_CRC_SIZE : totalSize))
#define comm_get_payload(cmdBuffPtr) (cmdBuffPtr + CMD_HDR_SIZE)
#define comm_get_ans_mask(cmdBuffPtr) ((uint8_t)(*cmdBuffPtr & ANS_ALL_MASK))

// return 0 - transmission error, >0 - payload size + CRC size
static uint8_t comm_header_decode(const uint8_t * const cmdBuff)
{
  //decode and check remaining message size
  uint8_t remSz = *cmdBuff & CMD_SIZE_MASK;
  if(remSz<CMD_MIN_REMSZ||remSz>CMD_MAX_REMSZ)
    return 0;
  //check header against supported commands list
  uint8_t ans=comm_get_ans_mask(cmdBuff);
  switch(ans)
  {
    case ANS_CARD_ABSENT:
    case ANS_CARD_PRESENT:
    case ANS_CARD_INACTIVE:
    case ANS_CARD_EOD:
    case ANS_OK:
      if(remSz>CMD_MIN_REMSZ)
        return 0;
      break;
    case ANS_CARD_DATA:
      if(remSz==CMD_MIN_REMSZ)
        return 0;
      break;
    case ANS_RESYNC:
      break;
    default:
      return 0;
  }
  return remSz;
}

// return 0 - verification error, 1 - ok
static uint8_t comm_verify(const uint8_t * const cmdBuff, const uint8_t cmdSize )
{
  if(cmdSize<(CMD_HDR_SIZE+CMD_CRC_SIZE))
    return 0;
#ifdef CMD_CRC_SIZE_IS_1
  if(*(cmdBuff+cmdSize-1)!=CRC8(cmdBuff,(uint8_t)(cmdSize-1)))
    return 0;
#else
#error unsupported CMD_CRC_SIZE
#endif
  return 1;
}

static uint8_t comm_message(uint8_t * const cmdBuff, const uint8_t cmdMask, const uint8_t * const payload, const uint8_t plLen)
{
  if(plLen>CMD_MAX_PLSZ)
    return 0;
  //place data to cmdBuff, payload buffer may overlap with cmdBuff
  if(payload!=cmdBuff+CMD_HDR_SIZE)
  {
    if(payload>cmdBuff+CMD_HDR_SIZE)
      for(uint8_t i=0; i<plLen; ++i)
        *(cmdBuff+CMD_HDR_SIZE+i)=*(payload+i);
    else
      for(uint8_t i=plLen; i>0; --i)
        *(cmdBuff+CMD_HDR_SIZE+i-1)=*(payload+i-1);
  }
  //write header
#ifdef CMD_HDR_SIZE_IS_1
  *cmdBuff=(uint8_t)(cmdMask|(plLen+CMD_CRC_SIZE));
#else
#error unsupporned CMD_HDR_SIZE
#endif
  //write crc
  uint8_t cmdLen=(uint8_t)(plLen+CMD_HDR_SIZE);
  *(cmdBuff+cmdLen)=CRC8(cmdBuff,cmdLen);
  return (uint8_t)(cmdLen+CMD_CRC_SIZE);
}

// 0 - timeout, or other error, >0 - actual bytes read
static uint8_t comm_recv(ifd_device_t * const dev, uint8_t * const buffer, const uint8_t len)
{
    uint8_t i;
    for (i = 0; i < len; i++)
    {
        int n = ifd_device_recv(dev, buffer+i, 1, CMD_TIMEOUT);
        if (n == IFD_ERROR_TIMEOUT)
            break;
        if (n == -1)
            return 0;
    }
    ifd_debug(3, "comm_recv: %s", ct_hexdump(buffer, len));
    return i;
}

// 0 - timeout, >0 - actual bytes written
static uint8_t comm_send(ifd_device_t * const dev, const uint8_t * const buffer, const uint8_t len)
{
    ifd_debug(3, "cardemu_send: %s", ct_hexdump(buffer, len));
    uint8_t i;
    for (i = 0; i<len; i++)
    {
        if (write(dev->fd,buffer+i,1) < 1)
            return i;
        tcdrain(dev->fd);
    }
    return i;
}

//0 error, 1 - ok
uint8_t resync(ifd_device_t * const dev)
{
    uint8_t resyncBuff[CMD_BUFF_SIZE];
    for(int i=0; i<MAX_RESYNC_TRIES; ++i)
    {
        //send empty resync command
        uint8_t msgLen=comm_message(resyncBuff,REQ_RESYNC,resyncBuff+CMD_HDR_SIZE,0);
        if(!comm_send(dev,resyncBuff,msgLen))
        {
            ifd_debug(3, "resync: initial REQ_RESYNC send failed");
            continue;
        }
        //read data until timeout
        int g;
        for(g=0; g<MAX_RESYNC_GARBAGE; ++g)
            if(!comm_recv(dev,resyncBuff,1))
                break;
        if(g>=MAX_RESYNC_GARBAGE)
        {
            ifd_debug(3, "resync: invalid response (1)");
            continue;
        }
        //send empty resync command
        msgLen=comm_message(resyncBuff,REQ_RESYNC,resyncBuff+CMD_HDR_SIZE,0);
        if(!comm_send(dev,resyncBuff,msgLen))
        {
            ifd_debug(3, "resync: second REQ_RESYNC send failed");
            continue;
        }
        //read empty resync response
        if(!comm_recv(dev,resyncBuff,CMD_HDR_SIZE))
        {
            ifd_debug(3, "resync: invalid response (2,HDR)");
            continue;
        }
        uint8_t remLen=comm_header_decode(resyncBuff);
        if(!remLen)
        {
            ifd_debug(3, "resync: invalid response (2,HDRDECODE)");
            continue;
        }
        if(comm_get_ans_mask(resyncBuff)!=ANS_RESYNC)
        {
            ifd_debug(3, "resync: invalid response (2,HDRTYPE)");
            continue;
        }
        if(remLen!=CMD_CRC_SIZE)
        {
            ifd_debug(3, "resync: invalid response (2,REMLEN)");
            continue;
        }
        uint8_t rem=remLen;
        while(rem>0)
        {
            uint8_t dr=comm_recv(dev,resyncBuff+CMD_HDR_SIZE+(remLen-rem),rem);
            if(!dr)
                break;
            rem-=dr;
        }
        if(rem>0)
        {
            ifd_debug(3, "resync: invalid response (2,REMDATA)");
            continue;
        }
        if(!comm_verify(resyncBuff,remLen+CMD_HDR_SIZE))
        {
            ifd_debug(3, "resync: invalid response (2,CRC)");
            continue;
        }
        //generate control-sequence
        for(int h=0;h<CMD_MAX_PLSZ;++h)
            *(resyncBuff+CMD_HDR_SIZE+h)=(uint8_t)(rand() % 256);
        msgLen=comm_message(resyncBuff,REQ_RESYNC,resyncBuff+CMD_HDR_SIZE,CMD_MAX_PLSZ);
        //send resync command with control sequence
        rem=msgLen;
        while(rem>0)
        {
            uint8_t dw=comm_send(dev,resyncBuff+CMD_HDR_SIZE+(msgLen-rem),rem);
            if(!dw)
                break;
            rem-=dw;
        }
        if(rem>0)
        {
            ifd_debug(3, "resync: failed to send final resync sequence");
            continue;
        }
        //read back resync-answer with control sequence
        uint8_t ansBuff[CMD_BUFF_SIZE];
        if(!comm_recv(dev,ansBuff,CMD_HDR_SIZE))
        {
            ifd_debug(3, "resync: invalid response (3,HDR)");
            continue;
        }
        remLen=comm_header_decode(ansBuff);
        if(!remLen)
        {
            ifd_debug(3, "resync: invalid response (3,HDRDECODE)");
            continue;
        }
        if(comm_get_ans_mask(ansBuff)!=ANS_RESYNC)
        {
            ifd_debug(3, "resync: invalid response (3,HDRTYPE)");
            continue;
        }
        if(remLen!=CMD_MAX_REMSZ)
        {
            ifd_debug(3, "resync: invalid response (3,REMLEN)");
            continue;
        }
        rem=remLen;
        while(rem>0)
        {
            uint8_t dr=comm_recv(dev,ansBuff+CMD_HDR_SIZE+(remLen-rem),rem);
            if(!dr)
                break;
            rem-=dr;
        }
        if(rem>0)
        {
            ifd_debug(3, "resync: invalid response (3,REMDATA)");
            continue;
        }
        if(!comm_verify(ansBuff,remLen+CMD_HDR_SIZE))
        {
            ifd_debug(3, "resync: invalid response (3,CRC)");
            continue;
        }
        //compare answer
        for(g=0;g<CMD_MAX_PLSZ;++g)
            if(*(ansBuff+CMD_HDR_SIZE+g)!=*(resyncBuff+CMD_HDR_SIZE+g))
                break;
        if(g<CMD_MAX_PLSZ)
        {
            ifd_debug(3, "resync: invalid response (3,SEQUENCE)");
            continue;
        }
        //send RESYNC_COMPLETE
        msgLen=comm_message(resyncBuff,REQ_RESYNC_COMPLETE,resyncBuff+CMD_HDR_SIZE,0);
        if(!comm_send(dev,resyncBuff,msgLen))
        {
            ifd_debug(3, "resync: failed to send REQ_RESYNC_COMPLETE");
            continue;
        }
        return 1;
    }
    ifd_debug(3, "resync failed!");
    return 0;
}

static int cardemu_open(ifd_reader_t* reader, const char *device_name)
{
    ifd_debug(1, "cardemu_open: device=%s", device_name);
    reader->name = "OpenPGPCardEmu reader";
    ifd_device_params_t params;
    ifd_device_t *dev;
    reader->nslots = 1;
    if (!(dev = ifd_device_open(device_name)))
    {
        ifd_debug(3, "ifd_device_open failed");
        return -1;
    }
    reader->device = dev;
    if (dev->type == IFD_DEVICE_TYPE_SERIAL)
    {
        if (ifd_device_get_parameters(dev, &params) < 0)
        {
            ifd_debug(3, "ifd_device_get_parameters failed");
            return -1;
        }
        params.serial.speed = 250000;
        params.serial.bits = 8;
        params.serial.stopbits = 1;
        params.serial.parity = IFD_SERIAL_PARITY_NONE;
        if (ifd_device_set_parameters(dev, &params) < 0)
        {
            ifd_debug(3, "ifd_device_set_parameters failed");
            return -1;
        }
    }
    else
    {
        ifd_debug(3, "cardemu_open failed, dev->type != IFD_DEVICE_TYPE_SERIAL ");
        return -1;
    }
    dev->user_data = NULL;
    dev->timeout = CMD_TIMEOUT;
    //perform resync
    if(!resync(dev))
    {
        ifd_debug(3, "cardemu_open: resync failed!");
        return -1;
    }
    return 0;
}


















static int cardemu_setctrl(ifd_device_t *dev, const int ctrl)
{
    int tmp;
    if (ioctl(dev->fd, TIOCMGET, &tmp) == -1)
        return -1;
    tmp &= ~(TIOCM_RTS | TIOCM_CTS | TIOCM_DTR);
    tmp |= ctrl;
    return (ioctl(dev->fd, TIOCMSET, &tmp));
}

static int cardemu_card_status(ifd_reader_t * reader, int slot, int *status)
{
    ifd_device_t *dev = reader->device;
    int tmp;
    if (slot)
    {
        ct_error("cardemu: bad slot index %u", slot);
        return IFD_ERROR_INVALID_SLOT;
    }
    tcflush(dev->fd, TCIOFLUSH);
    if (ioctl(dev->fd, TIOCMGET, &tmp) < 0)
    {
        ifd_debug(3, "cardemu_card_status: TIOCMGET ioctl failed");
        return -1;
    }
    *status = 0;
    *status |= ((tmp & TIOCM_CTS) != TIOCM_CTS) ? IFD_CARD_PRESENT : 0;
    return 0;
}

static int cardemu_deactivate(ifd_reader_t * reader)
{
    ifd_device_t *dev = reader->device;
    tcflush(dev->fd, TCIOFLUSH);
    if (cardemu_setctrl(dev, TIOCM_CTS))
    {
        ifd_debug(3, "cardemu_deactivate: cardemu_setctrl failed");
        return -1;
    }
    return 0;
}

static int cardemu_card_reset(ifd_reader_t *reader, int slot, void *atr, size_t size)
{
    ifd_device_t *dev = reader->device;
    int res;
    if (slot)
    {
        ct_error("cardemu: bad slot index %u", slot);
        return IFD_ERROR_INVALID_SLOT;
    }
    tcflush(dev->fd, TCIOFLUSH);
    if (cardemu_setctrl(dev, TIOCM_CTS | TIOCM_DTR) < 0)
    {
        ifd_debug(3, "cardemu_card_reset: cardemu_setctrl #1 failed");
        return -1;
    }
    if (cardemu_setctrl(dev, TIOCM_RTS | TIOCM_CTS | TIOCM_DTR) < 0)
    {
        ifd_debug(3, "cardemu_card_reset: cardemu_setctrl #2 failed");
        return -1;
    }
    if ((res = cardemu_recv(reader, 0,(unsigned char *)atr, size, dev->timeout)) < 1)
    {
        ifd_debug(3, "cardemu_recv: atr recv failed");
        return -1;
    }
    ifd_debug(1, "Bytes received %i\n", res);
    return res;
}

static int cardemu_change_parity(ifd_reader_t *reader, int parity)
{
    ifd_device_t *dev = reader->device;
    ifd_device_params_t params;
    if (dev->type != IFD_DEVICE_TYPE_SERIAL)
        return IFD_ERROR_NOT_SUPPORTED;
    if (ifd_device_get_parameters(dev, &params) < 0)
    {
        ifd_debug(3, "cardemu_change_parity: ifd_device_get_parameters failed");
        return -1;
    }
    params.serial.parity = parity;
    return ifd_device_set_parameters(dev, &params);
}

static int cardemu_activate(ifd_reader_t* reader)
{
    ifd_device_t *dev = reader->device;
    int tmp;
    uint8_t mode;
    if (cardemu_card_reset(reader, 0, &mode, 1) < 0)
        return -1;
    ifd_debug(1, "Mode received: 0x%x\n", mode);
    switch (mode)
    {
    case 0x3B:
        tmp = IFD_SERIAL_PARITY_EVEN;
        break;
    default:
        ifd_debug(3, "cardemu_activate: incorrect mode received: 0x%x\n", mode);
        return -1;
    }
    cardemu_change_parity(reader, tmp);
    return 0;
}



static int cardemu_recv(ifd_reader_t* reader, unsigned int dad, unsigned char *buffer, size_t len, long timeout)
{
    ifd_device_t* dev = reader->device;
    int i;
    for (i = 0; i < len; i++)
    {
        int n = ifd_device_recv(dev, buffer + i, 1, timeout);
        if (n == IFD_ERROR_TIMEOUT)
            break;
        if (n == -1)
            return -1;
    }
    ifd_debug(3, "data:%s", ct_hexdump(buffer, len));
    return i;
}

static int cardemu_send(ifd_reader_t* reader, unsigned int dad, const unsigned char *buffer, size_t len)
{
    ifd_device_t *dev = reader->device;
    if (!dev)
        return -1;
    ifd_debug(3, "data:%s", ct_hexdump(buffer, len));
    for (size_t i = 0; i < len; i++)
    {
        if (write(dev->fd, buffer + i, 1) < 1)
            return -1;
        tcdrain(dev->fd);
    }
    unsigned char tmp;
    struct pollfd pfd;
    for (size_t i = 0; i < len; i++)
    {
        pfd.fd = dev->fd;
        pfd.events = POLLIN;
        if (poll(&pfd, 1, dev->timeout) < 1)
        {
            ifd_debug(3, "cardemu_send->timeout");
            return -1;
        }
        if (read(dev->fd, &tmp, 1) < 1)
        {
            ifd_debug(3, "cardemu_send->read<1");
            return -1;
        }
        if (tmp != *(buffer + i))
        {
            ifd_debug(3, "cardemu_send->data mismatch at pos %d",i);
            return -1;
        }
    }
    return 0;
}

void ifd_cardemu_register(void)
{
    cardemu_driver.open = cardemu_open;
    cardemu_driver.activate = cardemu_activate;
    cardemu_driver.deactivate = cardemu_deactivate;
    cardemu_driver.card_status = cardemu_card_status;
    cardemu_driver.card_reset = cardemu_card_reset;
    cardemu_driver.send = cardemu_send;
    cardemu_driver.recv = cardemu_recv;
    ifd_driver_register("cardemu", &cardemu_driver);
}
