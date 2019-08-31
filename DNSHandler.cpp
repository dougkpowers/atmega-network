/*
 * This library builds on the UDP API by providing DNS 
 * (Domain Name Lookup) capabilities.
 *
 */


#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <hostutil.h>
#include "IPHandler.h"
#include "UDPHandler.h"
#include "DNSHandler.h"

#define DNS_HEADER_LENGTH 12

typedef struct dns_header {
  uint16_t id;
  uint16_t control;
  uint16_t qdcount;
  uint16_t ancount;
  uint16_t nscount;
  uint16_t arcount;
} dns_header;

/* ========================================================================= */
/*                           C O N S T R U C T O R S                         */
/* ========================================================================= */
DNSHandler::DNSHandler(UDPHandler* udpHandler,uint8_t *dnsip,
		       uint8_t cacheCapacity,uint8_t *dnsipbackup){
  this->udpHandler = udpHandler;

  this->dnsIP = dnsip;

  if (dnsipbackup == NULL)
    this->dnsIPBackup = dnsip;
  else
    this->dnsIPBackup = dnsipbackup;

  this->cache = (dnsLookup*)malloc(sizeof(dnsLookup)*cacheCapacity);
  if (this->cache != NULL)
    this->cacheCapacity = cacheCapacity;

  for(int i=0; i<this->cacheCapacity; i++){
    cache[i].domainName[0] = '\0';
    memcpy(cache[i].ipAddress,0,4);
    cache[i].status = INIT | NO_ERROR;
  }

  this->cacheSize = 0;
  this->timer = 0;

  udpHandler->registerListener(53,this);
}//end constructor

/* ========================================================================= */
/*                               P R I V A T E                               */
/* ========================================================================= */
// returns false if the UDP buffer is not large enough for the request
bool DNSHandler::sendDNSRequest(const char* domainName, uint16_t id, 
				uint8_t* dnsServer){

  if (strlen(domainName) > 255){
#ifdef DEBUG
    fprintf(stderr,"Cannot lookup domains longer than 255 characters.\n");
#endif
    return false;
  }

  //get our buffer
  Buffer *udpBuffer = udpHandler->getSendPayloadBuffer();

  //length check
  if (udpBuffer->size() < DNS_HEADER_LENGTH){
#ifdef DEBUG
    fprintf(stderr,"DNS header length exceeds available UDP buffer size.\n");
#endif
    return false;
  }

  //populate the DNS header
  if (!udpBuffer->writeNet16( 0,    id)) return false;  //id
  if (!udpBuffer->writeNet16( 2,0x0100)) return false;  //control:see rfc1035
  if (!udpBuffer->writeNet16( 4,0x0001)) return false;  //qdcount:just 1 lookup
  if (!udpBuffer->writeNet16( 6,0x0000)) return false;  //ancount:0
  if (!udpBuffer->writeNet16( 8,0x0000)) return false;  //nscount:0
  if (!udpBuffer->writeNet16(10,0x0000)) return false;  //arcount:0

  uint16_t length = DNS_HEADER_LENGTH;

  //now fill out the query section, one host at a time
  //www.google.com would be 3 hosts: www, google, and com
  //each host get's one uint8_t for the length of the host
  //followed by the host name
  //
  // ie 3: www
  //    6: google
  //    3: com
  //    0         (note that we end with a 0 length for the root domain)
  uint16_t hostLengthOffset = DNS_HEADER_LENGTH;

  const char* nextChar = domainName;
  uint8_t hostLength = 0;
  while (*nextChar != '\0'){
    if (*nextChar == '.'){//toggle to the next host
      
      //set the length of the prior host
      if (!udpBuffer->write8(hostLengthOffset,hostLength)) return false;

      //jump the hostLength offset ahead to the next host entry
      hostLengthOffset = hostLengthOffset + 1 + hostLength;

      //reset the host length to zero
      hostLength = 0;

      length++;
      nextChar++;
      continue;
    }
    
    if (!udpBuffer->write8(hostLengthOffset+1+hostLength,*nextChar)) 
      return false;

    length++;
    hostLength++;
    nextChar++;

  }//while more chars to go

  //set the length of the prior host
  if (!udpBuffer->write8(hostLengthOffset,hostLength)) return false;
  length++;
  
  //jump the hostLength offset ahead to the next host entry
  hostLengthOffset = hostLengthOffset + 1 + hostLength;
  if (!udpBuffer->write8(hostLengthOffset,0)) return false;
  length++;

  //find the qtype and qclass pointer
  uint16_t qtypeOffset = hostLengthOffset+1;
  uint16_t qclassOffset = hostLengthOffset+1+2;

  length += 4;

  if (!udpBuffer->writeNet16(qtypeOffset,1)) return false; //A record
  if (!udpBuffer->writeNet16(qclassOffset,1)) return false; //internet

  udpHandler->sendDatagram(dnsServer,53,53,length);

  return true;
}//end sendDNSRequest

