#ifdef STANDALONE_APP

#include "arduino-defines.h"
#include <cstdio>
#include <cstdint>
#define LOG(...) ({printf(__VA_ARGS__); fflush(stdout);})
//#define LOG(format,...) ({})

#else

#include <Arduino.h>
#define LOG(...) ({})

#endif

#include "comm_helper.h"
#include "crc8.h"

int8_t comm_header_decode(const uint8_t * const cmdBuff)
{
  //decode and check remaining message size
  int8_t remSz = (*cmdBuff) & CMD_SIZE_MASK;
  if(remSz<CMD_MIN_REMSZ||remSz>CMD_MAX_REMSZ)
    return 0;
  //check header against supported commands list
  uint8_t req=*cmdBuff & REQ_ALL_MASK;
  switch(req)
  {
    case REQ_CARD_STATUS:
    case REQ_CARD_RESET:
    case REQ_CARD_DEACTIVATE:
      if(remSz>CMD_MIN_REMSZ)
      {
        LOG("comm_header_decode: invalid remaining data size for request: 0x%02X)\n",req);
        return 0;
      }
      break;
    case REQ_CARD_SEND:
    case REQ_CARD_RESPOND:
    case REQ_RESYNC:
      if(remSz==CMD_MIN_REMSZ)
      {
        LOG("comm_header_decode: zero data size for send\respond\resync request: 0x%02X)\n",req);
        return 0;
      }
      break;
    default:
      LOG("comm_header_decode: invalid request received: 0x%02X)\n",req);
      return 0;
  }
  return remSz;
}

int8_t comm_verify(const uint8_t * const cmdBuff, const int8_t cmdSize )
{
  if(cmdSize<1)
    return 0;
  if(*(cmdBuff+cmdSize-1)!=CRC8(cmdBuff,(uint8_t)(cmdSize-1)))
    return 0;
  return 1;
}
