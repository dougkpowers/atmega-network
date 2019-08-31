#ifndef DNSHANDLER_H
#define DNSHANDLER_H

#include <UDPHandler.h>
#include <TimerHandler.h>

#define NO_ERROR 0x00
#define FORMAT_ERROR 0x01
#define SERVER_FAILURE 0x02
#define NAME_ERROR 0x03
#define NOT_IMPL 0x04
#define REFUSED 0x05
#define CLIENT_ERROR 0x08
#define NO_RESPONSE 0x09
#define PENDING 0x00
#define DONE 0x10
#define INIT 0x20
#define EXPIRED 0x40

typedef struct dnsLookup{
  char* domainName;
  uint8_t ipAddress[4];
  uint8_t status; //first four bits are for lookup status
                  //last four bits are for response code
  uint32_t time;  //in millis
  uint32_t ttl;   //in seconds
  uint8_t attempts;
} dnsLookup;

class DNSHandler: TimerHandler,DatagramReceiver {

  UDPHandler* udpHandler;
  uint8_t cacheCapacity;
  uint8_t cacheSize;
  dnsLookup* cache;
  uint8_t* dnsIP;
  uint8_t* dnsIPBackup;
  uint8_t timer;

  bool sendDNSRequest(const char* domainName, uint16_t id, uint8_t *dnsServer);
  void checkAndSetExpiration(uint8_t i);

 public:
  DNSHandler(UDPHandler* udpHandler, uint8_t* dnsip, uint8_t cacheCapcity,
	     uint8_t* dnsipbackup = NULL);
  uint8_t* resolve(const char* domainName, uint8_t* err, bool force = false);
  void handleTimer(uint8_t index);
  void handleDatagram(uint8_t* sourceIP, uint16_t sourcePort,Buffer *packet);
  void printDNSCache();
};

#endif
