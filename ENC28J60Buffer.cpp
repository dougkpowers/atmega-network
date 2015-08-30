/*
 *  The ENC28J60Buffer class provides direct access to the memory
 *  buffer on the enc28j60 ethernet controller.  The controller has
 *  8K of RAM addressable from 0x0000 through 0x1FFF.
 *
 *  If the wrap option is enabled and the caller attempts to access
 *  a buffer beyond the endingAddress, then then this class will
 *  automatically wrap around to the startAddress.
 */
#include <stdint.h>
#include "ENC28J60Buffer.h"

ENC28J60Buffer::ENC28J60Buffer(ENC28J60Driver* driver,
			       uint16_t startAddress,
			       uint16_t endAddress,
			       uint16_t len,
			       uint16_t payloadPointer,
			       bool wrap){
  this->driver = driver;
  this->bufferStart = startAddress;
  this->bufferEnd   = endAddress;
  this->wrap = wrap;
  this->payloadPointer = payloadPointer;
  this->len = len;
}

uint16_t ENC28J60Buffer::size(){
  return len;
}

#include <stdio.h>
bool ENC28J60Buffer::write(uint16_t offset, void* data, uint16_t len){

  uint16_t address = this->bufferStart+this->payloadPointer+offset;
  
  //if the offset address is beyond the ending address
  if (address > this->bufferEnd){
    if (!wrap) 
      return false;
    else
      address = (address-this->bufferEnd)+this->bufferStart-1;
  }

  return driver->write(address, (uint8_t*)data, len);  
}

bool ENC28J60Buffer::read(uint16_t offset, void* data, uint16_t len){
  uint16_t address = this->bufferStart+this->payloadPointer+offset;
  
  //if the offset address is beyond the ending address
  if (address > this->bufferEnd){
    if (!wrap) 
      return false;
    else
      address = (address-this->bufferEnd)+this->bufferStart-1;
  }
  
  return driver->read(address, (uint8_t*)data, len);
}

uint8_t ENC28J60Buffer::getBufferType() { return ENC28J60Buffer::BufferType; }

void ENC28J60Buffer::setPayloadPointer(uint16_t offset){
  this->payloadPointer = offset;
}

bool ENC28J60Buffer::copyTo(Buffer* dest,uint16_t dest_start, 
			    uint16_t src_start, uint16_t len){

  if (len == 0)
    len = size();

  //handle the case that the destination is an OffsetBuffer
  //by delegating to that class specifically
  if (dest->getBufferType() == OffsetBuffer::BufferType){
    OffsetBuffer* destOffBuf = static_cast<OffsetBuffer*>(dest);
    return destOffBuf->copyFrom(this,dest_start,src_start,len);
  }

  //handle the case that the destination buffer is an ENC28J60Buffer
  if (dest->getBufferType() == ENC28J60Buffer::BufferType){

    ENC28J60Buffer* deb = static_cast<ENC28J60Buffer*>(dest);

    uint16_t src_address = this->bufferStart+this->payloadPointer+src_start;
    uint16_t dst_address = deb->bufferStart+deb->payloadPointer+dest_start;
    
    //if the src_address is beyond the ending address
    //and we support wrapping, then find the correct src_address
    if (src_address > this->bufferEnd && this->wrap)
      src_address = (src_address-this->bufferEnd)+this->bufferStart-1;
    
    //now if the src_address plus the length would exceed the 
    //end of the src buffer and we are not wrapping then
    //return false to indicate a failure of the copy
    if (src_address + len - 1 > this->bufferEnd && !this->wrap) return false;
    
    //regardless of the wrap, if the len is greater than
    //the length of the buffer, then return false;
    if (len > this->size()) return false;
    
    //if the copy would exceed the end of the destination buffer
    //then return false to indicate a failure of the copy
    if (dst_address + len - 1 > deb->bufferEnd) return false;
    
    return driver->copy(src_address,dst_address,len);
  }

  //else handle this like a normal buffer
  return Buffer::copyTo(dest,dest_start,src_start,len);
}

bool ENC28J60Buffer::copyFrom(Buffer* source, uint16_t dest_start,
			      uint16_t src_start, uint16_t len){
  return source->copyTo(this,dest_start,src_start,len);
}//end copyFrom
