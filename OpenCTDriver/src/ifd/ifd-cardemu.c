#include "internal.h"

#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <sys/poll.h>


static struct ifd_driver_ops cardemu_driver = { 0 };

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
#define MAX_CMD_TRIES 3
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
#define ANS_CARD_DATA 0x80
#define ANS_CARD_EOD 0xA0
#define ANS_OK 0xC0
#define ANS_RESYNC 0xE0

// payload size
#define comm_calc_payload_size(totalSize) ((uint8_t)(totalSize>0 ? totalSize-CMD_CRC_SIZE : totalSize))
#define comm_get_remsize(cmdBuffPtr) ((uint8_t)(*cmdBuffPtr & CMD_SIZE_MASK))
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
    case ANS_CARD_EOD:
    case ANS_OK:
      if(remSz>CMD_MIN_REMSZ)
        return 0;
      break;
    case ANS_CARD_DATA:
      if(remSz==CMD_MIN_REMSZ)
        return 0;
      break;
    case ANS_CARD_PRESENT:
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
    if(i>0)
        ifd_debug(3, "%s", ct_hexdump(buffer, i));
    return i;
}

// 0 in case of error, or len in case of success
static uint8_t comm_send(ifd_device_t * const dev, const uint8_t * const buffer, const uint8_t len)
{
    if(ifd_device_send(dev,buffer,len)!=len)
        return 0;
    ifd_debug(3, "%s", ct_hexdump(buffer, len));
    return len;
}

// 0 - timeout or error, >0 - message length
static uint8_t comm_recv_message(ifd_device_t * const dev, uint8_t * const buffer)
{
    if(!comm_recv(dev,buffer,CMD_HDR_SIZE))
    {
        ifd_debug(3, "failed to read header");
        return 0;
    }
    uint8_t remLen=comm_header_decode(buffer);
    if(!remLen)
    {
        ifd_debug(3, "header decode failed");
        return 0;
    }
    uint8_t rem=remLen;
    while(rem>0)
    {
        uint8_t dr=comm_recv(dev,buffer+CMD_HDR_SIZE+(remLen-rem),rem);
        if(!dr)
            break;
        rem-=dr;
    }
    if(rem>0)
    {
        ifd_debug(3, "message body read failed");
        return 0;
    }
    uint8_t msgLen=remLen+CMD_HDR_SIZE;
    if(!comm_verify(buffer,msgLen))
    {
        ifd_debug(3, "CRC verification failed");
        return 0;
    }
    return msgLen;
}

// 0 - timeout or error, >0 - message length
/*static uint8_t comm_send_message(ifd_device_t * const dev, const uint8_t * const msgBuffer)
{
    return comm_send(dev,msgBuffer,(*msgBuffer & CMD_SIZE_MASK)+CMD_HDR_SIZE);
}*/

