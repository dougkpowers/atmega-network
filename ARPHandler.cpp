/*
 * This library builds on the EtherControl API by providing ARP
 * (Address Resolution Protocol) capabilities.
 *
 * Limitations:
 *  - This library only supports the IP address resolution
 *    over ethernet via the ARP protocol. 
 *  - Only the simple request/response protocol is implemented
 *    as outlined in RFC 826.  ARP Probe, ARP announcemnts,
 *    ARP mediation, inverse ARP, etc are not implemented.
 *  - In order to conserve RAM, it is recommended that the ARP routing
 *    table be limited in size.  Once fully allocated, all future
 *    MAC address requests will simply return NULL.  Old ARP route
 *    entries are never discarded.
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <hostutil.h>
#include <Buffer.h>
#include "IPHandler.h"
#include "ARPHandler.h"

#define ARP_RESOLVED 0b10000000
#define ARP_PROTOCOL 0x0806
#define ETH_PROTOCOL 0x0001
#define ARP_REQUEST 0x0001
#define ARP_RESPONSE 0x0002

ARPHandler::ARPHandler(uint8_t *ipAddress, uint8_t routingTableSize,
		       EtherControl *control){
  memcpy(this->ipAddress,ipAddress,4);
  etherControl = control;
  etherControl->registerProtocol(ARP_PROTOCOL,this);

  //initialize the routing table
  this->routingTableSize = routingTableSize;
  this->routingTable = (etherRoute*)
    malloc(sizeof(etherRoute)*routingTableSize);
  if (this->routingTable == NULL)
    this->routingTableSize = 0;
  else
    memset(routingTable,0,sizeof(etherRoute)*routingTableSize);

  //we have no active timer
  timer = 0;
}//end constructor

/* ==================================================================== */
/*                  S T A T U S    M G M T    H E L P E R S             */
/* ==================================================================== */
//lookup status bits
//bit 7 indicates if the route is resolved
//the rest of the bits indicate the lookup count
#define ATTEMPTCOUNT(route) ((((route).lookupStatus) << 1) >> 1)
#define SETATTEMPTCOUNT(route,value) ((route).lookupStatus = (route).lookupStatus & ARP_RESOLVED | ((value) << 1 >> 1))


/* ==================================================================== */
/*                          A R P    M G M T                            */
/* ==================================================================== */
bool ARPHandler::sendARPRequest(uint8_t target_protocol_addr[4]){

  Buffer* etherBuffer = etherControl->getSendPayloadBuffer();

  uint8_t *sender_hardware_addr = etherControl->getMACAddress();

  if (!etherBuffer->writeNet16(0,ETH_PROTOCOL)) return false; // 1 for ethernet
  if (!etherBuffer->writeNet16(2,IP_PROTOCOL)) return false;  // 0x0800 for IP
  if (!etherBuffer->write8(4,6)) return false; // mac addresses are 6 bytess
  if (!etherBuffer->write8(5,4)) return false; // IPv4 addresses are 4 bytes
  if (!etherBuffer->writeNet16(6,ARP_REQUEST)) return false;  // 1 is request
  if (!etherBuffer->write(8,sender_hardware_addr,6)) return false; //sender mac
  if (!etherBuffer->write(14,ipAddress,4)) return false;           //sender ip

 //target hw address
  for(int i=0;i<6;i++)
    if (!etherBuffer->write8(18+i,0x00)) return false;

  //target ip address
  if (!etherBuffer->write(24,target_protocol_addr,4)) return false;

  return etherControl->sendFrame(broadcastMAC,ARP_PROTOCOL,28);
}//end sendARPRequest

//send my IP and my mac to the requestor
bool ARPHandler::sendARPResponse(uint8_t target_hardware_addr[6],
			  uint8_t target_protocol_addr[4]){

  Buffer* etherBuffer = etherControl->getSendPayloadBuffer();

  uint8_t *sender_hardware_addr = etherControl->getMACAddress();
  
  if (!etherBuffer->writeNet16(0,ETH_PROTOCOL)) return false; // 1 for ethernet
  if (!etherBuffer->writeNet16(2,IP_PROTOCOL)) return false;   // 0x0800 for IP
  if (!etherBuffer->write8(4,6)) return false; // mac addresses are 6 bytes
  if (!etherBuffer->write8(5,4)) return false; // IPv4 addresses are 4 bytes
  if (!etherBuffer->writeNet16(6,ARP_RESPONSE)) return false;// 2 is response
  if (!etherBuffer->write(8,sender_hardware_addr,6)) return false; //sender mac
  if (!etherBuffer->write(14,ipAddress,4)) return false;   //sender ip
  
  //target hw address
  if (!etherBuffer->write(18,target_hardware_addr,6)) return false;
  //target ip address   
  if (!etherBuffer->write(24,target_protocol_addr,4)) return false;

  return etherControl->sendFrame(target_hardware_addr,ARP_PROTOCOL,28);
}//end sendARPResponse

