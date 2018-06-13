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
#include "smart_card.h"

#define LED_SYNC LED_BUILTIN

static uint8_t status = 0;
static CommHelper commHelper(&Serial);
static SmartCard smartCard;

#define IS_CARD_PRESENT (status&0x01_u8)
#define SET_CARD_STATUS(present) ({status = present ? status|0x01_u8 : status&0xFE_u8;})

void send_resync()
{
  LOG("send_resync: sending resync-pending notification");
  commHelper.SendAnswer(AnsType::Resync,NULL,0);
  return;
}

void resync()
{
  LOG("Starting resync");
  send_resync();
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
  resync();
}

//TODO
uint8_t cardReset()
{
  smartCard.Reset();
  auto atr=smartCard.GetAtr();
  uint8_t answerResult=1;
  uint8_t rem=atr.len;
  //TODO: encode atr length ans send it as separate answer
  //send ATR as multiple messages
  while(rem>0)
  {
    uint8_t partLen=CMD_MAX_PLSZ<rem?CMD_MAX_PLSZ:rem;
    answerResult=answerResult & commHelper.SendAnswer(AnsType::CardPresent,(atr.atr+(atr.len-rem)),rem);
    rem=(uint8_t)(rem-partLen);
  }
  //send OK
  answerResult=answerResult & commHelper.SendAnswer(AnsType::Ok,NULL,0);
  return answerResult;
}

void loop()
{
  //read request
  auto request=commHelper.ReceiveRequest();
  uint8_t answerResult=1;
  //perform action
  switch (request.reqType)
  {
    case ReqType::CardStatus:
      //TODO: detect action: status query, card reset or deactivate
      //TODO: perform action
      //SET_CARD_STATUS(0);
      //SET_CARD_STATUS(1);
      //return answer
      answerResult=commHelper.SendAnswer(IS_CARD_PRESENT?AnsType::CardPresent:AnsType::CardAbsent,NULL,0);
      break;
    case ReqType::CardCommit:
      smartCard.inBuffer.WriteCommit();
      //may take a LONG TIME while performing requested operation
      smartCard.Commit();
      //send Ok answer
      answerResult=commHelper.SendAnswer(AnsType::Ok,NULL,0);
      break;
    case ReqType::CardReadComplete:
      //trim data from buffer that was succsesfully read before
      smartCard.outBuffer.ReadTrim();
      //send Ok answer
      answerResult=commHelper.SendAnswer(AnsType::Ok,NULL,0);
      break;
    case ReqType::CardRead:
      //rewind outgoing buffer in order to recover from previous read-error (if any)
      smartCard.outBuffer.ReadRewind();
      //TODO: get outgoing-buffer data-size and compare it with requested data-size
      //TODO: read data from buffer and send it
      //send EOD
      answerResult=commHelper.SendAnswer(AnsType::CardEOD,NULL,0);
      break;
    case ReqType::CardSend:
      //TODO: write incoming data to card's inBuffer
      //send Ok answer
      answerResult=commHelper.SendAnswer(AnsType::Ok,NULL,0);
      break;
    default:
      LOG("Incorrect request type");
    case ReqType::Invalid:
    case ReqType::Resync:
    case ReqType::ResyncComplete:
      answerResult=0;
      break;
  }
  if(!answerResult)
  {
    smartCard.inBuffer.WriteRewind(); // to recover from possible write-error
    resync();
    return;
  }
}


