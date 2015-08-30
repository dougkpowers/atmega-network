#ifndef TCPHandler_H
#define TCPHandler_H

#include <stdint.h>
#include <IPHandler.h>

class Socket;
#include "Socket.h"

#define TCP_PROTOCOL 0x06
#define TCP_HEADER_LENGTH 20


typedef struct registeredSocket {
  Socket* socket;
  Buffer* stashBuffer;  
} registeredSocket;

class TCPHandler: PacketHandler, TimerHandler{

  IPHandler *ip;
  registeredSocket* registeredSockets;
  uint8_t socketCapacity;
  uint8_t timerIndex;
  uint16_t maxOutboundLen;

 public:
  TCPHandler(IPHandler *ipHandler, uint8_t socketCapacity, 
	     Buffer* outboundBuffer);
  ~TCPHandler();

  void handlePacket(uint8_t* sourceIP, Buffer *packet);
  void handleTimer(uint8_t index);

  IPHandler* getIPHandler();

  bool registerSocket(Socket* socket);
  void unregisterSocket(Socket* socket);

  //the size of the stash implies the total amount of data that
  //can be sent per packet
  Buffer* getStash(Socket* socket);
  
  //the max amount of tcp data we can receive excluding ip and tcp headers
  uint16_t getMaxSegmentSize();
};

#endif
