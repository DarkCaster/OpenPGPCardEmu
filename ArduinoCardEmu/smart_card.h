#ifndef SMART_CARD_H
#define SMART_CARD_H

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
    ATR GetAtr();
    void Reset();
};

#endif
