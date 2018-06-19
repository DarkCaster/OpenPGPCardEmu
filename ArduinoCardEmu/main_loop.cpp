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
static uint16_t writePending = 0;
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

uint8_t card_reset()
{
  smartCard.Reset();
  auto atr=smartCard.GetAtr();
  //encode atr length ans send it as separate answer
  uint8_t atrLen[1]={ atr.len };
  if(!commHelper.SendAnswer(AnsType::CardPresent,atrLen,sizeof(atrLen)))
    return 0;
  //send ATR as multiple messages
  uint8_t rem=atr.len;
  while(rem>0)
  {
    uint8_t partLen=CMD_MAX_PLSZ<rem?CMD_MAX_PLSZ:rem;
    if(!commHelper.SendAnswer(AnsType::CardPresent,(atr.atr+(atr.len-rem)),partLen))
      return 0;
    rem=(uint8_t)(rem-partLen);
  }
  return 1;
}

void loop()
{
  //read request
  auto request=commHelper.ReceiveRequest();
  uint8_t result=1;
  //perform action
  switch (request.reqType)
  {
    case ReqType::CardStatus:
      //detect action: status query, card reset or deactivate
      // status query
      if(request.plLen==0)
        result=commHelper.SendAnswer(IS_CARD_PRESENT?AnsType::CardPresent:AnsType::CardAbsent,NULL,0);
      // card deactivate
      else if (request.payload[0]==0x00_u8)
      {
        SET_CARD_STATUS(0);
        result=commHelper.SendAnswer(AnsType::CardAbsent,NULL,0);
      }
      // reset
      else
      {
        result=card_reset();
        if(result)
          SET_CARD_STATUS(1);
      }
      break;
    case ReqType::CardCommit:
      if(writePending>0)
      {
        LOG("Internal error! ReqType::CardCommit: writePending>0!");
        result=0;
      }
      else
      {
        smartCard.inBuffer.WriteCommit();
        //may take a LONG TIME while performing requested operation
        smartCard.Commit();
        //send Ok answer
        result=commHelper.SendAnswer(AnsType::Ok,NULL,0);
      }
      break;
    case ReqType::CardReadComplete:
      //trim data from buffer that was succsesfully read before
      smartCard.outBuffer.ReadTrim();
      //send Ok answer
      result=commHelper.SendAnswer(AnsType::Ok,NULL,0);
      break;
    case ReqType::CardRead:
      //rewind outgoing buffer in order to recover from previous read-result (if any)
      smartCard.outBuffer.ReadRewind();
      //TODO: get outgoing-buffer data-size and compare it with requested data-size
      //TODO: read data from buffer and send it
      //send EOD
      result=commHelper.SendAnswer(AnsType::CardEOD,NULL,0);
      break;
    case ReqType::CardSend:
      if(writePending>0)
      {
        //TODO: write incoming data to card's inBuffer, decrement writePending
        //send Ok answer
        result=commHelper.SendAnswer(AnsType::Ok,NULL,0);
      }
      else
      {
        //first send-command, encodes total bytes to be written
        //rewind incoming buffer to recover from possible error
        smartCard.inBuffer.WriteRewind();
        //TODO: decode bytes to be writen and set it to writePending var
        writePending=1;
        //send Ok answer
        result=commHelper.SendAnswer(AnsType::Ok,NULL,0);
      }
      break;
    default:
      LOG("Incorrect request type");
    case ReqType::Invalid:
    case ReqType::Resync:
    case ReqType::ResyncComplete:
      result=0;
      break;
  }
  if(!result)
  {
    //reset write pending bytes
    writePending=0;
    resync();
    return;
  }
}


