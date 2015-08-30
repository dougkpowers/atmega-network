#include "BufferedSocket.h"
#include <stdio.h>
#include <string.h>

void BufferedSocket::init(uint16_t bufferCapacity){
  this->recvBuffer = (uint8_t*)malloc(bufferCapacity);
  if (recvBuffer == NULL)
    this->bufferCapacity = 0;
  else
    this->bufferCapacity = bufferCapacity;
  this->bufferSize = 0;
}

BufferedSocket::BufferedSocket(char* server, uint16_t port, DNSHandler* dns, 
			       uint16_t bufferSize) : 
  Socket(server,port,dns){
  init(bufferSize);
}

BufferedSocket::BufferedSocket(uint8_t* remoteIP, uint16_t remotePort, 
			       uint16_t bufferSize): 
  Socket(remoteIP,remotePort){
  init(bufferSize);
}

BufferedSocket::BufferedSocket(uint16_t listenPort, uint16_t bufferSize):
  Socket(listenPort){
  init(bufferSize);
}

BufferedSocket::~BufferedSocket(){
  if (recvBuffer != NULL)
    free(recvBuffer);
}
  
#include <Arduino.h>
bool BufferedSocket::onDataReceived(Buffer* buf){

  if ((this->bufferSize + buf->size()) > this->bufferCapacity)
    return false;

  uint8_t* ptr = recvBuffer+bufferSize;

  if (buf->read(0,ptr,buf->size())){
    bufferSize+=buf->size();
    return true;
  }
  return false;

}

uint16_t BufferedSocket::dataAvailable(){
  return this->bufferSize;
}

uint16_t BufferedSocket::read(void* data, uint16_t length){

  if (length > this->bufferSize)
    length = this->bufferSize;

  memcpy(data,recvBuffer,length);

  if (bufferCapacity > 1)
    memmove(recvBuffer,recvBuffer+length,this->bufferSize-length);
  
  this->bufferSize -= length;

  return length;
}

uint16_t BufferedSocket::getApplicationWindowSize(){
  return this->bufferCapacity - this->bufferSize;
}
