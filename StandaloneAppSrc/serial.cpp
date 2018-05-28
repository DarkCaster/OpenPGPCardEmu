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
    dcbSerialParams.BaudRate = baud;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity   = NOPARITY;
    SetCommState(hComm, &dcbSerialParams);
}

void SerialDummy::end()
{
    CloseHandle(hComm);
}

#else
    #error platform is not supported for now!
#endif
