/*
 * This library builds on the EtherControl API by providing IP
 * (Internet Protocol) capabilities.
 *
 * Limitations:
 *  - This library does not handle TCP, UDP, ICMP or any other
 *    higher level protocols.  It simply handles wrapping
 *    of a payload inside an IP packet and sending it to the
 *    Ethernet controller, who in turn, can send it out as an
 *    ethernet frame.  For higher level protocol support, see
 *    the TCP, UDP, or ICMP classes.
 *  - This librarly will attempt to send messages to machines on 
 *    the same subnet directly, by performing an ARP request. If
 *    there is no response, then the message will be sent to the
 *    gateway.
 *  - For messages sent to IPs that are not on the same subnet,
 *    the messages will always be sent to the gateway.
 *  - There is no support for IP options (use of octets 20 through
 *    160 in the IP header).  These are not needed for TCP, UDP, or
 *    ICMP, so this should not be an issue.
 *  - The size of the IP routing table should be kept small, in order
 *    to minimize the use of RAM.  If you are connecting to N machines
 *    make the entry N in size.  Entries are never recycled.
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "IPHandler.h"

/* ========================================================================= */
/*                               H E L P E R S                               */
/* ========================================================================= */
bool IPHandler::ipsEquate(uint8_t ip_addr[4], uint8_t ip_addr_compare[4]){
  for(int i=0; i<4; i++)
    if (ip_addr[i] != ip_addr_compare[i]) return false;
  return true;
}

/* ========================================================================= */
/*                           C O N S T R U C T O R S                         */
/* ========================================================================= */
void IPHandler::init(uint8_t *ipAddress, uint8_t *gatewayIP, 
		     uint8_t *subnetMask, ARPHandler *arp, 
		     EtherControl *control){
  this->arp = arp;
  this->etherControl = control;
  memcpy(this->ipAddress,ipAddress,4);
  memcpy(this->gatewayIP,gatewayIP,4);
  memcpy(this->subnetMask,subnetMask,4);

  //set the network address
  for(int i=0; i<4; i++)
    this->ipNetwork[i] = this->ipAddress[i] & this->subnetMask[i];

  //set the broadcast address
  for(int i=0; i<4; i++)
    this->ipBroadcastAddress[i] = 
      (this->ipNetwork[i] | (~(this->subnetMask[i])));

  //initialize our protocol registry
  initProtocolRegistry();

  //set our starting source port
  this->nextPort = random() % 10000;

  etherControl->registerProtocol(IP_PROTOCOL,this);

  Buffer *frameBuffer = this->etherControl->getSendPayloadBuffer();
  this->sendPacketBuffer = new OffsetBuffer(frameBuffer,IP_HEADER_LENGTH);
}

IPHandler::IPHandler(uint8_t *ipAddress, uint8_t *gatewayIP, 
		     uint8_t *subnetMask,
		     ARPHandler *arp, EtherControl *control){
  init(ipAddress,gatewayIP,subnetMask,arp,control);
}

IPHandler::IPHandler(uint8_t *ipAddress, uint8_t *gatewayIP, 
		     uint8_t *subnetMask, 
		     uint8_t routingTableSize, EtherControl *control){
  ARPHandler* arp = new ARPHandler(ipAddress,routingTableSize,control);
  init(ipAddress,gatewayIP,subnetMask,arp,control);
}

IPHandler::~IPHandler(){
  delete sendPacketBuffer;
}

/* ========================================================================= */
/*                   P R O T O C O L    R E G I S T R A T I O N              */
/* ========================================================================= */
void IPHandler::initProtocolRegistry(){
  int i;
  for(i=0; i<sizeof(protocolRegistry) / sizeof(ipProtocolMap);i++){
    protocolRegistry[i].ipProtocol = 0;
    protocolRegistry[i].handler = NULL;
  }
}//end initRegistry

bool IPHandler::registerProtocol(uint8_t protocol, PacketHandler *handler){
  int i;
  for(i=0; i<sizeof(protocolRegistry) / sizeof(protocolMap);i++){
    if (protocolRegistry[i].ipProtocol == 0 //not in use
        || protocolRegistry[i].ipProtocol == protocol){ //replace existing
      protocolRegistry[i].ipProtocol = protocol;
      protocolRegistry[i].handler = handler;
      return true;
    }
  }

  return false;
}//end registerProtocol

PacketHandler* IPHandler::getProtocolHandler(uint8_t ipProtocol){
  int i;
  for(i=0; i<sizeof(protocolRegistry) / sizeof(protocolMap);i++){
    if(protocolRegistry[i].ipProtocol == ipProtocol){
      return protocolRegistry[i].handler;
    }//end if
  }//end for
  return NULL;
}//end getProtocolHandler

/* ========================================================================= */
/*                      T I M E R    R E G I S T R A T I O N                 */
/* ========================================================================= */
uint8_t IPHandler::registerTimer(TimerHandler *handler, uint16_t millisDelay){
  etherControl->registerTimer(handler,millisDelay);
}

void IPHandler::unregisterTimer(uint8_t index){
  etherControl->unregisterTimer(index);
}

