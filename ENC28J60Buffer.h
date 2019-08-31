#ifndef ENC28J60BUFFER_H
#define ENC28J60BUFFER_H

#include <stdint.h>
#include <OffsetBuffer.h>

class ENC28J60Driver;
#include "ENC28J60Driver.h"

class ENC28J60Buffer: public Buffer {
  
  ENC28J60Driver* driver;
  uint16_t bufferStart;
  uint16_t bufferEnd;
  uint16_t payloadPointer;
  uint16_t len;
  bool wrap;

 public:
  ENC28J60Buffer(ENC28J60Driver* driver,
		 uint16_t startAddress,
		 uint16_t endAddress,
		 uint16_t len,
		 uint16_t payloadPointer = 0,
		 bool wrap = false);
  
  uint16_t size();
  bool write(uint16_t offset, const void* data, uint16_t len);
  bool read(uint16_t offset, void* data, uint16_t len);

  void setPayloadPointer(uint16_t offset);

  bool copyTo(Buffer* dest,uint16_t dest_start=0, 
	      uint16_t src_start = 0, uint16_t len = 0);
  
  bool copyFrom(Buffer* destination,
		uint16_t dest_start=0, 
		uint16_t src_start = 0, 
		uint16_t len = 0);
  
  uint8_t getBufferType();
  const static uint8_t BufferType = 3;
};

#endif

