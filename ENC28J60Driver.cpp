/*
 * This is an EthernetDriver implementation for the ENC28J60.
 * The datasheet for the controller may be found here:
 *    http://ww1.microchip.com/downloads/en/DeviceDoc/39662c.pdf
 *
 * Additional erratas and other production information can be found here:
 *    http://www.microchip.com/wwwproducts/Devices.aspx?dDocName=en022889
 *
 * Incredible amounts of credit go to Guido Socher who first wrote an
 * ENC28J60 interface driver for the Arduino.  This driver is largely
 * based on his implementation.
 * 
 * In turn, his implementation was based on the enc28j60.c file
 * from the AVRlib library by Pascal Strong.  For AVRlib see
 * http://www.procyoneengineering.com/
 *
 * Copyright: GPLv2
 *
 * Doug Powers
 * doug@powersline.com
 * 2013-11-01
 */

#include "ENC28J60Driver.h"
#include "ENC28J60Registers.h"

#if ARDUINO >= 100
#include <Arduino.h> // Arduino 1.0
#else
#include <Wprogram.h> // Arduino 0022
#endif

// The RXSTART_INIT must be zero. See Rev. B4 Silicon Errata point 5.
// Buffer boundaries applied to internal 8K ram
// the entire available packet buffer space is allocated

#define RXSTART_INIT        0x0000  // start of RX buffer
                                    // room for 2 packets at 1500 MTU (3071)
#define RXSTOP_INIT         0x0BFF  // end of RX buffer
#define TXSTART_INIT        0x0C00  // start of TX buffer, room for 1 packet
#define TXSTOP_INIT         0x11FF  // end of TX buffer 4607 - 3072 = 1535

//scratch area is 3584 bytes
//this is unused space by the driver but may be used
//by the application via the StashBuffer
#define STASH_START_INIT  0x1200  // start of scratch area
#define STASH_STOP_INIT   0x1FFF  // end of scratch area

// max frame length which the conroller will accept:
// (note: maximum ethernet frame length would be 1518)
#define MAX_FRAMELEN      1500        
    
/* ======================================================================= */
/*                            I N I T I A L I Z E                          */
/* ======================================================================= */
ENC28J60Driver::ENC28J60Driver(uint8_t* macaddr, uint8_t csPin): 
  EthernetDriver(macaddr){

  this->sendBuffer = new ENC28J60Buffer(this,TXSTART_INIT+1,
					TXSTOP_INIT,
					MAX_FRAMELEN,
					0,false);
  this->recvBuffer = new ENC28J60Buffer(this,RXSTART_INIT,
					RXSTOP_INIT,
					MAX_FRAMELEN,
					0,true);
  this->stashBuffer = new ENC28J60Buffer(this,STASH_START_INIT,
					 STASH_STOP_INIT,
					 STASH_STOP_INIT - STASH_START_INIT +1,
					 0,false);
					 

  if (bitRead(SPCR, SPE) == 0)
    initSPI();

  selectPin = csPin;  
  pinMode(selectPin, OUTPUT);
  disableChip();

  writeOp(ENC28J60_SOFT_RESET, 0, ENC28J60_SOFT_RESET);
  delay(20); // errata B7/2

  while (!readOp(ENC28J60_READ_CTRL_REG, ESTAT) & ESTAT_CLKRDY)
    ;
  
  gNextPacketPtr = RXSTART_INIT;
  writeReg(ERXST, RXSTART_INIT);
  writeReg(ERXRDPT, RXSTART_INIT);
  writeReg(ERXND, RXSTOP_INIT);
  writeReg(ETXST, TXSTART_INIT);
  writeReg(ETXND, TXSTOP_INIT);
  
  writeRegByte(ERXFCON, ERXFCON_UCEN|ERXFCON_CRCEN|ERXFCON_PMEN|ERXFCON_BCEN);

  writeReg(EPMM0, 0x303f);
  writeReg(EPMCS, 0xf7f9);
  writeRegByte(MACON1, MACON1_MARXEN|MACON1_TXPAUS|MACON1_RXPAUS);
  writeRegByte(MACON2, 0x00);
  writeOp(ENC28J60_BIT_FIELD_SET, MACON3,
	  MACON3_PADCFG0|MACON3_TXCRCEN|MACON3_FRMLNEN);
  writeReg(MAIPG, 0x0C12);
  writeRegByte(MABBIPG, 0x12);
  writeReg(MAMXFL, MAX_FRAMELEN);  
  writeRegByte(MAADR5, macaddr[0]);
  writeRegByte(MAADR4, macaddr[1]);
  writeRegByte(MAADR3, macaddr[2]);
  writeRegByte(MAADR2, macaddr[3]);
  writeRegByte(MAADR1, macaddr[4]);
  writeRegByte(MAADR0, macaddr[5]);
  writePhy(PHCON2, PHCON2_HDLDIS);
  SetBank(ECON1);
  writeOp(ENC28J60_BIT_FIELD_SET, EIE, EIE_INTIE|EIE_PKTIE);
  writeOp(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_RXEN);
  
  this->revision = readRegByte(EREVID);

  // microchip forgot to step the number on the silcon when they
  // released the revision B7. 6 is now rev B7. We still have
  // to see what they do when they release B8. At the moment
  // there is no B8 out yet
  if (this->revision > 5) this->revision++;
}


