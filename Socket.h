#ifndef SOCKET_H
#define SOCKET_H

#include <Buffer.h>
#include <OffsetBuffer.h>
#include "DNSHandler.h"

class TCPHandler;
#include "TCPHandler.h"


#define LISTEN 1
#define SYN_SENT 2
#define SYN_RECEIVED 3
#define ESTABLISHED 4
#define FIN_WAIT_1 5
#define FIN_WAIT_2 6
#define CLOSE_WAIT 7
#define CLOSING 8
#define LAST_ACK 9
#define TIME_WAIT 10
#define CLOSED 11
#define RESOLVING 20
#define UNKNOWN_HOST 21

#define READY_TO_SEND 0x00
#define AWAITING_ACK 0xF0

#define CWR 128
#define ECE 64
#define URG 32
#define ACK 16
#define PSH 8
#define RST 4
#define SYN 2
#define FIN 1

//according to RFC793, the MSL (max seg len) should be 2 minutes
//The TIME_WAIT_DURATION must be 2x this value which means
//we will remain in time_wait for up to four minutes.
//the idea is that we won't reuse the same combo of ports and IPs
//for this duration so that we don't confuse the remote host
//if packets are mixed between sessions.
#define TIME_WAIT_DURATION 240

//wait up to 1 second for an ACK.  If we don't get it, resend
#define ACK_WAIT_DURATION 1

class Socket{
  
  uint8_t state;
  uint16_t listenPort;
  uint16_t localPort;

  bool connectOnResolution;
  DNSHandler* dns;

  uint8_t remoteIP[4];
  uint16_t remotePort;
  uint32_t localSeq;
  uint32_t remoteSeq;
  uint16_t remoteMSS;
  uint16_t remoteWindow;
  uint32_t stateTime;  //in milliseconds
  uint32_t timeout; //in milliseconds

  uint8_t attempts;
  TCPHandler* tcp;
  Buffer* stash;
  OffsetBuffer* recvBuffer;
  OffsetBuffer* sendBuffer;
  uint16_t lastDataLength;

  void init();
  
  void setState(uint8_t state);
  void closed();

  bool resendData();

  bool sendSegment(uint8_t control, uint16_t length, uint32_t seq = 0,
		   uint32_t ack = 0);

  bool transmit(uint16_t length, uint8_t option_length = 0);
  uint16_t calcChecksum(Buffer* buf, uint16_t len);


  uint16_t getWindowSize();
  bool reset(uint32_t seq,uint32_t ack);
  bool resolveIP();
 protected:
  char* remoteDomain;
  virtual uint16_t getApplicationWindowSize();


  //events
  virtual void onEstablished();
  virtual bool onDataReceived(Buffer* buf) = 0;
  virtual void onReadyToSend();
  virtual void onRemoteClosed();
  virtual void onLocalClosed();
  virtual void onClosed();
  virtual void onReset(bool resetByRemoteHost);

 public:
  Socket(uint8_t* remoteIP, uint16_t remotePort);
  Socket(char* server, uint16_t remotePort, DNSHandler* dns);
  Socket(uint16_t listenPort);
  ~Socket();
  
  void setTimeout(uint32_t t);

  bool registerTCPHandler(TCPHandler* handler);
  void unregisterTCPHandler();

  bool isHost();
  bool isClient();

  uint16_t getMaxSendPayload();
  Buffer* getSendDataBuffer();

  uint8_t getState();
  uint16_t getStateTime();

  bool localClosed();
  bool remoteClosed();

  uint16_t getLocalPort();

  bool connect();

  bool readyToSend();
  bool send(char* data);
  bool send(uint8_t* data, uint16_t length);
  bool send(uint16_t length);

  uint16_t read(uint8_t* data, uint16_t length, uint16_t offset=0);

  bool close();
  void forceClose();

  void handleSegment(uint8_t* sourceIP, Buffer * buf);
  void checkState();

  bool equals(uint8_t* remoteIP, uint16_t remotePort, uint16_t localPort);
  bool equals(uint16_t listenPort);

};

#endif
