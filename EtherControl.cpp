// 2013-10-01 <doug@powersline.com>

#include <hostutil.h>
#include "EtherControl.h"

#if ARDUINO >= 100
#include <Arduino.h> // Arduino 1.0
#else
#include <Wprogram.h> // Arduino 0022
#endif

//#define EMULATE_PACKET_LOSS_PCT 25

#define MAC_SIZE 6
#define HEADER_LENGTH (MAC_SIZE*2 + sizeof(uint16_t))

/* ========================================================================= */
/*                               H E L P E R S                               */
/* ========================================================================= */
bool EtherControl::macsEquate(uint8_t mac_addr[MAC_SIZE], 
			      uint8_t mac_addr_compare[MAC_SIZE]){
  for(int i=0; i<MAC_SIZE; i++)
    if (mac_addr[i] != mac_addr_compare[i]) return false;
  return true;
}

/* ========================================================================= */
/*                   P R O T O C O L    R E G I S T R A T I O N              */
/* ========================================================================= */
void EtherControl::initProtocolRegistry(){
  int i;
  for(i=0; i<protocolCapacity;i++){
    protocolRegistry[i].etherType = 0;
    protocolRegistry[i].handler = NULL;
  }
}//end initRegistry

bool EtherControl::registerProtocol(uint16_t etherType, 
				    PayloadHandler *handler){
  int i;
  for(i=0; i<protocolCapacity;i++){
    if (protocolRegistry[i].etherType == 0 //not in use
	|| protocolRegistry[i].etherType == etherType){ //replace existing
      protocolRegistry[i].etherType = etherType;
      protocolRegistry[i].handler = handler;
      return true;
    }
  }

  return false;
}//end registerProtocol

PayloadHandler* EtherControl::getProtocolHandler(uint16_t etherType){
  int i;  
  for(i=0; i<protocolCapacity;i++){
    if(protocolRegistry[i].etherType == etherType){
      return protocolRegistry[i].handler;
    }//end if
  }//end for
  return NULL;
}//end getProtocolHandler

/* ========================================================================= */
/*                      T I M E R    R E G I S T R A T I O N                 */
/* ========================================================================= */
void EtherControl::initTimerRegistry(){
  timers = 0;
  int i;
  for(i=0; i<timerCapacity; i++){
    timerRegistry[i].handler = NULL;
    timerRegistry[i].startTime = 0;
    timerRegistry[i].delayTime = 0;
  }//end for
}//initTimerRegistry

uint8_t EtherControl::registerTimer(TimerHandler *handler, 
				    uint16_t millisDelay){
  int i;
  for(i=0; i<timerCapacity; i++){
    if (timerRegistry[i].handler == NULL){
      timerRegistry[i].handler = handler;
      timerRegistry[i].delayTime = millisDelay;
      timerRegistry[i].startTime = host_millis();
      timers++;
      return i+1;
    }
  }//end for
  return 0;
}

void EtherControl::unregisterTimer(uint8_t index){
  if (timerRegistry[index-1].handler != NULL){
    timers--;
    timerRegistry[index-1].handler = NULL;
    timerRegistry[index-1].startTime = 0;
    timerRegistry[index-1].delayTime = 0;
  }
}

void EtherControl::processTimers(){
  int processed = 0;
  int i;
  for(i=0; processed < timers && i<timerCapacity;i++){
    if (timerRegistry[i].handler != NULL && 
	host_millis()-timerRegistry[i].startTime>timerRegistry[i].delayTime){
      timerRegistry[i].startTime = host_millis();
      timerRegistry[i].handler->handleTimer((uint8_t)(i+1));
      processed++;
    }//end if
  }//end for
}//end processTimers


