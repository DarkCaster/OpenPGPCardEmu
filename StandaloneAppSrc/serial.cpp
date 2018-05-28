#include "serial.h"
#include <cstdio>

SerialDummy Serial;

#if BUILD_PLATFORM==WINDOWS

void SerialDummy::begin(unsigned long baud)
{
    hComm = CreateFile(COMPORTNAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hComm == INVALID_HANDLE_VALUE)
    {
        printf("Error in opening serial port\n");
        return;
    }
    printf("Serial port opened\n");
    fflush(stdout);
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if(!GetCommState(hComm, &dcbSerialParams))
    {
        printf("Error getting comm state: %lu\n",GetLastError());
        return;
    }
    dcbSerialParams.BaudRate = baud;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.Parity   = NOPARITY;
    dcbSerialParams.StopBits = ONESTOPBIT;
    if(!SetCommState(hComm, &dcbSerialParams))
    {
        printf("Error setting comm state: %lu\n",GetLastError());
        return;
    }
    if(GetCommTimeouts(hComm, &commTimeout))
    {
        commTimeout.ReadIntervalTimeout = MAXDWORD;
        commTimeout.ReadTotalTimeoutConstant = 0;
        commTimeout.ReadTotalTimeoutMultiplier = 0;
        commTimeout.WriteTotalTimeoutConstant = 0;
        commTimeout.WriteTotalTimeoutMultiplier = 0;
    }
    else
    {
        printf("Error getting timeouts\n");
        return;
    }
    if(!SetCommTimeouts(hComm, &commTimeout))
    {
        printf("Error setting timeouts\n");
        return;
    }
    printf("Serial port config ok\n");
}

void SerialDummy::end()
{
    CloseHandle(hComm);
}

int SerialDummy::read(void)
{
    BYTE rx;
    DWORD dwBytesTransferred=0;
    if(ReadFile(hComm, &rx,1, &dwBytesTransferred, 0) && dwBytesTransferred ==1)
        return rx;
    return -1;
}

size_t SerialDummy::readBytes(char *buffer, size_t length)
{
    if(length<=0)
        return 0;
    int readCNT=0;
    int rx=read();
    while(rx>0 && length>0)
    {
        *(buffer+readCNT)=(BYTE)rx;
        readCNT++;
        length--;
        if(length>0)
            rx=read();
    }
    return readCNT;
}

int SerialDummy::available(void)
{
    COMSTAT comStat;
    DWORD   dwErrors;
    // Get and clear current errors on the port.
    if (!ClearCommError(hComm, &dwErrors, &comStat))
        return -1;
    if (comStat.cbInQue)
        return comStat.cbInQue;
    return 0;
}

#else
    #error platform is not supported for now!
#endif
