#ifndef OFFSETBUFFER_H
#define OFFSETBUFFER_H

#include <stdint.h>
#include <Buffer.h>

class OffsetBuffer : public Buffer {
  
  uint16_t bufferLength;
  uint16_t offset;
  Buffer *innerBuffer;

 public: 
  Buffer* getRootBuffer();
  uint16_t getRootBufferOffset();

  OffsetBuffer(Buffer *buffer,uint16_t offset, uint16_t length = 0);
  bool reinit(Buffer *buffer,uint16_t offset, uint16_t length = 0);

  uint16_t size();
  bool write(uint16_t offset, void* data, uint16_t len);
  bool read(uint16_t offset, void* data, uint16_t len);

  bool copyTo(Buffer* destination, uint16_t dest_start = 0, 
	      uint16_t src_start = 0, uint16_t len = 0);
  bool copyFrom(Buffer* source, uint16_t start = 0,
		uint16_t src_start = 0, uint16_t len = 0);

  uint8_t getBufferType();
  const static uint8_t BufferType = 2;
};

#endif