EtherControl::EtherControl (EthernetDriver *driver, 
			    uint8_t protocolCapacity,
			    uint8_t timerCapacity){

  //many of the upper layer protocols in the library 
  //use the hostutils library, so let's just make sure its
  //initialized here
  hostinit();

  this->driver = driver;

  //initialize the payload buffer
  //the frame starts with 2 mac addresses (6 uint8_ts each)
  //and then 2 uint8_ts for the EtherType.  Everything after
  //that is the payload
  sendPayloadBuffer = new OffsetBuffer(driver->getSendBuffer(),HEADER_LENGTH);
  
  timerRegistry = (timer*) malloc(timerCapacity * sizeof(timer));

  if (timerRegistry != NULL)
    this->timerCapacity = timerCapacity;
  else{
    this->timerCapacity = 0;
#ifdef DEBUG
    fprintf(stderr,"Out of memory allocating timerRegistry in EtherControl\n");
#endif
  }

  protocolRegistry = (protocolMap*)
    malloc(protocolCapacity * sizeof(protocolMap));

  if (protocolRegistry != NULL)
    this->protocolCapacity = protocolCapacity;
  else{
    this->protocolCapacity = 0;
#ifdef DEBUG
  if (protocolRegistry == NULL)
    fprintf(stderr,
	    "Out of memory allocating protocolRegistry in EtherControl\n");
#endif
  }

  initTimerRegistry();
  initProtocolRegistry();
}

EtherControl::~EtherControl(){
  delete sendPayloadBuffer;
}

/* ========================================================================= */
/*                           S E N D  /  R E C E I V E                       */
/* ========================================================================= */
Buffer* EtherControl::getSendPayloadBuffer(){  
  return sendPayloadBuffer;
}

bool EtherControl::sendFrame(const uint8_t *destinationMAC, uint16_t protocol, 
			     uint16_t payloadLength){

#ifdef EMULATE_PACKET_LOSS_PCT
  if ((random() % 100) + 1 < EMULATE_PACKET_LOSS_PCT) return true;
#endif

  Buffer* sendBuffer = driver->getSendBuffer();

  //we need 12 uint8_ts for two MACs plus 2 uint8_ts for payload size
  if (payloadLength > sendBuffer->size() - HEADER_LENGTH)
    return false; //too big

  //set the mac_dest
  bool e = sendBuffer->write(0,(void*)destinationMAC,MAC_SIZE);
  if (!e) return false;

  //set the mac_src
  e = sendBuffer->write(MAC_SIZE,this->driver->getMACAddr(),MAC_SIZE);
  if (!e) return false;

  //set the protocol
  e = sendBuffer->writeNet16(MAC_SIZE+MAC_SIZE,protocol);
  if (!e) return false;

  //payload should already be set
  //so just send the packet
  uint16_t frame_len = HEADER_LENGTH+payloadLength;
  driver->sendFrame(frame_len);
  return true;
}

bool EtherControl::sendFrame(const uint8_t *destinationMAC, uint16_t protocol, 
			     uint16_t length,uint8_t *payload){

  Buffer* sendBuffer = driver->getSendBuffer();
  
  //set the payload
  bool e = sendBuffer->write(HEADER_LENGTH,payload,length);
  if (!e) return false;

  //send the frame with the rest of the header
  return this->sendFrame(destinationMAC,protocol,length);
}//send frame

bool EtherControl::processFrame(){
  Buffer* recvBuffer = driver->getReceiveBuffer();

  uint16_t len = driver->receiveFrame();

#ifdef EMULATE_PACKET_LOSS_PCT
  if ((random() % 100) + 1 < EMULATE_PACKET_LOSS_PCT) return true;
#endif
  
  if (len > 0){
    
    //we have a frame, get the etherType
    uint16_t etherType;
    if (!recvBuffer->readNet16(MAC_SIZE*2,&etherType)) return false;

    //lookup the handler for the etherType
    PayloadHandler *handler = getProtocolHandler(etherType);

    //if we found a handler, call it with the payload
    if (handler != NULL){
      uint16_t payloadLength = len - HEADER_LENGTH;
      OffsetBuffer frameBuffer = OffsetBuffer(recvBuffer,HEADER_LENGTH,
					      payloadLength);
      handler->handlePayload(&frameBuffer);
    }//done calling handler

  }//end if we have a frame

  //process our timers
  processTimers();

  return true;
}//end processFrame

uint16_t EtherControl::getMaxReceivePayload(){
  Buffer* recvBuffer = driver->getReceiveBuffer();
  return recvBuffer->size() - HEADER_LENGTH;
}

/* ========================================================================= */
/*                                A C C E S S O R S                          */
/* ========================================================================= */
uint8_t *EtherControl::getMACAddress(){
  return this->driver->getMACAddr();
}
