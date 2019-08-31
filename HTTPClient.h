#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H

#include <Socket.h>

class HTTPClient: public Socket{
  
  const char* path;
  const char* method;
  const char* body;
  const char* contentType;
  uint32_t bytes;
  bool sendInProgress;

 protected:
  void onEstablished();
  bool onDataReceived(Buffer* buf);
  void onRemoteClosed();
  void onClosed();
  void onReset(bool resetByRemoteHost);

 public:
  HTTPClient(uint8_t* ip, uint16_t port = 80);
  HTTPClient(const char* server, DNSHandler* dns, uint16_t port = 80);
  bool send(const char* method, const char* path, 
            const char* contentType, const char* body);
  bool readyToSend();
};

#endif
