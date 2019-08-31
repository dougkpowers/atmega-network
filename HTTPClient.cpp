#include <hostutil.h>
#include <string.h>
#include "HTTPClient.h"

HTTPClient::HTTPClient(uint8_t* ip, uint16_t port)
  :Socket(ip,port){
  this->sendInProgress = false;
}

HTTPClient::HTTPClient(const char* server, DNSHandler* dns, uint16_t port)
  :Socket(server,port,dns){
  this->sendInProgress = false;
}

#include <Arduino.h>

bool HTTPClient::send(const char* method, const char* path, 
                      const char* contentType,const char* body) {

  if (!this->readyToSend())
    return false;

  this->sendInProgress = true;

  this->method = method;
  this->path = path;
  this->body = body;
  this->contentType = contentType;
  
  this->connect();

  return true;
}

bool HTTPClient::readyToSend(){
  if (this->sendInProgress) return false;
  if (getState() != CLOSED) return false;
  return true;
}

void HTTPClient::onEstablished(){
  Buffer* buf = this->getSendDataBuffer();
  uint16_t pos = 0;
  buf->write(pos,method,strlen(method)); pos += strlen(method);
  buf->write(pos," ",1); pos += 1;
  buf->write(pos,path,strlen(path)); pos += strlen(path);
  buf->write(pos,"\n",1); pos += 1;

  if (body != NULL && strlen(body) > 0) {
    if (contentType != NULL && strlen(contentType) > 0){
       buf->write(pos, "Content-Type: "); pos += 14;
       buf->write(pos, contentType); pos += strlen(contentType);
       buf->write(pos,"\n",1); pos += 1;
    }

    buf->write(pos, "Content-Length: "); pos += 16;
    char contentLength[5];
    itoa(strlen(body), contentLength, 10);
    buf->write(pos,contentLength,strlen(contentLength));
    pos += strlen(contentLength);
    buf->write(pos,"\n",1); pos += 1;
  }
  buf->write(pos,"\n",1); pos += 1;
  if (body != NULL && strlen(body) > 0) {
    buf->write(pos,body,strlen(body));
    pos += strlen(body);
  }
  Socket::send(pos);
}

bool HTTPClient::onDataReceived(Buffer* buf){
  // this client is really just for uploading data
  // therefore, we don't care about the result

#ifdef DEBUG
  uint8_t c;
  uint16_t offset = 0;

  while(buf->read8(offset++,&c))
    printf("%c",(char)c);

#endif
  return true;
}


void HTTPClient::onRemoteClosed(){
  // connection closed by remote host
  this->sendInProgress = false;
  close();
}

void HTTPClient::onClosed(){
  // closed locally

  //we may be in time-wait which would prevent us from
  //being able to do another send despite that a new
  //socket connection will always allocate a different port
  //so let's force ourselves straight to CLOSED                                 
  if (this->sendInProgress)
    forceClose();

  this->sendInProgress = false;
}

void HTTPClient::onReset(bool resetByRemoteHost){
  this->sendInProgress = false;

}
