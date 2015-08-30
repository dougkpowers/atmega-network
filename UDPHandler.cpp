/*
 * This library builds on the IP API by providing UDPHandler
 * (User Datagram Protocol) capabilities.
 *
 * The UDPHandler protocol is very simple as it is stateless.  It is
 * fully implemented here except for the checksum.  The checksum is
 * optional for IPv4, so this shouldn't be a big issue.  To send a 
 * datagram simply use one of the sendDatagram functions.
 *
 * You can listen for incoming datagrams on any given port.  Simply
 * register a callback function via registerListener(port,datagramReceiver)
 * and the callback will be executed everytime a UDPHandler packet is received
 * on the given port.
 */

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <hostutil.h>
#include "UDPHandler.h"

#define DATAGRAM_HEADER_LENGTH 8

/* ========================================================================= */
/*                           C O N S T R U C T O R S                         */
/* ========================================================================= */
UDPHandler::UDPHandler(IPHandler *ipHandler, uint8_t receiverCount){
  this->ip = ipHandler;
  
  this->receivers = (listenerMap*)malloc(receiverCount * sizeof(listenerMap));

  if (this->receivers != NULL)
    this->receiverCount = receiverCount;
  else
    this->receiverCount = 0;

  for(int i=0; i<this->receiverCount; i++){
    this->receivers[i].port = 0;
    this->receivers[i].receiver = NULL;
  }//end for

  ipHandler->registerProtocol(UDP_PROTOCOL,this);

  sendPayloadBuffer = new OffsetBuffer(ipHandler->getSendPayloadBuffer(),
				       DATAGRAM_HEADER_LENGTH );

}//end constructor

UDPHandler::~UDPHandler(){
  delete sendPayloadBuffer;
}

/* ========================================================================= */
/*                   L I S T E N E R    R E G I S T R A T I O N              */
/* ========================================================================= */
bool UDPHandler::registerListener(uint16_t port, DatagramReceiver *receiver){

  //see if the port already has a listener
  //if so, replace the existing listener
  for(int i=0; i<receiverCount;i++){
    if (receivers[i].port == port){
      receivers[i].receiver = receiver;
      return true;
    }
  }

  //see if we have an empty spot in the registry
  for(int i=0; i<receiverCount;i++){
    if (receivers[i].receiver == NULL){
      receivers[i].port     = port;
      receivers[i].receiver = receiver;
      return true;
    }
  }

  //no cigar
  return false;
}//end registerListener

void UDPHandler::unregisterListener(uint16_t port){
  for(int i=0; i<receiverCount;i++){
    if(receivers[i].port == port){
      receivers[i].receiver = NULL;
      receivers[i].port = 0;
    }//end if
  }//end for
}

DatagramReceiver* UDPHandler::getListener(uint16_t port){
  for(int i=0; i<receiverCount;i++){
    if(receivers[i].port == port){
      return receivers[i].receiver;
    }//end if
  }//end for
  return NULL;
}//end getListener


/* ========================================================================= */
/*                                 N E T W O R K                             */
/* ========================================================================= */
Buffer* UDPHandler::getSendPayloadBuffer(){

  return sendPayloadBuffer;

}//end getPayloadBuffer

bool UDPHandler::sendDatagram(uint8_t *destinationIP, 
			      uint16_t destinationPort, 
			      uint16_t sourcePort, uint16_t payloadLength){
  
  uint16_t len = DATAGRAM_HEADER_LENGTH + payloadLength;

  Buffer *datagram = ip->getSendPayloadBuffer();
  if (datagram->size() <  len){
#ifdef DEBUG
    fprintf(stderr,"Err: buffer not large enough for datagram.\n");
#endif
    return false;
  }

  //populate the Datagram packet header
  if (!datagram->writeNet16(0,sourcePort)) return false;
  if (!datagram->writeNet16(2,destinationPort)) return false;
  if (!datagram->writeNet16(4,payloadLength +  DATAGRAM_HEADER_LENGTH )) 
      return false;

  //calculate the checksum
  if (!datagram->writeNet16(6,calcChecksum(datagram,len,destinationIP))) 
    return false;
  //  if (!datagram->write16(6,0x0000)) return false;
  return ip->sendPacket(destinationIP,UDP_PROTOCOL,
			payloadLength +  DATAGRAM_HEADER_LENGTH );
}//end sendDatagram