void ARPHandler::handlePayload(Buffer *p){

  if (p->size() < 28) return; //inbound packet should be at least 28 bytes
                              //the size of an ARP frame

  uint8_t *macAddress = etherControl->getMACAddress();

  //we only care about ethernet
  uint16_t w;
  if (!p->readNet16(0,&w) || w != ETH_PROTOCOL) return;
 
  //we only care about ip
  if (!p->readNet16(2,&w) || w != IP_PROTOCOL) return;

  //wouldn't know what to do if mac address was not 6 bytess 
  uint8_t b;
  if (!p->read8(4,&b) || b != 6) return;  

  //wouldn't know what to do if ip address was not 4 bytess
  if (!p->read8(5,&b) || b != 4) return;

  // get the ARP type
  uint16_t operation;
  if (!p->readNet16(6,&operation)) return;

  uint8_t sender_hardware_addr[6];
  uint8_t sender_protocol_addr[4];
  uint8_t target_hardware_addr[6];
  uint8_t target_protocol_addr[4];
  if (!p->read(8,sender_hardware_addr,6)) return;
  if (!p->read(14,sender_protocol_addr,4)) return;
  if (!p->read(18,target_hardware_addr,6)) return;
  if (!p->read(24,target_protocol_addr,4)) return;
  
  if (operation == ARP_REQUEST){

    //ignore responses not sent to our ip
    if (!IPHandler::ipsEquate(target_protocol_addr,ipAddress)) return;
    
    sendARPResponse(sender_hardware_addr,sender_protocol_addr);
  }//end if request

  //if we received a response
  else if (operation == ARP_RESPONSE){
    //ignore responses not sent to our mac  
    if (!EtherControl::macsEquate(target_hardware_addr,macAddress)) 
      return;
  
    //ignore responses not sent to our ip
    if (!IPHandler::ipsEquate(target_protocol_addr,ipAddress)) 
      return;
  
    //ok, let's find the row in our ARP 
    //table that represents the request we made
    for(int i=0; i<routingTableSize; i++){
      if (IPHandler::ipsEquate(sender_protocol_addr,
			routingTable[i].ipAddress)){
	memcpy(routingTable[i].macAddress,sender_hardware_addr,6);
	//mark the route as resolved (the first bit represents resolved)
	routingTable[i].lookupStatus |= ARP_RESOLVED;
	
	return;
      }
    }//end for

    //Otherwise, we should ignore the response as we have a limited
    //ARP table size.  We only want to store the IPs we care about
  }//end if response
}//end handlePayload

uint8_t* ARPHandler::getMACAddress(uint8_t *remoteIP){

  //try to find the ip in our routing table
  for(int i=0; i<routingTableSize; i++){
    if (IPHandler::ipsEquate(routingTable[i].ipAddress,remoteIP))
      //if resolved then just return the MAC address
      if ((routingTable[i].lookupStatus & ARP_RESOLVED))
	return routingTable[i].macAddress;
      else //otherwise, return NULL.  We're already looking it up
	return NULL;
  }//end for

  //we don't have it at all
  return NULL;
}//end getMACAddress

