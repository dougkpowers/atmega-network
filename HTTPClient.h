#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H

#include <Socket.h>

class HTTPClient: public Socket{
  
  char* path;
  uint32_t bytes;
  uint32_t startTime;

 protected:
  void onEstablished();
  bool onDataReceived(Buffer* buf);
  void onRemoteClosed();
  void onClosed();
  void onReset(bool resetByRemoteHost);

 public:
  HTTPClient(uint8_t* ip, uint16_t port = 80, char* path = "/");
  HTTPClient(char* server, DNSHandler* dns, 
	     uint16_t port = 80, char* path = "/");

};

#endif
