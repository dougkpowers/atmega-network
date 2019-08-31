/*
 *  This abstract class defines the interface that must be implemented
 *  by any driver that wishes to be called by the EthernetController class.
 *
 *  getStashBuffer is used to access a Buffer of unused available memory
 *  on the ethernet device.  If there is no excess memory, this function
 *  may return NULL.
 */
#ifndef ETHERNET_DRIVER_H
#define ETHERNET_DRIVER_H

#include <stdint.h>
#include <Buffer.h>
    
class EthernetDriver {

  uint8_t macAddr[6];

public:

  EthernetDriver(uint8_t* macAddr);
  
  uint8_t* getMACAddr();

  virtual Buffer* getSendBuffer() = 0;
  virtual Buffer* getReceiveBuffer() = 0;
  virtual Buffer* getStashBuffer() = 0;

  virtual void sendFrame (uint16_t len) = 0;
  virtual uint16_t receiveFrame() = 0;

  virtual bool isLinkUp () = 0;
  virtual void powerDown() = 0;
  virtual void powerUp() = 0;
  
};

#endif
