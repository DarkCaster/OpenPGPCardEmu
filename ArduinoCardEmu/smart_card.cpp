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

ATR::ATR()
{
  //TODO: proper atr generation, use OpenPGP v3 for now
  uint8_t atrVal[len]={0x3B, 0xDA, 0x18, 0xFF, 0x81, 0xB1, 0xFE, 0x75, 0x1F, 0x03, 0x00, 0x31, 0xF5, 0x73, 0xC0, 0x01, 0x60, 0x00, 0x90, 0x00, 0x1C};
  for(uint8_t i=0; i<len; ++i)
    atr[i]=atrVal[i];
}

SmartCard::SmartCard()
{

}

ATR SmartCard::GetAtr()
{
  return ATR();
}

void SmartCard::Reset()
{

}
