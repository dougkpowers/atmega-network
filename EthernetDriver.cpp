#include "EthernetDriver.h"
#include <string.h>

EthernetDriver::EthernetDriver(uint8_t* mac){
  memcpy(this->macAddr,mac,6);
}

uint8_t* EthernetDriver::getMACAddr(){
  return this->macAddr;
}