//0 error, 1 - ok
uint8_t resync(ifd_device_t * const dev)
{
    ifd_debug(3, "running");
    uint8_t resyncBuff[CMD_BUFF_SIZE];
    for(int i=0; i<MAX_RESYNC_TRIES; ++i)
    {
        //send empty resync command
        uint8_t msgLen=comm_message(resyncBuff,REQ_RESYNC,resyncBuff+CMD_HDR_SIZE,0);
        if(!comm_send(dev,resyncBuff,msgLen))
        {
            ifd_debug(3, "initial REQ_RESYNC send failed");
            continue;
        }
        //read data until timeout
        int g;
        for(g=0; g<MAX_RESYNC_GARBAGE; ++g)
            if(!comm_recv(dev,resyncBuff,1))
                break;
        if(g>=MAX_RESYNC_GARBAGE)
        {
            ifd_debug(3, "invalid response (1)");
            continue;
        }
        //send empty resync command
        msgLen=comm_message(resyncBuff,REQ_RESYNC,resyncBuff+CMD_HDR_SIZE,0);
        if(!comm_send(dev,resyncBuff,msgLen))
        {
            ifd_debug(3, "second REQ_RESYNC send failed");
            continue;
        }
        //read empty resync response
        msgLen=comm_recv_message(dev,resyncBuff);
        if(msgLen!=CMD_CRC_SIZE+CMD_HDR_SIZE)
        {
            ifd_debug(3, "invalid response (2,MSGLEN)");
            continue;
        }
        if(comm_get_ans_mask(resyncBuff)!=ANS_RESYNC)
        {
            ifd_debug(3, "invalid response (2,HDRTYPE)");
            continue;
        }
        //generate control-sequence
        for(int h=0;h<CMD_MAX_PLSZ;++h)
            *(resyncBuff+CMD_HDR_SIZE+h)=(uint8_t)(rand() % 256);
        msgLen=comm_message(resyncBuff,REQ_RESYNC,resyncBuff+CMD_HDR_SIZE,CMD_MAX_PLSZ);
        //send resync command with control sequence
        if(!comm_send(dev,resyncBuff,msgLen))
        {
            ifd_debug(3, "failed to send final resync sequence");
            continue;
        }
        //read back resync-answer with control sequence
        uint8_t ansBuff[CMD_BUFF_SIZE];
        msgLen=comm_recv_message(dev,ansBuff);
        if(msgLen!=CMD_MAX_REMSZ+CMD_HDR_SIZE)
        {
            ifd_debug(3, "invalid response (3,MSGLEN)");
            continue;
        }
        if(comm_get_ans_mask(ansBuff)!=ANS_RESYNC)
        {
            ifd_debug(3, "invalid response (3,HDRTYPE)");
            continue;
        }
        //compare answer
        for(g=0;g<CMD_MAX_PLSZ;++g)
            if(*(ansBuff+CMD_HDR_SIZE+g)!=*(resyncBuff+CMD_HDR_SIZE+g))
                break;
        if(g<CMD_MAX_PLSZ)
        {
            ifd_debug(3, "invalid response (3,SEQUENCE)");
            continue;
        }
        //send RESYNC_COMPLETE
        msgLen=comm_message(resyncBuff,REQ_RESYNC_COMPLETE,resyncBuff+CMD_HDR_SIZE,0);
        if(!comm_send(dev,resyncBuff,msgLen))
        {
            ifd_debug(3, "failed to send REQ_RESYNC_COMPLETE");
            continue;
        }
        //receive confirmation
        msgLen=comm_recv_message(dev,resyncBuff);
        if(msgLen!=CMD_HDR_SIZE+CMD_CRC_SIZE)
        {
            ifd_debug(3, "invalid response (4,MSGLEN)");
            continue;
        }
        if(comm_get_ans_mask(resyncBuff)!=ANS_OK)
        {
            ifd_debug(3, "invalid response (4,HDRTYPE)");
            continue;
        }
        ifd_debug(3, "done");
        return 1;
    }
    ifd_debug(3, "failed!");
    return 0;
}

static int cardemu_open(ifd_reader_t* reader, const char *device_name)
{
    ifd_debug(3, "starting");
    ifd_debug(1, "device=%s", device_name);
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
        params.serial.speed = 38400;
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
    //perform tcflush(int fd, int queue_selector);
    if(tcflush(dev->fd, TCIOFLUSH)!=0)
    {
        ifd_debug(3, "tcflush failed!");
        return -1;
    }
    //reset random generator
    srand((unsigned int)(time(NULL)/2L));
    //perform resync
    if(!resync(dev))
    {
        ifd_debug(3, "resync failed!");
        return -1;
    }
    ifd_debug(3, "done");
    return 0;
}

#define RUN_RESYNC_AND_RETRY() { if (resync(dev)) continue; else break; }

