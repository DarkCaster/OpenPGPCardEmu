#include "internal.h"

#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <sys/poll.h>

#define TIMEOUT	1000

static struct ifd_driver_ops cardemu_driver = { 0 };
static int cardemu_recv(ifd_reader_t* reader, unsigned int dad, unsigned char *buffer, size_t len, long timeout);

//fast crc8 algo taken from wikipedia
static const unsigned char Crc8Table[256] = {
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

static unsigned char CRC8(const unsigned char *source, unsigned char len)
{
    unsigned char crc = 0xFF;
    while (len--)
        crc = Crc8Table[crc ^ *source++];
    return crc;
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

static int cardemu_open(ifd_reader_t* reader, const char *device_name)
{
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
        params.serial.speed = 9600;
        params.serial.bits = 8;
        params.serial.stopbits = 1;
        params.serial.parity = IFD_SERIAL_PARITY_NONE;
        params.serial.dtr = 1;
        params.serial.rts = 1;
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
    dev->timeout = TIMEOUT;
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
