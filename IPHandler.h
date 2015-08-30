#ifndef IP_H
#define IP_H

#include <stdint.h>
#include "EtherControl.h"
#include "ARPHandler.h"
#include "TimerHandler.h"
#include "Buffer.h"

#define IP_PROTOCOL 0x0800
#define IP_HEADER_LENGTH 20

class PacketHandler {

 public:
  virtual void handlePacket(uint8_t *sourceIP, Buffer *packet) = 0;

};

typedef struct ipProtocolMap {
  uint8_t ipProtocol;
  PacketHandler *handler;
} ipProtocolMap;

class IPHandler: PayloadHandler{

  EtherControl *etherControl;
  uint8_t ipAddress[4];
  uint8_t gatewayIP[4];
  uint8_t subnetMask[4];
  uint8_t ipNetwork[5];
  uint8_t ipBroadcastAddress[4];
  ARPHandler *arp;
  uint16_t nextPort;

  //large enough to handle ICMP, UDP, TCP
  ipProtocolMap protocolRegistry[3];

  void initProtocolRegistry();
  PacketHandler* getProtocolHandler(uint8_t ipProtocol);

  void init(uint8_t *ipAddress, uint8_t *gatewayIP, uint8_t *subnetMask,
	    ARPHandler *arp, EtherControl *control);

  OffsetBuffer *sendPacketBuffer;

 public:
  IPHandler(uint8_t *ipAddress, uint8_t *gatewayIP, uint8_t *subnetMask,
	    ARPHandler *arp, EtherControl *control); 
  IPHandler(uint8_t *ipAddress, uint8_t *gatewayIP, uint8_t *subnetMask, 
	    uint8_t routingTableSize, EtherControl *control);

  ~IPHandler();

  uint16_t getPort();

  bool registerProtocol(uint8_t ipProtocol, PacketHandler *handler);

  Buffer* getSendPayloadBuffer();


  bool sendPacket(uint8_t *destinationIP, uint8_t protocol,
		  uint16_t packetPayloadLength);
  
  bool sendPacket(uint8_t *destinationIP,uint8_t protocol,
		  uint16_t packetPayloadLength,uint8_t *payload);
  
  void handlePayload(Buffer *p);

  ARPHandler* getARPHandler();
  
  const uint8_t* getMACForIP(uint8_t *destinationIP);

  uint8_t registerTimer(TimerHandler *handler, uint16_t millisDelay);

  void unregisterTimer(uint8_t index);
  
  static bool ipsEquate(uint8_t ip_addr[4], uint8_t ip_addr_compare[4]);

  uint8_t *getIPAddress();

  uint16_t getMaxReceivePayload();

};

#endif
