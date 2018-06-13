#ifndef SMART_CARD_H
#define SMART_CARD_H

#include "data_buffer.h"

struct ATR
{
  static const uint8_t len=21;
  uint8_t atr[len];
  ATR();
};

class SmartCard
{
  public:
    SmartCard();
    DataBuffer inBuffer;
    DataBuffer outBuffer;
    ATR GetAtr();
    void Reset();
    void Commit();
};

#endif
