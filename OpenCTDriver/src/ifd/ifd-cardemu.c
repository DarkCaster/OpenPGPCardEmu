#include "internal.h"

//#include <errno.h>
#include <unistd.h>
//#include <stdio.h>
//#include <string.h>
//#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <sys/poll.h>

#define TIMEOUT	1000

static struct ifd_driver_ops cardemu_driver = { 0 };
static int cardemu_recv(ifd_reader_t* reader, unsigned int dad, unsigned char *buffer, size_t len, long timeout);

static int cardemu_setctrl(ifd_device_t *dev, const int ctrl)
{
  int tmp;
  if (ioctl(dev->fd, TIOCMGET, &tmp) == -1)
    return -1;
  tmp &= ~(TIOCM_RTS | TIOCM_CTS | TIOCM_DTR);
  tmp |= ctrl;
  return (ioctl(dev->fd, TIOCMSET, &tmp));
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
        ifd_debug(3, "cardemu_setctrl #1 failed");
        return -1;
    }
    if (cardemu_setctrl(dev, TIOCM_RTS | TIOCM_CTS | TIOCM_DTR) < 0)
    {
        ifd_debug(3, "cardemu_setctrl #2 failed");
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
    //cardemu_driver.activate = cardemu_activate;
    //cardemu_driver.deactivate = cardemu_deactivate;
    //cardemu_driver.card_status = cardemu_card_status;
    cardemu_driver.card_reset = cardemu_card_reset;
    cardemu_driver.send = cardemu_send;
    cardemu_driver.recv = cardemu_recv;
    ifd_driver_register("cardemu", &cardemu_driver);
}
