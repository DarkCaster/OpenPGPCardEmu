#include "main_loop.h"

#ifdef STANDALONE_APP

#include "standalone_config.h"
#include "arduino-defines.h"
#include "serial.h"
#include <cstdio>
#define LOG(...) ({printf(__VA_ARGS__); printf("\n"); fflush(stdout);})
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

static uint8_t status = 0;
static CommHelper commHelper(&Serial);

void send_resync()
{
  LOG("send_resync: sending resync-pending notification after protocol error");
  commHelper.SendAnswer(AnsType::Resync,NULL,0);
  return;
}

void resync()
{
  LOG("Starting resync");
  uint8_t resyncState=0;
  while(true)
  {
    SYNC_ERR();
    //read request
    auto request=commHelper.ReceiveRequest();
    LOG("Resync in progress");
    //check header
    if(request.reqType==ReqType::Invalid)
    {
      LOG("resync: invalid request received");
      send_resync();
      resyncState=0;
      continue;
    }
    if(resyncState==1)
    {
      //if request is ResyncComplete
      if(request.reqType==ReqType::ResyncComplete)
      {
        //send ANS_OK
        if(!commHelper.SendAnswer(AnsType::Ok,NULL,0))
        {
          LOG("resync: failed to send Ok answer to ResyncComplete request");
          send_resync();
          resyncState=0;
          continue;
        }
        //resync complete!
        LOG("Resync complete!");
        SYNC_OK();
        return;
      }
      resyncState=0;
    }
    //final resync-sequence
    if(request.reqType!=ReqType::Resync || !commHelper.SendAnswer(AnsType::Resync,request.payload,request.plLen))
    {
      LOG("resync: incorrect request type or final resync-sequence send-failure");
      send_resync();
      continue;
    }
    resyncState=1;
  }
}

void setup()
{
  SYNC_LED_PREP();
  SYNC_ERR();
  commHelper.Init(38400);
}

void loop()
{
  //try to perform resync
  if(!status)
  {
    resync();
    status=1;
  }
  //TODO: read request
  //TODO: perform action
  //TODO: send response
}


