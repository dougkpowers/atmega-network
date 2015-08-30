//
// A class for use in Arduino applications that manages
// application flow control as the result of inbound Ethernet
// frames.  
//
// This class API provides for a generic way to transmit 
// ethernet frames, and receive ethernet frames.
//
// Further, this class can provide for application flow control
// in a couple of ways:
//   (1) Protocol handlers may be registered for various
//       EtherTypes.  When an ethernet frame arrives with a 
//       particular EtherType, this class simply delegates
//       processing of the frame's payload to the protocol
//       handler for the given EtherType.  While 
//       word receiveFrame(etherFrame *frame) provides the
//       caller with the raw ethernet frame, processFrame()
//       receives the frame and calls the associated protocol
//       handler for the frame's etherType.
//   (2) Timers may be registered with this class.  Register
//       a timer by simply calling
//       byte registerTimer(TimerHandler *handler, uint16_t millisDelay).
//       During the processFrame() function, if more than 'millis' 
//       milliseconds have elapsed, the timerHandler function is
//       invoked.  This will continue every 'millis' milliseconds
//       until the timer is unregistered.  The register timer
//       function returns a byte reprenting the timer id.  Call
//       unregisterTimer(byte) to disable and remove the timer.
//
// 2013-10-01 <doug@powersline.com>

#ifndef ETHERCONTROL_H
#define ETHERCONTROL_H

#include <stdlib.h>
#include <EthernetDriver.h>
#include <TimerHandler.h>
#include <Buffer.h>
#include <OffsetBuffer.h>

const uint8_t broadcastMAC[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
 
class PayloadHandler {

 public:
  virtual void handlePayload(Buffer *payload) = 0;

};

typedef struct protocolMap {
  uint16_t etherType;
  PayloadHandler *handler;
} protocolMap;

typedef struct timer {
  uint32_t delayTime;
  uint32_t startTime;
  TimerHandler *handler;
} timer;


class EtherControl {
  
  //just need enough room for ARP and IP
  protocolMap* protocolRegistry;

  //the size here is arbitrary
  //ARP uses exactly one so long as there is an active request
  //DNS uses exactly one so long as there is something in the table
  //TCP uses exactly one so long as a Socket is open
  timer* timerRegistry;
  int timers;
  
  EthernetDriver* driver;
  OffsetBuffer *sendPayloadBuffer;

  void initProtocolRegistry();
  void initTimerRegistry();
  PayloadHandler* getProtocolHandler(uint16_t etherType);
  void processTimers();

  uint8_t protocolCapacity;
  uint8_t timerCapacity;

 public:
  EtherControl (EthernetDriver* driver,
		uint8_t protocolCapacity = 2,
		uint8_t timerCapacity = 3);

  ~EtherControl();

  void initialize ();
  bool sendFrame(const uint8_t *destinationMAC, uint16_t protocol, 
		 uint16_t length);
  bool sendFrame(const uint8_t *destinationMAC, 
		 uint16_t protocol, uint16_t length, uint8_t *payload);
  bool processFrame();
  Buffer* getSendPayloadBuffer();

  bool registerProtocol(uint16_t etherType, PayloadHandler *handler);
  uint8_t registerTimer(TimerHandler *handler, uint16_t millisDelay);
  void unregisterTimer(uint8_t index);

  uint8_t *getMACAddress();

  //the number of octects the EtherNet controller is capable of receiving
  //for higher level protocols.  This excludes the size of the eth header
  uint16_t getMaxReceivePayload();

  static bool macsEquate(uint8_t mac_addr[6], uint8_t mac_addr_compare[6]);
};

#endif
