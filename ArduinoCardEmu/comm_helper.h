#ifndef COMM_HELPER
#define COMM_HELPER

constexpr uint8_t operator "" _u8 (unsigned long long arg) noexcept
{
  return static_cast<uint8_t>(arg);
}

// command buffer sizes
#define CMD_HDR_SIZE 1_u8
#define CMD_HDR_SIZE_IS_1
#define CMD_CRC_SIZE 1_u8
#define CMD_BUFF_SIZE 16 //1 byte - header, up to 14 bytes - payload, 1 byte - crc8 checksum
#define CMD_TIMEOUT 500 //timeout for reading command payload

#define CMD_SIZE_MASK 0x1F_u8
#define CMD_MAX_REMSZ 15_u8
#define CMD_MIN_REMSZ 1_u8
#define CMD_MAX_PLSZ  14_u8

// requests (masks)
#define REQ_ALL_MASK 0xE0_u8
#define REQ_INVALID 0x00_u8
#define REQ_CARD_STATUS 0x20_u8
#define REQ_CARD_RESET 0x40_u8
#define REQ_CARD_DEACTIVATE 0x60_u8
#define REQ_CARD_SEND 0x80_u8
#define REQ_CARD_RESPOND 0xA0_u8
#define REQ_RESYNC_COMPLETE 0xC0_u8
#define REQ_RESYNC 0xE0_u8

// answers (masks)
#define ANS_ALL_MASK 0xE0_u8
#define ANS_INVALID 0x00_u8
#define ANS_CARD_ABSENT 0x20_u8
#define ANS_CARD_PRESENT 0x40_u8
#define ANS_CARD_DATA 0x80_u8
#define ANS_CARD_EOD 0xA0_u8
#define ANS_OK 0xC0_u8
#define ANS_RESYNC 0xE0_u8

// payload size
#define comm_get_payload_size_M(totalSize) ((uint8_t)(totalSize>0 ? totalSize-CMD_CRC_SIZE : totalSize))
#define comm_get_payload_M(cmdBuffPtr) (cmdBuffPtr + CMD_HDR_SIZE)
#define comm_get_req_mask_M(cmdBuffPtr) ((uint8_t)(*cmdBuffPtr & REQ_ALL_MASK))

// return 0 - transmission error, >0 - payload size + CRC size
uint8_t comm_header_decode(const uint8_t * const cmdBuff);

// return 0 - verification error, 1 - ok
uint8_t comm_verify(const uint8_t * const cmdBuff, const uint8_t cmdSize );

uint8_t comm_message(uint8_t * const cmdBuff, const uint8_t cmdMask, const uint8_t * const payload, const uint8_t plLen);





enum class ReqType : uint8_t
{
  Invalid = REQ_INVALID,
  CardStatus = REQ_CARD_STATUS,
  CardReset = REQ_CARD_RESET,
  CardDeactivate = REQ_CARD_DEACTIVATE,
  CardSend = REQ_CARD_SEND,
  CardRespond = REQ_CARD_RESPOND,
  ResyncComplete = REQ_RESYNC_COMPLETE,
  Resync = REQ_RESYNC
};

enum class AnsType : uint8_t
{
  Invalid = ANS_INVALID,
  CardAbsent = ANS_CARD_ABSENT,
  CardPresent = ANS_CARD_PRESENT,
  CardData = ANS_CARD_DATA,
  CardEOD = ANS_CARD_EOD,
  Ok = ANS_OK,
  Resync = ANS_RESYNC
};

struct Request
{
  private:
    Request();
  public:
    static Request Invalid();
    //Request(const uint8_t* const message);
    Request(const uint8_t req, const uint8_t* const payload, const uint8_t plLen);
    ReqType reqType;
    uint8_t plLen;
    uint8_t payload[CMD_MAX_PLSZ];
};

class CommHelper
{
  private:
    HardwareSerial serial;
  public:
    CommHelper(const HardwareSerial port);
    void Init(const long speed);
    void Deinit();
    Request ReceiveRequest();
    uint8_t SendAnswer(const AnsType answer, const uint8_t* const payload, uint8_t plLen);
};

#endif
