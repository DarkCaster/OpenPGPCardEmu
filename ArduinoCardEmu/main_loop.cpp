#include "main_loop.h"

#ifdef STANDALONE_APP

#include "standalone_config.h"
#include "arduino-defines.h"
#include "serial.h"
#include <cstdio>
#define LOG(...) ({printf(__VA_ARGS__); fflush(stdout);})
//#define LOG(format,...) ({})
#define SYNC_OK() ({})
#define SYNC_ERR() ({})
#define SYNC_LED_PREP() ({})

#else

#include <Arduino.h>
#define LOG(...) ({})
#define SYNC_OK() ({digitalWrite(LED_SYNC, HIGH);})
#define SYNC_ERR() ({digitalWrite(LED_SYNC, LOW);})
#define SYNC_LED_PREP() ({pinMode(LED_SYNC, OUTPUT);;})

#endif

#include "comm_helper.h"

#define LED_SYNC LED_BUILTIN

static int8_t status = 0;
static CommHelper commHelper(Serial);

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
  SYNC_ERR();
  //read request
  auto request=commHelper.ReceiveRequest();
  LOG("resync in progress");
  //check header
  if(request.reqType==ReqType::Invalid)
  {
    LOG("resync: invalid request received");
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
        LOG("resync: failed to send Ok answer to ResyncComplete request");
        send_resync();
        return;
      }
      //resync complete!
      status=1;
      LOG("resync complete!");
      SYNC_OK();
      return;
    }
    else
      status=-1;
  }
  if(status==-1)
  {
    //if header is not RESYNC, send ANS_RESYNC with 0 bytes payload, return
    if(request.reqType!=ReqType::Resync)
    {
      LOG("resync: incorrect request type");
      send_resync();
      return;
    }
    //reply with current request's payload
    if(!commHelper.SendAnswer(AnsType::Resync,request.payload,request.plLen))
    {
      LOG("resync: failed to send final resync-sequence");
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
  SYNC_LED_PREP();
  SYNC_ERR();
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


