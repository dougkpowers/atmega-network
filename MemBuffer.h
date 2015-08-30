#ifndef MEMBUFFER_H
#define MEMBUFFER_H

#include "Buffer.h"

class MemBuffer: public Buffer {
  
  uint8_t* buffer;
  bool allocMode;
  uint16_t bufferLength;

 public:
  MemBuffer(uint16_t bufferLength);
  MemBuffer(uint16_t bufferLength, uint8_t* buffer);
  ~MemBuffer();

  uint16_t size();

  bool write(uint16_t offset, void* data, uint16_t len);
  bool read(uint16_t offset, void* data, uint16_t len);

  uint8_t getBufferType();
  const static uint8_t BufferType = 1;
};

#endif

