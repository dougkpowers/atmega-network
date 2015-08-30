#include <stdlib.h>
#include <string.h>
#include "MemBuffer.h"

MemBuffer::MemBuffer(uint16_t bufferLength, uint8_t* buffer){
  this->buffer = buffer;
  this->bufferLength = bufferLength;
  this->allocMode = false;
}


MemBuffer::MemBuffer(uint16_t bufferLength){
  uint8_t* buf = (uint8_t*)malloc(bufferLength);
  if (buf == NULL)
    this->bufferLength = 0;
  this->bufferLength = bufferLength;
  this->buffer = buf;
  this->allocMode = true;
}

MemBuffer::~MemBuffer(){
  if (this->allocMode)
    free(this->buffer);
}

uint16_t MemBuffer::size(){
  return this->bufferLength;
}

bool MemBuffer::write(uint16_t offset, void* data, uint16_t len){
  if (offset + len > this->size()) return false;
  memcpy(this->buffer+offset,data,len);
  return true;
}

bool MemBuffer::read(uint16_t offset, void* data, uint16_t len){
  if (offset + len > this->size()) return false;
  memcpy(data,this->buffer+offset,len);
  return true;
}

uint8_t MemBuffer::getBufferType() { return MemBuffer::BufferType; }