/* ========================================================================= */
/*                                P U B L I C                                */
/* ========================================================================= */
uint8_t* DNSHandler::resolve(const char* domainName, uint8_t* err, bool force){

  //first, see if this domainName is in our cache
  for(int i=0; i<cacheSize; i++){

    if (strcmp(domainName,cache[i].domainName) == 0){

      //check to see if this entry has expired and set the
      //status to expired if so
      checkAndSetExpiration(i);

      //if we force is set to true then regardless of the state
      //we want to resubmit the request to the DNS server
      //also, if the DNS entry has expired, then we will want
      //to resubmit the request to the DNS server
      if (force || ((cache[i].status & 0xF0) == EXPIRED)){
	cache[i].status = PENDING | NO_ERROR;

	//send the DNS request
	if (sendDNSRequest(domainName,i+1,dnsIP)){
	  //record the time of our request
	  cache[i].time = host_millis();
	  cache[i].attempts = 1;

	  //make sure we have a timer enabled so we can retry if needed
	  if (this->timer == 0){
	    IPHandler* ip = this->udpHandler->getIPHandler();
	    this->timer = ip->registerTimer(this,1000);
	  }
	}//end if we successfully sent a request
	else{ //set our error state
	  cache[i].status == DONE | CLIENT_ERROR;
	}
      }

      *err = cache[i].status & 0x0F; //just take the last 4 bits

      if (cache[i].status == DONE | NO_ERROR)
	return cache[i].ipAddress;
      else
	return NULL;      
    }//end if
  }//end for

  //not in the cache, so we need to initialize a lookup

  //se if we have room in the cache
  if (this->cacheSize >= this->cacheCapacity){
#ifdef DEBUG
    fprintf(stderr,"Cannot send DNS request. DNS cache table at capacity\n");
#endif
    *err = CLIENT_ERROR;
    return NULL;
  }

  //ok, we have space, so let's use the next entry in our cache
  cache[cacheSize].status = NO_ERROR | PENDING;
  *err = cache[cacheSize].status;
  cache[cacheSize].domainName = (char*)malloc(strlen(domainName)+1);
  if (cache[cacheSize].domainName == NULL){
    cache[cacheSize].status = DONE | CLIENT_ERROR;
#ifdef DEBUG
    fprintf(stderr,"Out of memory in DNSHandler.cpp\n");
#endif
    *err = CLIENT_ERROR;
    return NULL;
  }//end if

  memcpy(cache[cacheSize].domainName,domainName,strlen(domainName)+1);
  cache[cacheSize].attempts = 1;

  //send the DNS request
  if(sendDNSRequest(domainName,cacheSize+1,dnsIP)){  

    //record the time of our request
    cache[cacheSize].time = host_millis();
    
    //make sure we have a timer enabled so we can retry if needed
    if (this->timer == 0){
      IPHandler* ip = this->udpHandler->getIPHandler();
      this->timer = ip->registerTimer(this,5000);
    }

  }
  else{
    free(cache[cacheSize].domainName); //forget we were ever here
    *err = CLIENT_ERROR;
  }

  //increment the size of our cache
  cacheSize++;

  //all done, check back later for a response
  return NULL;

}//end resolve

