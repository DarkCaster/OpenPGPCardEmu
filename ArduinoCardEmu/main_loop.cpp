#include "main_loop.h"

#ifdef STANDALONE_APP

#include "standalone_config.h"
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

void resync()
{
  //TODO: create and send ANS_RESYNC response, status=-1
  #define RESYNC_FAILED() \
  ({\
    comm_message(commBuffer,ANS_RESYNC,commBuffer+CMD_HDR_SIZE,0);\
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
      //TODO: read remaining data
      //verify, if verification failed - send ANS_RESYNC, status==-1, return
      if(!comm_verify(commBuffer,(uint8_t)(remLen+CMD_HDR_SIZE)))
      {
        RESYNC_FAILED();
        return;
      }
      //TODO: send ANS_OK
      //resync complete!
      status=1;
      return;
    }
    else
      status=-1;
  }
  if(status==-1)
  {
    //TODO: check header type
    //TODO: if header is not RESYNC, send ANS_RESYNC with 0 bytes payload, return
    if(0)
    {
      RESYNC_FAILED();
      return;
    }
    //TODO: read remaining data, check for timeout
    //TODO: if timeout fired or checksum is invalid - send ANS_RESYNC with 0 bytes payload, return
    if(0)
    {
      RESYNC_FAILED();
      return;
    }
    //TODO: create resync answer + checksum
    //TODO: send answer to serial
    status--;
  }
  else
    //invalid status
    RESYNC_FAILED();
}

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


