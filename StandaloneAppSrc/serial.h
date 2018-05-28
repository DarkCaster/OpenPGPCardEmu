#ifndef SERIAL_H_INCLUDED
#define SERIAL_H_INCLUDED

#if BUILD_PLATFORM==WINDOWS
    #define COMPORTNAME "\\\\.\\COM2"
    #include<windows.h>
#else
    #error platform is not supported for now!
#endif

#include <cstddef>
#include <cstdint>

class SerialDummy
{
private:
#if BUILD_PLATFORM==WINDOWS
    HANDLE hComm;
    DCB dcbSerialParams;
#endif
public:
    void begin(unsigned long baud);
    void end();
    int available(void);
    int peek(void);
    int read(void);
    int availableForWrite(void);
    void flush(void);
    size_t write(uint8_t);
    size_t write(unsigned long n) { return write((uint8_t)n); }
    size_t write(long n) { return write((uint8_t)n); }
    size_t write(unsigned int n) { return write((uint8_t)n); }
    size_t write(int n) { return write((uint8_t)n); }
    operator bool() { return true; }
};

extern SerialDummy Serial;

#endif