void DNSHandler::handleDatagram(uint8_t* sourceIP, uint16_t sourcePort, 
				Buffer *packet){

  //quick length check
  if (packet->size() < DNS_HEADER_LENGTH)
    return;

  //if the sourceIP is not either of our configured DNS servers
  //then this could be a DNS poisoning attempt; abort!
  if (!IPHandler::ipsEquate(sourceIP,dnsIP) &&
      !IPHandler::ipsEquate(sourceIP,dnsIPBackup))
    return;

  dns_header header;
  if (!packet->readNet16( 0,&header.id))      return;
  if (!packet->readNet16( 2,&header.control)) return;
  if (!packet->readNet16( 4,&header.qdcount)) return;
  if (!packet->readNet16( 6,&header.ancount)) return;
  if (!packet->readNet16( 8,&header.nscount)) return;
  if (!packet->readNet16(10,&header.arcount)) return;
  
  //check to make sure this is a response
  if (header.control >> 15 != 0x01) 
    //the first bit is the QR code
    return; //not a response packet, so just ignore it
  
  //see if the TC code is set.  If so, we have to ignore the response
  if (header.control << 6 >> 15  == 0x01) return;

  //now find the entry in our cache corresponding to the id
  if ((header.id)-1 >= cacheSize ) return; //id is too large
  dnsLookup *cacheEntry = &cache[(header.id)-1];
  if (cacheEntry->status & PENDING != PENDING) 
    return;//if status is not pending, abort


  //set the response code
  uint8_t status = (uint8_t)(header.control<<12>>12);

  cacheEntry->status = DONE | status;
  if (status != NO_ERROR) {
    return; //return if not successful
  }
  
  //set the time we received the IP so that we can set a timer for the ttl
  cacheEntry->time = host_millis();

  //we need at least one answer entry
  if (header.ancount <= 0){
    //this should not happen
    //we should either get a name resolution error
    //or a success with more than 0 answer sections
    cacheEntry->status = DONE | SERVER_FAILURE;
    return; 
  }

  //skip over our question entries
  uint16_t ptrOffset = DNS_HEADER_LENGTH;
  for(int i=0; i<header.qdcount; i++){
    uint8_t size;
    if (!packet->read8(ptrOffset++,&size)) return;
    while (size > 0){
      ptrOffset += size;
      if (!packet->read8(ptrOffset++,&size)) return;
    }
    ptrOffset += 4; //skip the qtype and qclass fields
  }//end each q

  //read our first answer and use that as our reply
  for(int i=0; i<header.ancount; i++){
    uint8_t size;
    if (!packet->read8(ptrOffset++,&size)) return;

    while (size > 0){
      if (size >> 6 == 3){ //we're a pointer
	ptrOffset++; //skip the rest of the offset
	break; //after a pointer, we're done
      }
      else{
	ptrOffset += size;
	if (!packet->read8(ptrOffset++,&size)) return;
      }
    }

    //skip the qtype and qclass fields
    //for better or worse, we'll assume they are the same as our request
    ptrOffset += 4;
    
    //seconds to cache the response
    if (!packet->readNet32(ptrOffset,&(cacheEntry->ttl))) return;
    ptrOffset += 4;

    uint16_t rdlength;
    if (!packet->readNet16(ptrOffset,&rdlength)) return;
    ptrOffset += 2;

    //store the IP
    if (rdlength == 4){  //should be 4 for IP INET
      for(int i=0; i<4; i++){ 
	if (!packet->read8(ptrOffset+i,&(cacheEntry->ipAddress[i]))) return;
      }
    }
    
    ptrOffset += rdlength;
    
    //after we get our first record, we really don't need anything else
    //for domain names with multiple A records, we could keep looping 
    //through the balance, but we won't
    if (rdlength == 4)
      break; 
    
  }//end each answer
  
}//end handleDatagram

void DNSHandler::handleTimer(uint8_t index){
  
  uint32_t currentTime = host_millis();
  bool needTimer = false;

  //loop through our cache entries
  for(int i=0; i<cacheSize; i++){
    
    //if we are pending and it has been more than 1 second
    if (cache[i].status == (PENDING | NO_ERROR)){
      
      if ((currentTime - cache[i].time) > 1000){
	//if we have already tried 5 times, give up
	if (cache[i].attempts >= 5){
	  cache[i].status = DONE | NO_RESPONSE;
	  continue;
        }

	//we still need the timer
	needTimer = true;

	//if we are still unresolved, send another request
	cache[i].attempts++;
	cache[i].time = currentTime;
	uint8_t* dnsServer = dnsIP;
	if (cache[i].attempts % 2 == 0){
	  dnsServer = dnsIPBackup;
	}
	sendDNSRequest(cache[i].domainName,i+1,dnsServer);
      }//end if
    }//end unresolved cache entry

  }//end for each cache entry

  //if no entries needed a timer, then we should unregister our timer
  if (!needTimer){
    udpHandler->getIPHandler()->unregisterTimer(timer);
    this->timer = 0;
  }
  
}//end handleTimer

void DNSHandler::checkAndSetExpiration(uint8_t i){

  uint32_t currentTime = host_millis();
  if (cache[i].status == (DONE | NO_ERROR)){
    if ((currentTime - cache[i].time) > cache[i].ttl*1000){
      cache[i].status = (EXPIRED | NO_ERROR);
    }
  }//end if checking expiration

}//end expireCache



void DNSHandler::printDNSCache(){
  printf("DNS Cache\n");
  printf
    ("----------------------------------------------------------------------");
  printf("\n");

  uint32_t currentTime = host_millis();
  
  for(int i=0; i<cacheSize; i++){
    checkAndSetExpiration(i);
    int skipChars = 0;
    if (cache[i].status == (DONE | NO_ERROR) || 
	cache[i].status == PENDING ||
	cache[i].status == EXPIRED){
      printf(" %d: %s",i,cache[i].domainName);
      for(int j=strlen(cache[i].domainName); j<35; j++) printf(" ");
    }
    printf(" -> ");
    if (cache[i].status == PENDING){
      printf("(pending:%u)\n",cache[i].attempts);
    }
    else{
      for(int j=0; j<4; j++){
	printf("%u",cache[i].ipAddress[j]);
	if (j<3) printf(".");
      }
      printf(" ttl: ");
      if (cache[i].status == EXPIRED) 
	printf("expired\n");
      else
	printf("%u\n",((cache[i].time+cache[i].ttl*1000)-currentTime) / 1000);
    }
  }//end for each cache entry
}//end printDNSCache