/* ========================================================================= */
/*                                 P R I V A T E                             */
/* ========================================================================= */
const uint8_t* IPHandler::getMACForIP(uint8_t *destinationIP){

  //see if the ip is on the local network
  bool onLocalNetwork = true;
  for(int i=0; i<4; i++)
    if (subnetMask[i] & destinationIP[i] != ipNetwork[i])
      onLocalNetwork = false;

  //if this is a broadcast address on the local network
  if (onLocalNetwork && ipsEquate(this->ipBroadcastAddress,destinationIP))
    return broadcastMAC;

  //else, if we're on the local network
  //lookup the MAC from our ARP tables

  if (onLocalNetwork){
    return this->arp->getMACAddress(destinationIP);
  }

  //if this is not a network address, then default to the gateway

  //otherwise, lookup the MAC for our gateway ip
  return this->arp->getMACAddress(this->gatewayIP);

}//end getMACForIP

/* ========================================================================= */
/*                                  P U B L I C                              */
/* ========================================================================= */
uint16_t IPHandler::getPort(){
  return this->nextPort++;
}

uint8_t* IPHandler::getIPAddress(){
  return this->ipAddress;
}

Buffer* IPHandler::getSendPayloadBuffer(){
  return sendPacketBuffer;
}//end getPacketpayloadBuffer

bool IPHandler::sendPacket(uint8_t *destinationIP, uint8_t protocol,
		    uint16_t packetPayloadLength){
  //get the transmit buffer and make sure we have enough room
  Buffer *p = etherControl->getSendPayloadBuffer();
  if (p->size() < IP_HEADER_LENGTH + packetPayloadLength){
#ifdef DEBUG
    fprintf(stderr,"Err: buffer not large enough for IP packet.\n");
#endif
    return false;
  }

  //make sure we have a route to our destination IP
  const uint8_t* macAddr = this->getMACForIP(destinationIP);

  if (macAddr == NULL){
#ifdef DEBUG
    fprintf(stderr,"Err: No route to host. Failed sending IP packet.\n");
#endif
    return false;
  }

  //populate the IP packet header
  p->write8(0,0x45); //IPv4, 5 32-bit uint16_ts in header
  p->write8(1,0x00);  //dscp and enc = 0
  p->writeNet16(2,packetPayloadLength + IP_HEADER_LENGTH); //ip frame length
  p->writeNet16(4,0); //identification
  p->writeNet16(6,0x4000); //flags and fragment offset
  p->write8(8,64); // set ttl to 64
  p->write8(9,protocol); //protocol
  p->write(12,this->ipAddress,4);
  p->write(16,destinationIP,4);

  //calculate the header checksum
  p->writeNet16(10,p->checksum(IP_HEADER_LENGTH,10));

  return etherControl->sendFrame(macAddr,IP_PROTOCOL,
				 packetPayloadLength + IP_HEADER_LENGTH);
}

bool IPHandler::sendPacket(uint8_t *destinationIP, uint8_t protocol,
			   uint16_t packetPayloadLength, uint8_t *payload){

  //get the transmit buffer and make sure we have enough room
  Buffer *p = etherControl->getSendPayloadBuffer();
  if (p->size() < IP_HEADER_LENGTH + packetPayloadLength){
#ifdef DEBUG
    fprintf(stderr,"Err: buffer not large enough for IP packet.\n");
#endif
    return false;
  }

  //populate the packet buffer
  p->write(IP_HEADER_LENGTH,payload,packetPayloadLength);

  //send the packet
  return sendPacket(destinationIP,protocol,packetPayloadLength);

}//end sendPacket


void IPHandler::handlePayload(Buffer *p){
  
  //our payload must at least be as big as our ip_header
  if (p->size() < IP_HEADER_LENGTH)
      return;

  //verify checksum of the IP header
  uint16_t checksum = 0;
  if (!p->readNet16(10,&checksum)) return;
  if (checksum != p->checksum(IP_HEADER_LENGTH,10)){
    return; //bad checksum; data corrupted in transit so discard
  }

  //the size of the packet should equal the size of the payload (or less))
  //If not, discard the packet because something is wrong
  uint16_t len = 0;
  if (!p->readNet16(2,&len)) return;
  if (p->size() < len) return;

  //we should only care about packet sent to our IP 
  //or packets sent to our broadcast address
  uint8_t ip[4];
  if (!p->read(16,ip,4)) return; //load the destination ip
  if (!ipsEquate(ip,ipAddress) &&
      !ipsEquate(ip,ipBroadcastAddress))
    return;

  //determine our protocol and call our protocol handler
  uint8_t protocol;
  if (!p->read8(9,&protocol)) return;
  PacketHandler *handler = getProtocolHandler(protocol);

  if (handler != NULL){
    OffsetBuffer ipPacketBuffer = 
      OffsetBuffer(p,IP_HEADER_LENGTH,len - IP_HEADER_LENGTH);
    
    //load the source ip
    if (!p->read(12,ip,4)) return;

    handler->handlePacket(ip,&ipPacketBuffer);
  }
  
}//end handlePayload

ARPHandler* IPHandler::getARPHandler(){
  return this->arp;
}

uint16_t IPHandler::getMaxReceivePayload(){
  return etherControl->getMaxReceivePayload() - IP_HEADER_LENGTH;
}


