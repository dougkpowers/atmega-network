#include <hostutil.h>
#include <string.h>
#include "Buffer.h"

bool Buffer::write(uint16_t offset, const char* d){
  return this->write(offset,d,strlen(d));
}

bool Buffer::write8(uint16_t offset, uint8_t d){ 
  return this->write(offset,&d,1); 
}

bool Buffer::write16(uint16_t offset, uint16_t d){ 
  return this->write(offset,(uint8_t*)&d,2); 
}

bool Buffer::write32(uint16_t offset, uint32_t d){
  return this->write(offset,(uint8_t*)&d,4); 
}

bool Buffer::writeNet16(uint16_t offset, uint16_t d){ 
  return write16(offset,HTONS(d));
}

bool Buffer::writeNet32(uint32_t offset, uint32_t d){
  return write32(offset,HTONL(d));
}

bool Buffer::read8(uint16_t offset, uint8_t* data){
  return read(offset,data,1);
}

bool Buffer::read16(uint16_t offset, uint16_t* data){
  return read(offset,(uint8_t*)data,2);
}

bool Buffer::read32(uint16_t offset, uint32_t* data){
  return read(offset,(uint8_t*)data,4);
}

bool Buffer::readNet16(uint16_t offset, uint16_t* data){
  bool success = read16(offset,data);
  if (!success) return false;
  *data = NTOHS(*data);
  return true;
}

bool Buffer::readNet32(uint16_t offset, uint32_t* data){
  bool success = read32(offset,data);
  if (!success) return false;
  *data = NTOHL(*data);
  return true;
}

bool Buffer::copyTo(Buffer* destination, uint16_t dest_start, 
		    uint16_t src_start, uint16_t len){

  if (len == 0){
    len = size();
    if (destination->size() < len) len = destination->size();
  }

  if (src_start + len > size()) return false;
  if (dest_start + len > destination->size()) return false; 

  for(uint16_t offset = 0; offset < len; offset++){
    uint8_t d;
    if (!this->read8(src_start+offset,&d)) return false;
    if (!destination->write8(dest_start+offset,d)) return false;
  }//end for

  return true;
}//end copyTo

bool Buffer::copyFrom(Buffer* source, uint16_t dest_start,
		      uint16_t src_start, uint16_t len){
  return source->copyTo(this,dest_start,src_start,len);
}//end copyFrom

uint16_t Buffer::checksum(uint16_t len, 
			  uint16_t checksum_offset,
			  uint32_t pseudo){

  uint32_t sum = pseudo;  

  if (len > this->size()) len = this->size();
  
  for(int i=0; i < len; i+=2){
    
    //the checksum is stored at offset 16, so we will skip it
    if (i==checksum_offset) continue;

    uint16_t value = 0x0000;
    if (i+1 < len)
      this->readNet16(i,&value);
    else{
      uint8_t v8;
      this->read8(i,&v8);
      value = (v8 & 0x00FF);
      value = NTOHS(value);
    }
    sum += value;
  }
  
  //The sum is two 16 bit words
  //keep summing them together until the sum is 16 bits or less
  while (sum >> 16)
    sum =  (sum & 0xFFFF) + (sum >> 16);

  //compute the 1's complement of the result and that's the checksum
  sum =  ~sum;  
  
  return (uint16_t) sum;
}