//regardless of whether or not we have the IP address in our
//routing table, go and try to fetch the MAC for the IP
bool ARPHandler::requestMACAddress(uint8_t *remoteIP){

  //first, let's try to find the ip in our routing table
  int index = -1;
  for(int i=0; i<routingTableSize; i++){
    if (IPHandler::ipsEquate(routingTable[i].ipAddress,remoteIP))
      //if not resolved, then we're already looking it up
      if (!(routingTable[i].lookupStatus & ARP_RESOLVED))
	return true;
      else //we'll use this entry for our index
	index = i;
  }//end for

  //if it was not found, then we need to allocate an entry in our table
  if (index == -1){
    for(int i=0; i<routingTableSize; i++){

      //if the status is not resolved and the lookup 
      //count is zero then this entry is not in use
      if (!(routingTable[i].lookupStatus & ARP_RESOLVED) &&
	  ATTEMPTCOUNT(routingTable[i]) == 0){
	index = i;
	break;
      }//end if we found an opportunity
    }//end for
  }//end if we need an index

  if (index == -1){ //no more room  
#ifdef DEBUG
    fprintf(stderr,"Err: no more room in ARP table.");
#endif
    return false;
  }

  //register a timer, if we haven't already
  if (timer == 0){
    timer = etherControl->registerTimer(this,250);
    if (timer == 0){
#ifdef DEBUG
      fprintf(stderr,"Err: Could not register timer for ARPHandler.");
#endif
      return false;
    }
  }

  //set the attempt count to 1
  SETATTEMPTCOUNT(routingTable[index],1);

  //set the start time for the lookup
  routingTable[index].lookupTime = host_millis();
  
  //set the IP address we are searching for
  memcpy(routingTable[index].ipAddress,remoteIP,4);

  //once we have made an entry in the routing
  //table, we can send the request
  return sendARPRequest(remoteIP);

}//end requestMACAddress

void ARPHandler::handleTimer(uint8_t index){

  uint8_t lookupCount = 0;
  uint32_t current = host_millis();

  //loop through our ARP table and see if we
  //have any ARP searches that have expired
  for(int i=0; i<routingTableSize; i++){

    uint8_t count = ATTEMPTCOUNT(routingTable[i]);

    //if the attempt count is zero, then the entry is not active
    if (count == 0)
      continue;

    //if we are resolved, continue
    if (routingTable[i].lookupStatus & ARP_RESOLVED)
      continue;

    //we have an active lookup
    lookupCount++;

    //if it has not been more than 250ms, continue
    if (current - routingTable[i].lookupTime < 250)
      continue;

    //it's been more than 1 second.  If our count is at 5 or greater
    if (count >= 5){
      //just forget we ever made a request
      memset(&routingTable[i],0,sizeof(etherRoute));
    }//end if
    else if (count > 0){
      //otherwise, increase our attempt counter
      SETATTEMPTCOUNT(routingTable[i],count+1);
      
      //update our lookup time
      routingTable[i].lookupTime = current;

      //and send another ARP request
      sendARPRequest(routingTable[i].ipAddress);
    }
  }//end for

  //if we have no active lookups
  if (lookupCount == 0 && timer != 0){
    etherControl->unregisterTimer(timer);
    timer = 0;
  }

}//end handleTimer

void ARPHandler::printRoutingTable(){
  printf("ARP Routing Table\n");
  printf
    ("--------------------------------------------------------------------\n");

  for(int i=0; i<routingTableSize; i++){
    int skipChars = 0;
    if (!(routingTable[i].lookupStatus & ARP_RESOLVED) &&
	ATTEMPTCOUNT(routingTable[i]) == 0)
      continue;

    if (i<10) printf(" ");
    printf  ("%d: ",i);
    for(int j=0; j<3; j++){
      printf("%u",routingTable[i].ipAddress[j]);
      if (routingTable[i].ipAddress[j] < 10) skipChars++;
      if (routingTable[i].ipAddress[j] < 100) skipChars++;
      printf(".");
    }
    printf("%u",routingTable[i].ipAddress[3]);
    if (routingTable[i].ipAddress[3] < 10) skipChars++;
    if (routingTable[i].ipAddress[3] < 100) skipChars++;

    for(int x=0; x<skipChars; x++) printf(" ");
    printf("\t->\t");

    for(int j=0; j<5; j++){
      if (routingTable[i].macAddress[j] <=9) printf("0");
      printf("%x",routingTable[i].macAddress[j]);
      printf("-");
    }
    if (routingTable[i].macAddress[5] <=9) printf("0");
    printf("%x",routingTable[i].macAddress[5]);

    printf("\t| ");
    printf((routingTable[i].lookupStatus & ARP_RESOLVED) ? 
	   "resolved\n" : "fetching\n");
    
  }//end for
  printf("\n");
}//end printRoutingTable


