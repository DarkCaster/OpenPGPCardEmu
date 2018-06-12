#ifndef SMART_CARD_H
#define SMART_CARD_H

struct ATR
{
  public:
    static const uint8_t len=21;
    uint8_t atr[len];
    ATR();
};

#endif
