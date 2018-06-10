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

static int8_t status = 0;
static CommHelper commHelper(Serial);

#define READ_REMAINING(remLen,timeout) \
({\
  auto startTime=millis();\
  auto rem=remLen;\
  while(rem>0)\
  {\
    rem-=static_cast<decltype(rem)>(Serial.readBytes(commBuffer+CMD_HDR_SIZE+(remLen-rem),rem));\
    if(millis()-startTime>CMD_TIMEOUT)\
    {\
      LOG("READ_REMAINING: timeout\n");\
      timeout=1;\
      break;\
    }\
  }\
})

void send_resync()
{
  LOG("send_resync: resync pending after serial protocol error\n");
  commHelper.SendAnswer(AnsType::Resync,NULL,0);
  status=-1;
  return;
}

void resync()
{
  if(status>0)
    return;
  if(status==0)
    status--;
  //read request
  auto request=commHelper.ReceiveRequest();
  LOG("resync in progress");
  //check header
  if(request.reqType==ReqType::Invalid)
  {
    send_resync();
    return;
  }
  if(status==-2)
  {
    //if request is ResyncComplete
    if(request.reqType==ReqType::ResyncComplete)
    {
      //send ANS_OK
      if(!commHelper.SendAnswer(AnsType::Ok,NULL,0))
      {
        send_resync();
        return;
      }
      //resync complete!
      status=1;
      LOG("resync complete!");
      return;
    }
    else
      status=-1;
  }
  if(status==-1)
  {
    //if header is not RESYNC, send ANS_RESYNC with 0 bytes payload, return
    if(request.reqType==ReqType::Resync)
    {
      send_resync();
      return;
    }
    //reply with current request's payload
    if(!commHelper.SendAnswer(AnsType::Resync,request.payload,request.plLen))
    {
      send_resync();
      return;
    }
    status--;
  }
  else
    send_resync();
}

void setup()
{
  commHelper.Init(250000);
}

void loop()
{
  //try to perform resync
  resync();
  //TODO: read request
  //TODO: perform action
  //TODO: send response
}


