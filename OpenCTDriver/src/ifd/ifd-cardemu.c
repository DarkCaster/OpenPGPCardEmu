#include "internal.h"

//#include <errno.h>
#include <unistd.h>
//#include <stdio.h>
//#include <string.h>
//#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <sys/poll.h>

static struct ifd_driver_ops cardemu_driver = { 0 };

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
    //cardemu_driver.open = cardemu_open;
    //cardemu_driver.activate = cardemu_activate;
    //cardemu_driver.deactivate = cardemu_deactivate;
    //cardemu_driver.card_status = cardemu_card_status;
    //cardemu_driver.card_reset = cardemu_card_reset;
    cardemu_driver.send = cardemu_send;
    cardemu_driver.recv = cardemu_recv;
    ifd_driver_register("cardemu", &cardemu_driver);
}
