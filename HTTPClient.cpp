#include <hostutil.h>
#include <string.h>
#include "HTTPClient.h"

HTTPClient::HTTPClient(uint8_t* ip, uint16_t port, char* path)
  :Socket(ip,port){
  this->path = path;
}

HTTPClient::HTTPClient(char* server, DNSHandler* dns, 
		       uint16_t port, char* path)
  :Socket(server,port,dns){
  this->path = path;
}

#include <Arduino.h>
void HTTPClient::onEstablished(){
  Serial.println("writing");
  startTime = host_millis();
  Buffer* buf = this->getSendDataBuffer();
  buf->write(0,(void*)"GET ",4);
  buf->write(4,path,strlen(path));
  buf->write(4+strlen(path),(void*)"\n\n",2);
  this->send(4+strlen(path)+2);
}

bool HTTPClient::onDataReceived(Buffer* buf){
  for(int i=0; i<buf->size(); i++){
    char c;
    buf->read(i,&c,1);
    printf("%c",c);
  }
  return true;
}


void HTTPClient::onRemoteClosed(){
  printf("Connection closed by remote host.\n");
  close();
}

void HTTPClient::onClosed(){
  printf("Closed\n");
}

void HTTPClient::onReset(bool resetByRemoteHost){
  if (resetByRemoteHost)
    printf("Connection reset by remote host.\n");
  else
    printf("Connection reset by client.\n");
}