uint32_t UDPHandler::calcChecksum(Buffer* buf, uint16_t len, 
				  uint8_t* remoteIP){
  uint32_t pseudo = len;
  pseudo += UDP_PROTOCOL;

  //source ip
  uint8_t* localIP = this->getIPHandler()->getIPAddress();
  pseudo += HTONS(*((uint16_t*)localIP));
  pseudo += HTONS(*((uint16_t*)(localIP+2)));
  
  //destination ip
  pseudo += HTONS(*((uint16_t*)remoteIP));
  pseudo += HTONS(*((uint16_t*)(remoteIP+2)));

  return buf->checksum(len,6,pseudo);
}

bool UDPHandler::sendDatagram(uint8_t *destinationIP, 
			      uint16_t destinationPort, 
			      uint16_t payloadLength){
  return sendDatagram(destinationIP,destinationPort,0,payloadLength);
}//sendDatagram
   
bool UDPHandler::sendDatagram(uint8_t *destinationIP, uint16_t destinationPort,
			      uint16_t sourcePort, uint16_t payloadLength, 
			      uint8_t *payload){

  //get the transmit buffer and make sure we have enough room
  Buffer *datagram = ip->getSendPayloadBuffer();
  if (datagram->size() <  DATAGRAM_HEADER_LENGTH  + payloadLength){
#ifdef DEBUG
    fprintf(stderr,"Err: buffer not large enough for datagram.\n");
#endif
    return false;
  }

  //populate the packet buffer
  if (!datagram->write( DATAGRAM_HEADER_LENGTH ,payload,payloadLength)) 
    return false;

  //send the packet
  return sendDatagram(destinationIP,destinationPort,sourcePort,payloadLength);

}//end sendDatagram

bool UDPHandler::sendDatagram(uint8_t *destinationIP, uint16_t destinationPort,
		       uint16_t payloadLength, uint8_t *payload){
  return sendDatagram(destinationIP,destinationPort,0,payloadLength,payload);
}//sendDatagram

bool UDPHandler::sendDatagram(uint8_t *destinationIP, uint16_t destinationPort,
		       uint16_t sourcePort, char *payload){
  uint16_t len = strlen(payload) + 1;
  return sendDatagram(destinationIP,destinationPort,sourcePort,
		      len,(uint8_t*)payload);
}//sendDatagram

bool UDPHandler::sendDatagram(uint8_t *destinationIP, uint16_t destinationPort,
		       char *payload){
  return sendDatagram(destinationIP,destinationPort,0,payload);
}//endDatagram


void UDPHandler::handlePacket(uint8_t *sourceIP, Buffer *datagram){

  //our payload must at least be as big as our datagram_header
  if (datagram->size() <  DATAGRAM_HEADER_LENGTH )
    return;

  //the size of the datagram should be less than or equal to the
  //ip payload. If not, discard the datagram because something is wrong
  uint16_t udpLength;
  if (!datagram->readNet16(4,&udpLength) || udpLength > datagram->size())
    return;
  
  //verify the checksum
  uint16_t checksum;
  if (!datagram->readNet16(6,&checksum)) return;
  //the checksum is optional, so if set to all zero's we can ignore
  if (checksum != 0){
    if (checksum != calcChecksum(datagram,datagram->size(),sourceIP))
      return;
  }

  //determine our port and call our port listener
  uint16_t destinationPort;
  uint16_t sourcePort;
  uint16_t datagramLength;

  if (!datagram->readNet16(0,&sourcePort)) return;
  if (!datagram->readNet16(2,&destinationPort)) return;
  if (!datagram->readNet16(4,&datagramLength)) return;
  
  DatagramReceiver *receiver = getListener(destinationPort);
  
  if (receiver != NULL){
    OffsetBuffer datagramPayloadBuffer = OffsetBuffer(datagram,
						      DATAGRAM_HEADER_LENGTH,
						      datagramLength - 
						      DATAGRAM_HEADER_LENGTH);
    receiver->handleDatagram(sourceIP,
			     sourcePort,
			     &datagramPayloadBuffer);
  }
}//end handlePayload

IPHandler* UDPHandler::getIPHandler(){
  return ip;
}