/* ======================================================================= */
/*                        P R I V A T E    H E L P E R S                   */
/* ======================================================================= */

void ENC28J60Driver::initSPI () {
    pinMode(SS, OUTPUT);
    digitalWrite(SS, HIGH);
    pinMode(MOSI, OUTPUT);
    pinMode(SCK, OUTPUT);   
    pinMode(MISO, INPUT);
    
    digitalWrite(MOSI, HIGH);
    digitalWrite(MOSI, LOW);
    digitalWrite(SCK, LOW);

    SPCR = bit(SPE) | bit(MSTR); // 8 MHz @ 16
    bitSet(SPSR, SPI2X);
}

void ENC28J60Driver::enableChip () {
    cli();
    digitalWrite(selectPin, LOW);
}

void ENC28J60Driver::disableChip () {
    digitalWrite(selectPin, HIGH);
    sei();
}

void ENC28J60Driver::xferSPI (uint8_t data) {
    SPDR = data;
    while (!(SPSR&(1<<SPIF)))
        ;
}

byte ENC28J60Driver::readOp (uint8_t op, uint8_t address) {
    enableChip();
    xferSPI(op | (address & ADDR_MASK));
    xferSPI(0x00);
    if (address & 0x80)
        xferSPI(0x00);
    uint8_t result = SPDR;
    disableChip();
    return result;
}


void ENC28J60Driver::writeOp (uint8_t op, uint8_t address, uint8_t data) {
    enableChip();
    xferSPI(op | (address & ADDR_MASK));
    xferSPI(data);
    disableChip();
}

void ENC28J60Driver::SetBank (uint8_t address) {
    if ((address & BANK_MASK) != Enc28j60Bank) {
        writeOp(ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_BSEL1|ECON1_BSEL0);
        Enc28j60Bank = address & BANK_MASK;
        writeOp(ENC28J60_BIT_FIELD_SET, ECON1, Enc28j60Bank>>5);
    }
}

uint8_t ENC28J60Driver::readRegByte (uint8_t address) {
    SetBank(address);
    return readOp(ENC28J60_READ_CTRL_REG, address);
}

uint16_t ENC28J60Driver::readReg(uint8_t address) {
	return readRegByte(address) + (readRegByte(address+1) << 8);
}

void ENC28J60Driver::writeRegByte (uint8_t address, uint8_t data) {
    SetBank(address);
    writeOp(ENC28J60_WRITE_CTRL_REG, address, data);
}

void ENC28J60Driver::writeReg(uint8_t address, uint16_t data) {
    writeRegByte(address, data);
    writeRegByte(address + 1, data >> 8);
}

uint16_t ENC28J60Driver::readPhyByte (uint8_t address) {
    writeRegByte(MIREGADR, address);
    writeRegByte(MICMD, MICMD_MIIRD);
    while (readRegByte(MISTAT) & MISTAT_BUSY)
        ;
    writeRegByte(MICMD, 0x00);
    return readRegByte(MIRD+1);
}

void ENC28J60Driver::writePhy (uint8_t address, uint16_t data) {
    writeRegByte(MIREGADR, address);
    writeReg(MIWR, data);
    while (readRegByte(MISTAT) & MISTAT_BUSY)
        ;
}

/* ======================================================================= */
/*                 D A T A      B U F F E R     A C C E S S                */
/* ======================================================================= */

Buffer* ENC28J60Driver::getSendBuffer(){ return sendBuffer;  }
Buffer* ENC28J60Driver::getReceiveBuffer(){ return recvBuffer;  }
Buffer* ENC28J60Driver::getStashBuffer(){ return stashBuffer;  }

void ENC28J60Driver::readBuf(uint16_t len, uint8_t* data) {
    enableChip();
    xferSPI(ENC28J60_READ_BUF_MEM);
    while (len--) {
        xferSPI(0x00);
        *data++ = SPDR;
    }
    disableChip();
}

void ENC28J60Driver::writeBuf(uint16_t len, const uint8_t* data) {
    enableChip();
    xferSPI(ENC28J60_WRITE_BUF_MEM); //send spi command for write mem
    while (len--)
        xferSPI(*data++);
    disableChip();
}


bool ENC28J60Driver::write(uint16_t offset, uint8_t* data, uint16_t len){
  
  writeReg(EWRPT, offset);     //where will we start to write
  writeBuf(len,data);
  return true;
}

bool ENC28J60Driver::read(uint16_t offset, uint8_t* data, uint16_t len){

  writeReg(ERDPT, offset);     //where will we start to read
  readBuf(len,data);
  return true;
}

