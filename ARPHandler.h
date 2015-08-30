#ifndef ARP_H
#define ARP_H

#include <stdint.h>
#include <Buffer.h>
#include <EtherControl.h>
#include <TimerHandler.h>

typedef struct etherRoute{
  uint8_t ipAddress[4];
  uint8_t macAddress[6];
  uint32_t lookupTime;

  //lookup status bits
  //bit 7 indicates if the route is resolved
  //the rest of the bits represent the lookup count
  uint8_t lookupStatus;
} etherRoute;

class ARPHandler: PayloadHandler, TimerHandler{

  EtherControl *etherControl;
  uint8_t ipAddress[4];
  etherRoute *routingTable;
  uint8_t routingTableSize;
  uint8_t timer;

  bool sendARPRequest(uint8_t target_protocol_addr[4]);
  bool sendARPResponse(uint8_t target_hardware_addr[6],
		       uint8_t target_protocol_addr[4]);

 public:
  ARPHandler(uint8_t *ipAddress, uint8_t routingTableSize, 
	     EtherControl *control);
  void handlePayload(Buffer *p);
  uint8_t* getMACAddress(uint8_t *remoteIP);
  bool requestMACAddress(uint8_t *remoteIP);
  void handleTimer(uint8_t timer);
  void printRoutingTable();
};

#endif
