#include "main_loop.h"
#ifdef STANDALONE_APP
    #include "standalone_config.h"
    #include "serial.h"
    #include <cstdio>
#define LOG(format,...) ({printf(format,__VA_ARGS__); fflush(stdout);})
//#define LOG(format,...) ({})
#else
    #include <Arduino.h>
    #define LOG(format,...) ({})
#endif

void setup() {
    Serial.begin(9600);
}

void loop() {
    int val=Serial.read();
    if(val>=0)
        LOG("COM PORT READ: %d\n", val);
}
