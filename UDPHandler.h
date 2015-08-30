#ifndef UDPHandler_H
#define UDPHandler_H

#include <stdint.h>
#include <IPHandler.h>

#define UDP_PROTOCOL 0x11

class DatagramReceiver{
 public:
  virtual void handleDatagram(uint8_t* sourceIP, uint16_t sourcePort,
			      Buffer *packet) = 0;
}; //end class DatagramReceiver

typedef struct listenerMap{
  uint16_t port;
  DatagramReceiver *receiver;
} listenerMap;


class UDPHandler: PacketHandler{

  IPHandler *ip;
  uint8_t receiverCount;
  listenerMap *receivers;
  OffsetBuffer *sendPayloadBuffer;

  uint32_t calcChecksum(Buffer* buf, uint16_t len, uint8_t* remoteIP);

 public:
  UDPHandler(IPHandler *ipHandler, uint8_t receiverCount);
  ~UDPHandler();

  void handlePacket(uint8_t* sourceIP, Buffer *packet);
  
  bool registerListener(uint16_t port, DatagramReceiver *receiver);
  DatagramReceiver* getListener(uint16_t port);
  void unregisterListener(uint16_t port);
  
  Buffer* getSendPayloadBuffer();
  
  bool sendDatagram(uint8_t *destinationIP, uint16_t destinationPort, 
		    uint16_t sourcePort, uint16_t payloadLength);
  
  bool sendDatagram(uint8_t *destinationIP,uint16_t destinationPort,
		    uint16_t sourcePort, uint16_t payloadLength,uint8_t *payload);
  
  bool sendDatagram(uint8_t *destinationIP, uint16_t destinationPort, 
		    uint16_t sourcePort, char* message);

  bool sendDatagram(uint8_t *destinationIP, uint16_t destinationPort, 
		    uint16_t payloadLength);
  
  bool sendDatagram(uint8_t *destinationIP,uint16_t destinationPort,
		    uint16_t payloadLength,uint8_t *payload);
  
  bool sendDatagram(uint8_t *destinationIP, uint16_t destinationPort, 
		    char* message);

  IPHandler* getIPHandler();
};

#endif