static int cardemu_card_status(ifd_reader_t * reader, int slot, int *status)
{
    ifd_debug(3, "starting");
    ifd_device_t *dev = reader->device;
    if (slot)
    {
        ct_error("cardemu: bad slot index %u", slot);
        return IFD_ERROR_INVALID_SLOT;
    }
    uint8_t buff[CMD_BUFF_SIZE];
    for(int tr=0; tr<MAX_CMD_TRIES; ++tr)
    {
        //send REQ_CARD_STATUS
        uint8_t msgLen=comm_message(buff,REQ_CARD_STATUS,buff+CMD_HDR_SIZE,0);
        if(comm_send(dev,buff,msgLen)!=msgLen)
            RUN_RESYNC_AND_RETRY();
        //receive answer, run resync in case of error
        if(!comm_recv_message(dev,buff))
            RUN_RESYNC_AND_RETRY();
        //check answer, run resync in case of wrong answer
        if(comm_get_ans_mask(buff)!=ANS_CARD_ABSENT && comm_get_ans_mask(buff)!=ANS_CARD_PRESENT)
            RUN_RESYNC_AND_RETRY();
        //write status result
        *status = comm_get_ans_mask(buff)==ANS_CARD_PRESENT ? IFD_CARD_PRESENT : 0;
        ifd_debug(3, "done, status is %d", *status);
        return 0;
    }
    //error getting status
    ct_error("cardemu: failed to get card status");
    return -1;
}

static int cardemu_deactivate(ifd_reader_t * reader)
{
    ifd_debug(3, "starting");
    ifd_device_t *dev = reader->device;
    uint8_t buff[CMD_BUFF_SIZE];
    for(int tr=0; tr<MAX_CMD_TRIES; ++tr)
    {
        //send REQ_CARD_DEACTIVATE
        uint8_t msgLen=comm_message(buff,REQ_CARD_DEACTIVATE,buff+CMD_HDR_SIZE,0);
        if(comm_send(dev,buff,msgLen)!=msgLen)
            RUN_RESYNC_AND_RETRY();
        //receive answer, run resync in case of error
        if(!comm_recv_message(dev,buff))
            RUN_RESYNC_AND_RETRY();
        //check answer, run resync in case of wrong answer
        if(comm_get_ans_mask(buff)!=ANS_OK)
            RUN_RESYNC_AND_RETRY();
        ifd_debug(3, "done");
        return 0;
    }
    //error
    ct_error("cardemu: failed to deactivate card");
    return -1;
}

static int cardemu_card_reset(ifd_reader_t *reader, int slot, void *atr, size_t size)
{
    ifd_debug(3, "starting");
    ifd_device_t *dev = reader->device;
    if (slot)
    {
        ct_error("cardemu: bad slot index %u", slot);
        return IFD_ERROR_INVALID_SLOT;
    }
    uint8_t buff[CMD_BUFF_SIZE];
    for(int tr=0; tr<MAX_CMD_TRIES; ++tr)
    {
        //send REQ_CARD_RESET
        uint8_t msgLen=comm_message(buff,REQ_CARD_RESET,buff+CMD_HDR_SIZE,0);
        if(comm_send(dev,buff,msgLen)!=msgLen)
            RUN_RESYNC_AND_RETRY();
        //receive answer, run resync in case of error
        if(!comm_recv_message(dev,buff))
            RUN_RESYNC_AND_RETRY();
        //check answer, run resync in case of wrong answer
        if(comm_get_ans_mask(buff)!=ANS_CARD_PRESENT)
            RUN_RESYNC_AND_RETRY();
        //copy atr from answer
        uint8_t* pl=comm_get_payload(buff);
        uint8_t plLen=comm_calc_payload_size(comm_get_remsize(buff));
        ifd_debug(3, "atr received: %s", ct_hexdump(pl, plLen));
        uint8_t atrLen=(size>plLen)?plLen:(uint8_t)size;
        for(int i=0;i<atrLen;++i)
            *((uint8_t*)(atr+i))=*(pl+i);
        ifd_debug(3, "done, atr returned: %s", ct_hexdump(atr, atrLen));
        return atrLen;
    }
    ct_error("cardemu: reset failed!");
    return -1;
}

static int cardemu_activate(ifd_reader_t* reader)
{
    ifd_debug(3, "starting");
    uint8_t mode;
    if (cardemu_card_reset(reader, 0, &mode, 1) < 0)
    {
        ct_error("cardemu: activate failed!");
        return -1;
    }
    ifd_debug(3, "done");
    return 0;
}

#define RUN_RESYNC_AND_RETRY_2() { if (resync(dev)) { error=1; continue; } else { error=2; break; } }

