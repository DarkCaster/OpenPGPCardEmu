#ifdef STANDALONE_APP

#include "arduino-defines.h"
#include <cstdio>
#include <cstdint>
#define LOG(...) ({printf(__VA_ARGS__); fflush(stdout);})

#else

#include <Arduino.h>
#define LOG(...) ({})

#endif

#include "smart_card.h"
