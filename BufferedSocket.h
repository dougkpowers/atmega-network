#ifndef BUFSOCKET_H
#define BUFSOCKET_H

#include <Socket.h>

class BufferedSocket: public Socket{
  
  uint8_t* recvBuffer;
  uint16_t bufferSize;
  uint16_t bufferCapacity;

  void init(uint16_t bufferSize);

 protected:

  bool onDataReceived(Buffer* buf);

 public:
  BufferedSocket(char* server, uint16_t port, DNSHandler* dns, 
		 uint16_t bufferSize);
  BufferedSocket(uint8_t* remoteIP, uint16_t remotePort, uint16_t bufferSize);
  BufferedSocket(uint16_t listenPort, uint16_t bufferSize);
  ~BufferedSocket();

  uint16_t dataAvailable();
  uint16_t read(void* data, uint16_t length);

  uint16_t getApplicationWindowSize();
};

#endif