bool ENC28J60Driver::copy(uint16_t src_addr_start, 
			  uint16_t dest_addr, uint16_t len){

  //sanity check the dest_addr and src_addr_start
  if (dest_addr > 0x1FFF) return false;
  if (src_addr_start > 0x1FFF) return false;

  //we should never write to the receive buffer
  if (dest_addr >= RXSTART_INIT && dest_addr <= RXSTOP_INIT)
    return false;

  //set the address at which we stop copying
  uint16_t src_addr_end = src_addr_start + len;
  
  //if we are starting in the recv buffer, then the internal pointer
  //will loop around to the beginning of the receive buffer, so 
  //determining the end pointer is a special case
  if (src_addr_start >= RXSTART_INIT && src_addr_start <= RXSTOP_INIT){
    if (src_addr_end > RXSTOP_INIT)
      src_addr_end = src_addr_end - RXSTOP_INIT + RXSTART_INIT - 1;
    //if the src_addr_end is still beyond RXSTOP_INIT return false;
    if (src_addr_end > RXSTOP_INIT) return false;
  }
  
  
  //if the src_addr_end is beyond the end of total memory return false
  if (src_addr_end > 0x1FFF) return false;

  //make sure the dest would not rap past 8191 (0x1FFF)
  //which is the end of our memory buffer on the controller
  if (dest_addr+len > 0x1FFF) return false;

  //make sure a DMA is not in progress
  if(readOp(ENC28J60_READ_CTRL_REG, ECON1) & ECON1_DMAST)
    return false;

  //clear the DMAST bit and the CSUMEN bit
  writeOp(ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_DMAST | ECON1_CSUMEN);

  //set our read start and end pointers
  writeReg(EDMAST, src_addr_start);
  writeReg(EDMAND, src_addr_end);

  //set our write start pointer
  //there is no write end pointer as it is implied
  writeReg(EDMADST, dest_addr);

  //setting the DMAST bit on the ECON1 register starts the DMA copy
  writeOp(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_DMAST);

  //wait for copy to finish
  //the controller will clear this bit when done
  while(readOp(ENC28J60_READ_CTRL_REG, ECON1) & ECON1_DMAST)
    ;

  return true;
}

/* ======================================================================= */
/*                    S E N D      A N D      R E C E I V E                */
/* ======================================================================= */

void ENC28J60Driver::sendFrame(uint16_t len) {

  //while we're current transmitting, simply wait
  while (readOp(ENC28J60_READ_CTRL_REG, ECON1) & ECON1_TXRTS){
    //but if there was a tx error
    if (readRegByte(EIR) & EIR_TXERIF) { 
      //reset all transmission logic
      writeOp(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_TXRST);
      writeOp(ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_TXRST);
    }
  }

  //set the packet end
  writeReg(ETXND, TXSTART_INIT+len);

  //write the control byte (which is always 0x00 in our case)
  writeReg(EWRPT, TXSTART_INIT); 
  writeOp(ENC28J60_WRITE_BUF_MEM, 0, 0x00);

  //start a transmission
  writeOp(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_TXRTS);
}


uint16_t ENC28J60Driver::receiveFrame() {
    uint16_t len = 0;
    //if we have more than zero packets in the buffer
    if (readRegByte(EPKTCNT) > 0) {
      writeReg(ERDPT, gNextPacketPtr);
      
      //read in the header so we know what we're looking at
      struct {
	uint16_t nextPacket;
	uint16_t byteCount;
	uint16_t status;
      } header;      
      readBuf(sizeof header, (uint8_t*) &header);

      //ERDPT will have automatically advanced since we've 
      //read the header, so we can set our offset value
      //to the new value of ERDPT
      uint16_t offset = readReg(ERDPT);
      recvBuffer->setPayloadPointer(offset);
      
      gNextPacketPtr  = header.nextPacket;
      len = header.byteCount - 4; //remove the CRC count
      if (len > getReceiveBuffer()->size())
	len = getReceiveBuffer()->size();
      if ((header.status & 0x80) == 0)
	len = 0;

      //the controller can write up to the beginning of our 
      //offset value.  Anything after this is off limits
      //until we call receiveFrame again
      writeReg(ERXRDPT, offset);

      writeOp(ENC28J60_BIT_FIELD_SET, ECON2, ECON2_PKTDEC);

    }

    return len;
}

/* ======================================================================= */
/*                      P O W E R     M A N A G E M E N T                  */
/* ======================================================================= */
bool ENC28J60Driver::isLinkUp() {
    return (readPhyByte(PHSTAT2) >> 2) & 1;
}

/* 
 * Contributed by Alex M. 
 * Based on code from: 
 *       http://blog.derouineau.fr/2011/07/
 *       putting-enc28j60-ethernet-controler-in-sleep-mode/
 */
void ENC28J60Driver::powerDown() {
  writeOp(ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_RXEN);
  while(readRegByte(ESTAT) & ESTAT_RXBUSY);
  while(readRegByte(ECON1) & ECON1_TXRTS);
  writeOp(ENC28J60_BIT_FIELD_SET, ECON2, ECON2_VRPS);
  writeOp(ENC28J60_BIT_FIELD_SET, ECON2, ECON2_PWRSV);
}

void ENC28J60Driver::powerUp() {
  writeOp(ENC28J60_BIT_FIELD_CLR, ECON2, ECON2_PWRSV);
  while(!readRegByte(ESTAT) & ESTAT_CLKRDY);
  writeOp(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_RXEN);
}

