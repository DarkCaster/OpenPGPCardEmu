#ifndef COMM_HELPER
#define COMM_HELPER

static constexpr uint8_t operator "" _u8 (unsigned long long arg) noexcept
{
  return static_cast<uint8_t>(arg);
}

// command buffer sizes
#define CMD_HDR_SIZE 1
#define CMD_CRC_SIZE 1
#define CMD_BUFF_SIZE 16 //1 byte - header, up to 14 bytes - payload, 1 byte - crc8 checksum
#define CMD_TIMEOUT 2000 //timeout for reading command payload

#define CMD_SIZE_MASK 0x1F_u8
#define CMD_MAX_REMSZ 15
#define CMD_MIN_REMSZ 1

// requests (masks)
#define REQ_ALL_MASK 0xE0_u8
#define REQ_CARD_STATUS 0x20_u8
#define REQ_CARD_RESET 0x40_u8
#define REQ_CARD_DEACTIVATE 0x60_u8
#define REQ_CARD_SEND 0x80_u8
#define REQ_CARD_RESPOND 0xA0_u8
#define REQ_RESYNC_COMPLETE 0xC0_u8
#define REQ_RESYNC 0xE0_u8

// answers (masks)
#define ANS_ALL_MASK 0xE0_u8
#define ANS_CARD_ABSENT 0x20_u8
#define ANS_CARD_PRESENT 0x40_u8
#define ANS_CARD_INACTIVE 0x60_u8
#define ANS_CARD_DATA 0x80_u8
#define ANS_CARD_EOD 0xA0_u8
#define ANS_OK 0xC0_u8
#define ANS_RESYNC 0xE0_u8

// payload size
#define comm_get_payload_size(totalSize) (totalSize>0 ? totalSize-CMD_CRC_SIZE : totalSize)
#define comm_get_payload(cmdBuffPtr) (cmdBuffPtr+CMD_HDR_SIZE)

// return 0 - transmission error, >0 - payload size + CRC size
uint8_t comm_header_decode(const uint8_t * const cmdBuff);

// return 0 - verification error, 1 - ok
uint8_t comm_verify(const uint8_t * const cmdBuff, const int8_t cmdSize );

#endif