static int cardemu_send(ifd_reader_t* reader, unsigned int dad, const unsigned char *buffer, size_t len)
{
    ifd_debug(3, "starting");
    ifd_device_t *dev = reader->device;
    if (!dev)
    {
        ct_error("cardemu: device is not defined!");
        return -1;
    }
    ifd_debug(3, "data:%s", ct_hexdump(buffer, len));
    uint8_t sendBuffer[CMD_BUFF_SIZE];
    while(len>0)
    {
        uint8_t pkgLen=CMD_MAX_PLSZ>len?(uint8_t)len:CMD_MAX_PLSZ;
        uint8_t error=0;
        for(int tr=0; tr<MAX_CMD_TRIES; ++tr)
        {
            //send REQ_CARD_SEND
            uint8_t msgLen=comm_message(sendBuffer,REQ_CARD_SEND,buffer,pkgLen);
            if(comm_send(dev,sendBuffer,msgLen)!=msgLen)
                RUN_RESYNC_AND_RETRY_2();
            //receive answer, run resync in case of error
            if(!comm_recv_message(dev,sendBuffer))
                RUN_RESYNC_AND_RETRY_2();
            //check answer, run resync in case of wrong answer
            if(comm_get_ans_mask(sendBuffer)!=ANS_OK)
                RUN_RESYNC_AND_RETRY_2();
            error=0;
            break;
        }
        if(error)
        {
            ct_error("cardemu: cardemu_send failed!");
            return -1;
        }
        len-=pkgLen;
        buffer+=pkgLen;
    }
    ifd_debug(3, "done");
    return 0;
}

static void encode_uint16_t(uint8_t *buffer, uint16_t value)
{
    *(buffer)=(uint8_t)(value&0xFF);
    *(buffer+1)=(uint8_t)((value>>8)&0xFF);
}

#define RUN_RESYNC_AND_RETRY_3() { if (resync(dev)) { error=1; break; } else { error=2; break; } }

static int cardemu_recv(ifd_reader_t* reader, unsigned int dad, unsigned char *buffer, size_t len, long timeout)
{
    ifd_debug(3, "starting");
    if(len>0xFFFF)
    {
        ct_error("cardemu: cardemu_recv len is too big!");
        return -1;
    }
    ifd_device_t* dev = reader->device;
    uint8_t sendBuffer[CMD_BUFF_SIZE];
    //send data-read-request with length
    for(int tr=0; tr<MAX_CMD_TRIES; ++tr)
    {
        //encode length
        encode_uint16_t(sendBuffer+CMD_HDR_SIZE,len);
        //send REQ_CARD_RESPOND
        uint8_t msgLen=comm_message(sendBuffer,REQ_CARD_RESPOND,sendBuffer+CMD_HDR_SIZE,2);
        if(comm_send(dev,sendBuffer,msgLen)!=msgLen)
            RUN_RESYNC_AND_RETRY();
        size_t dr=0;
        uint8_t* targetBuffer=buffer;
        uint8_t error=0;
        while(dr<=len)
        {
            //receive message with data
            if(!comm_recv_message(dev,sendBuffer))
                RUN_RESYNC_AND_RETRY_3();
            if(comm_get_ans_mask(sendBuffer)!=ANS_CARD_DATA || comm_get_ans_mask(sendBuffer)!=ANS_CARD_EOD)
                RUN_RESYNC_AND_RETRY_3();
            if(dr==len && comm_get_ans_mask(sendBuffer)!=ANS_CARD_EOD)
                RUN_RESYNC_AND_RETRY_3();
            if(comm_get_ans_mask(sendBuffer)==ANS_CARD_EOD)
                break;
            //comm_get_ans_mask(sendBuffer)==ANS_CARD_DATA
            uint8_t* pl=comm_get_payload(sendBuffer);
            uint8_t plLen=comm_calc_payload_size(comm_get_remsize(sendBuffer));
            dr+=plLen;
            while(plLen--)
               *(targetBuffer++)=*(pl++);
        }
        if(error==1)
            continue;
        if(error==2)
            break;
        ifd_debug(3, "done");
        return dr;
    }
    ct_error("cardemu: cardemu_recv failed!");
    return -1;
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
