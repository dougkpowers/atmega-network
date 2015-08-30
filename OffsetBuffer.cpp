#include <stdlib.h>
#include "OffsetBuffer.h"

OffsetBuffer::OffsetBuffer(Buffer *buffer,uint16_t offset, uint16_t length){
  reinit(buffer,offset,length);
}

bool OffsetBuffer::reinit(Buffer *buffer,uint16_t offset, uint16_t length){

  bool retval = true;

  //configure the offset safely
  if (offset > buffer->size()){
    offset = buffer->size();
    retval = false;
  }
  this->offset = offset;

  //set the inner buffer
  this->innerBuffer = buffer;

  //set the length safely
  this->bufferLength = buffer->size() - offset; //default
  if (length > 0){ //if we have a custom length
    if (length < this->bufferLength) //it must be shorter than the default
      this->bufferLength = length;
    else //otherwise, bad config
      retval = false;
  }

  return (retval);
}

uint16_t OffsetBuffer::size(){
  return this->bufferLength;
}

Buffer* OffsetBuffer::getRootBuffer(){
  OffsetBuffer* b = this;
  while (b->innerBuffer->getBufferType() == OffsetBuffer::BufferType)
    b = static_cast<OffsetBuffer*>(b->innerBuffer);

  return b->innerBuffer;
}

uint16_t OffsetBuffer::getRootBufferOffset(){

  OffsetBuffer* b = this;
  uint16_t offset = b->offset;
  while (b->innerBuffer->getBufferType() == OffsetBuffer::BufferType){
    b = static_cast<OffsetBuffer*>(b->innerBuffer);
    offset += b->offset;
  }

  return offset;
}

#include <Arduino.h>
bool OffsetBuffer::write(uint16_t start, void* data, uint16_t len){

  //make sure we are within the bounds of the offset buffer
  if (start+len > this->size()) return false;

  return this->innerBuffer->write(this->offset+start,data,len);
}

bool OffsetBuffer::read(uint16_t start, void* data, uint16_t len){
  //make sure we are within the bounds of the offset buffer
  if (start+len > this->size()) return false;

  return this->innerBuffer->read(this->offset+start,data,len);
}

uint8_t OffsetBuffer::getBufferType() { return OffsetBuffer::BufferType; }

bool OffsetBuffer::copyTo(Buffer* destination,uint16_t dest_start, 
			  uint16_t src_start, uint16_t len){

  if (len == 0)
    len = size();

  //make sure we are within the bounds of the offset buffer
  if (len + src_start > this->size()) return false;
  //if we are copying to an offset buffer, handle that special case
  if (destination->getBufferType() == this->getBufferType()){
    OffsetBuffer* destOffBuff = static_cast<OffsetBuffer*>(destination);

    //make sure we are within the bounds of the offset buffers
    if (len + dest_start > destination->size()) return false;

    return this->getRootBuffer()->
      copyTo(destOffBuff->getRootBuffer(),
	     dest_start + destOffBuff->getRootBufferOffset(),
	     src_start + this->getRootBufferOffset(),
	     len);
  }
  
  //otherwise...

  return this->getRootBuffer()->copyTo(destination,dest_start,
				       src_start + this->getRootBufferOffset(),
				       len);
}//end copyTo

bool OffsetBuffer::copyFrom(Buffer* source, uint16_t dest_start,
		      uint16_t src_start, uint16_t len){
  return source->copyTo(this,dest_start,src_start,len);
}//end copyFrom
