#include "main_loop.h"
#ifdef STANDALONE_APP
    #include "standalone_config.h"
    #include "serial.h"
    #include <cstdio>
#define LOG(...) ({printf(__VA_ARGS__); fflush(stdout);})

//#define LOG(format,...) ({})
#else
    #include <Arduino.h>
    #define LOG(...) ({})
#endif

void setup() {
    Serial.begin(9600);
}

void loop() {
    int val=Serial.read();
    if(val>=0)
    {
        LOG("COM PORT READ: %d\n", val);
        if(!Serial.write(val))
            LOG("COM PORT WRITE FAILED!\n");
    }
}
