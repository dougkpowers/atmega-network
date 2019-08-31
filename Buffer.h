#ifndef BUFFER_H
#define BUFFER_H

#include <stdint.h>

class Buffer {
  
 public:
  virtual uint16_t size() = 0;
  
  virtual bool write(uint16_t offset, const void* data, uint16_t len) = 0;
  virtual bool read(uint16_t offset, void* data, uint16_t len) = 0;

  virtual bool copyTo(Buffer* destination, uint16_t dest_start = 0, 
	      uint16_t src_start = 0, uint16_t len = 0);
  virtual bool copyFrom(Buffer* source, uint16_t start = 0,
		uint16_t src_start = 0, uint16_t len = 0);
  
  bool read8(uint16_t offset, uint8_t* data);
  bool read16(uint16_t offset, uint16_t* data);
  bool read32(uint16_t offset, uint32_t* data);

  bool readNet16(uint16_t offset, uint16_t* data);
  bool readNet32(uint16_t offset, uint32_t* data);
  
  bool write8(uint16_t offset, uint8_t d);
  bool write16(uint16_t offset, uint16_t d);
  bool write32(uint16_t offset, uint32_t d); 
  bool write(uint16_t offset, const char* d);

  bool writeNet16(uint16_t offset, uint16_t d);
  bool writeNet32(uint32_t offset, uint32_t d);

  uint16_t checksum(uint16_t len, uint16_t checksum_offset, 
		    uint32_t pseudo = 0);

  //compiling GCC with -fno-rtti will remove
  //getType() from C++ classes and will disable
  //the ability to do a dynamic_cast.
  //
  //for our copyTo functionality, we need to be able to
  //determine the type of Buffer we are dealing with
  //so that sub-classes can handle special cases of
  //copying between buffers of different types
  virtual uint8_t getBufferType() = 0;
};

#endif

