#ifndef VALUE_ENCODER_H
#define VALUE_ENCODER_H

class ValueEncoder
{
  public:
    static uint8_t EncodeValue(int16_t value, uint8_t * const target);
    static uint8_t EncodeValue(uint16_t value, uint8_t * const target);
    static uint8_t EncodeValue(int32_t value, uint8_t * const target);
    static uint8_t EncodeValue(uint32_t value, uint8_t * const target);

    static int16_t DecodeINT16T(const uint8_t * const source);
    static uint16_t DecodeUINT16T(const uint8_t * const source);
    static int32_t DecodeINT32T(const uint8_t * const source);
    static uint32_t DecodeUINT32T(const uint8_t * const source);
};

#endif
