#ifndef ENC28J60DRIVER_H
#define ENC28J60DRIVER_H

#include <EthernetDriver.h>
#include <stdint.h>

class ENC28J60Buffer;
#include "ENC28J60Buffer.h"

class ENC28J60Driver: public EthernetDriver {

  uint8_t Enc28j60Bank;
  int gNextPacketPtr;
  uint8_t selectPin;
  uint8_t revision;
  ENC28J60Buffer *sendBuffer;
  ENC28J60Buffer *recvBuffer;
  ENC28J60Buffer *stashBuffer;

  //helpers
  void initSPI();
  void enableChip();
  void disableChip();
  void xferSPI (uint8_t data);
  uint8_t readOp (uint8_t op, uint8_t address);
  void writeOp (uint8_t op, uint8_t address, uint8_t data);
  void SetBank (uint8_t address);
  uint8_t readRegByte (uint8_t address);
  uint16_t readReg(uint8_t address);
  void writeRegByte (uint8_t address, uint8_t data);
  void writeReg(uint8_t address, uint16_t data);
  uint16_t readPhyByte (uint8_t address);
  void writePhy (uint8_t address, uint16_t data);
  void readBuf(uint16_t len, uint8_t* data);
  void writeBuf(uint16_t len, const uint8_t* data);


public:

  ENC28J60Driver(uint8_t* mac,uint8_t csPin = 10);
  
  Buffer* getSendBuffer();
  Buffer* getReceiveBuffer();
  Buffer* getStashBuffer();

  void sendFrame (uint16_t len);
  uint16_t receiveFrame();

  bool isLinkUp ();
  void powerDown();
  void powerUp();

  bool write(uint16_t offset, uint8_t* data, uint16_t length);
  bool read(uint16_t offset, uint8_t* data, uint16_t length);
  bool copy(uint16_t src_addr, uint16_t dest_addr, uint16_t len);
};

#endif
