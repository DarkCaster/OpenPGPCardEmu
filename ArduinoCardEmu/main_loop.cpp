#include "main_loop.h"

#ifdef STANDALONE_APP

#include "standalone_config.h"
#include "arduino-defines.h"
#include "serial.h"
#include <cstdio>
#define LOG(...) ({printf(__VA_ARGS__); fflush(stdout);})
//#define LOG(format,...) ({})

#else

#include <Arduino.h>
#define LOG(...) ({})

#endif

#include "comm_helper.h"

static uint8_t commBuffer[CMD_BUFF_SIZE];

static int8_t status = 0;

#define READ_REMAINING(remLen,timeout) \
({\
  auto startTime=millis();\
  auto rem=remLen;\
  while(rem>0)\
  {\
    rem-=static_cast<decltype(rem)>(Serial.readBytes(commBuffer+CMD_HDR_SIZE+(remLen-rem),rem));\
    if(millis()-startTime>CMD_TIMEOUT)\
    {\
      timeout=1;\
      break;\
    }\
  }\
})

#if STANDALONE_APP
#pragma GCC diagnostic ignored "-Wconversion"
#endif

void resync()
{
  //TODO: create and send ANS_RESYNC response, status=-1
  #define RESYNC_FAILED() \
  ({\
    auto msgLen=comm_message(commBuffer,ANS_RESYNC,commBuffer+CMD_HDR_SIZE,0);\
    for(uint8_t i=0; i<msgLen; ++i) \
      while(Serial.write(commBuffer[i])<1) {} \
  })
  if(status>0)
    return;
  if(status==0)
    status--;
  //read header
  Serial.readBytes(commBuffer,1);
  //check header
  auto remLen=comm_header_decode(commBuffer);
  if(!remLen)
  {
    RESYNC_FAILED();
    return;
  }
  auto req=comm_get_req_mask(commBuffer);
  if(status==-2)
  {
    //awaiting for resync complete request
    //if request is resync complete
    if(req==REQ_RESYNC_COMPLETE)
    {
      //read remaining data, check for timeout
      uint8_t timeout=0;
      READ_REMAINING(remLen,timeout);
      //verify, if verification failed or timeout fired - send ANS_RESYNC, status==-1, return
      if(timeout||!comm_verify(commBuffer,(uint8_t)(remLen+CMD_HDR_SIZE)))
      {
        RESYNC_FAILED();
        return;
      }
      //send ANS_OK
      auto msgLen=comm_message(commBuffer,ANS_OK,commBuffer+CMD_HDR_SIZE,0);
      for(uint8_t i=0; i<msgLen; ++i)
        while(Serial.write(commBuffer[i])<1) {}
      //resync complete!
      status=1;
      return;
    }
    else
      status=-1;
  }
  if(status==-1)
  {
    //if header is not RESYNC, send ANS_RESYNC with 0 bytes payload, return
    if(req!=REQ_RESYNC)
    {
      RESYNC_FAILED();
      return;
    }
    //read remaining data, check for timeout
    uint8_t timeout=0;
    READ_REMAINING(remLen,timeout);
    //if timeout fired or checksum is invalid - send ANS_RESYNC with 0 bytes payload, return
    if(timeout||!comm_verify(commBuffer,(uint8_t)(remLen+CMD_HDR_SIZE)))
    {
      RESYNC_FAILED();
      return;
    }
    //create resync answer (with checksum)
    auto plLen=comm_get_payload_size(remLen);
    auto payload=comm_get_payload(commBuffer);
    //send answer to serial
    auto msgLen=comm_message(commBuffer,ANS_RESYNC,payload,plLen);
    for(uint8_t i=0; i<msgLen; ++i)
      while(Serial.write(commBuffer[i])<1) {}
    status--;
  }
  else
    //invalid status
    RESYNC_FAILED();
}

#if STANDALONE_APP
#pragma GCC diagnostic pop
#endif

void setup()
{
  Serial.begin(250000);
}

void loop()
{
  if(!Serial.available())
    return;
  //try to perform resync
  resync();
  //TODO: read request
  //TODO: perform action
  //TODO: send response
}


