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

uint8_t comm_header_decode(const uint8_t * const cmdBuff)
{
  //decode and check remaining message size
  uint8_t remSz = *cmdBuff & CMD_SIZE_MASK;
  if(remSz<CMD_MIN_REMSZ||remSz>CMD_MAX_REMSZ)
  {
    LOG("comm_header_decode: remaining data size is out of bounds\n");
    return 0;
  }
  //check header against supported commands list
  auto req=comm_get_req_mask_M(cmdBuff);
  switch(req)
  {
    case REQ_CARD_STATUS:
    case REQ_CARD_RESET:
    case REQ_CARD_DEACTIVATE:
      if(remSz>CMD_MIN_REMSZ)
      {
        LOG("comm_header_decode: invalid remaining data size for request: 0x%02X\n",req);
        return 0;
      }
      break;
    case REQ_CARD_SEND:
    case REQ_CARD_RESPOND:
      if(remSz==CMD_MIN_REMSZ)
      {
        LOG("comm_header_decode: zero data size for send\respond request: 0x%02X\n",req);
        return 0;
      }
      break;
    case REQ_RESYNC:
    case REQ_RESYNC_COMPLETE:
      break;
    default:
      LOG("comm_header_decode: invalid request received: 0x%02X\n",req);
      return 0;
  }
  return remSz;
}

uint8_t comm_verify(const uint8_t * const cmdBuff, const uint8_t cmdSize )
{
  if(cmdSize<2)
  {
    LOG("comm_verify: invalid cmdSize\n");
    return 0;
  }
  if(*(cmdBuff+cmdSize-1)!=CRC8(cmdBuff,(uint8_t)(cmdSize-1)))
    return 0;
  return 1;
}

uint8_t comm_message(uint8_t * const cmdBuff, const uint8_t cmdMask, const uint8_t * const payload, const uint8_t plLen)
{
  if(plLen>CMD_MAX_PLSZ)
    return 0;
  //place data to cmdBuff, payload buffer may overlap with cmdBuff
  if(payload!=cmdBuff+CMD_HDR_SIZE)
  {
    if(payload>cmdBuff+CMD_HDR_SIZE)
      for(uint8_t i=0; i<plLen; ++i)
        *(cmdBuff+CMD_HDR_SIZE+i)=*(payload+i);
    else
      for(uint8_t i=plLen; i>0; --i)
        *(cmdBuff+CMD_HDR_SIZE+i-1)=*(payload+i-1);
  }
  //write header
#ifdef CMD_HDR_SIZE_IS_1
  *cmdBuff=(uint8_t)(cmdMask|(plLen+CMD_CRC_SIZE));
#else
#error unsupporned CMD_HDR_SIZE
#endif
  //write crc
  auto cmdLen=(uint8_t)(plLen+CMD_HDR_SIZE);
  *(cmdBuff+cmdLen)=CRC8(cmdBuff,cmdLen);
  return (uint8_t)(cmdLen+CMD_CRC_SIZE);
}
