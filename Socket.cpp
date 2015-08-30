/*
 * State management of the socket is a bit of a pain; a state
 * machine can be found here: http://tools.ietf.org/html/rfc793#page-22
 *
 * See comments on TCPHandler.cpp for usage
 */

#include <string.h>
#include <stdio.h>
#include <hostutil.h>
#include "Socket.h"
#include "IPHandler.h"
#include "DNSHandler.h"


/* ====================================================================== */
/*                         C O N S T R U C T O R S                        */
/* ====================================================================== */
void Socket::init(){
  this->localSeq = random() % 1000;
  this->remoteSeq = 0;
  this->remoteMSS = 0;
  this->remoteWindow = 0;
  this->tcp = NULL;
  this->attempts = 0;
  this->lastDataLength = 0;
  this->stash = NULL;
  this->recvBuffer = NULL;
  this->sendBuffer = NULL;
  this->connectOnResolution = false;
  this->timeout = 0;
  closed();

}

Socket::Socket(uint8_t* remoteIP, uint16_t remotePort){
  this->listenPort = 0;
  memcpy(this->remoteIP,remoteIP,4);
  this->remotePort = remotePort;
  init();
}//end constructor

Socket::Socket(char* server, uint16_t remotePort, DNSHandler* dns){
  this->listenPort = 0;
  this->remoteDomain = server;
  memset(this->remoteIP,0,4);
  this->remotePort = remotePort;
  this->dns = dns;
  init();
}//end constructor

Socket::Socket(uint16_t listenPort){
  this->listenPort = listenPort;
  this->localPort = listenPort;
  init();
}//end constructor

Socket::~Socket(){
  unregisterTCPHandler();
  if (recvBuffer != NULL)
    delete this->recvBuffer;
  this->recvBuffer = NULL;
  if (sendBuffer!= NULL)
    delete this->sendBuffer;
  this->sendBuffer = NULL;
}
#include <Arduino.h>
/* ====================================================================== */
/*                      D N S    R E S O L U T I O N                      */
/* ====================================================================== */
bool Socket::resolveIP(){

  //we should only be able to resolve an IP if we are actively
  //resolving or if we are currently in a closed state
  if (this->state != RESOLVING && 
      this->state != CLOSED &&
      this->state != UNKNOWN_HOST)
    return false;

  //if we have no remote domain, then abort
  if (remoteDomain == NULL){
    return true;
  }

  //see if we can resolve
  uint8_t error;
  uint8_t* rip = dns->resolve(remoteDomain,&error);

  if (error != NO_ERROR){ //if we got any error, then abort
    setState(UNKNOWN_HOST);
    return false;
  }

  //if we got a response, copy over the ip
  if (rip != NULL){
    memcpy(this->remoteIP,rip,4);

    //if we are in a resolving state, return to a closed state
    if (this->state == RESOLVING){

      //return to a closed state
      closed();

      //see if we need to return flow control to the connect function
      if (this->connectOnResolution){
	this->connectOnResolution = false;
	return connect();
      }
    }

    return true;
  }
  else{
    setState(RESOLVING);
  }

  return true;
}

/* ====================================================================== */
/*                              E Q U A L I T Y                           */
/* ====================================================================== */
bool Socket::equals(uint8_t* remoteIP, uint16_t remotePort, 
		    uint16_t localPort){
  
  if (isHost() && this->state == LISTEN) return false;
  if (this->state == CLOSED) return false;
  if (remotePort != this->remotePort) return false;
  if (localPort != this->localPort) return false;
  if (!IPHandler::ipsEquate(remoteIP,this->remoteIP)) return false;
  return true;
}

bool Socket::equals(uint16_t listenPort){
  if (!isHost()) return false;
  if (this->state != LISTEN) return false;
  if (this->listenPort != listenPort) return false;
  return true;
}

/* ====================================================================== */
/*                                E V E N T S                             */
/* ====================================================================== */
void Socket::onEstablished(){;}
void Socket::onReadyToSend(){;}
void Socket::onRemoteClosed(){;}
void Socket::onLocalClosed(){;}
void Socket::onClosed(){;}
void Socket::onReset(bool resetByRemoteHost){;}

/* ====================================================================== */
/*                          P U B L I C      A P I                        */
/* ====================================================================== */

/*
 * Sets the timeout for the TCP connection.  If no data is sent or
 * received and the state of the connection does not change in t
 * seconds then the connection is closed.  If the connection is
 * has been closed locally, but has not been closed by the remote host
 * then the connection is forcibly closed.
 *
 * Set the timeout to 0 for no timeout.
 */
void Socket::setTimeout(uint32_t t){
  this->timeout = t;
}

/*
 * The largest amount of payload data we can send.  This may be limited
 * by either the local host, the remote host, or both.
 */
uint16_t Socket::getMaxSendPayload(){
  
  uint16_t s = (stash == NULL ? 0 : stash->size());
  
  if (this->remoteWindow < s)
    s = this->remoteWindow;
  
  if (this->remoteMSS > 0 && this->remoteMSS < s)
     s = this->remoteMSS;

  return s;
}

bool Socket::isHost(){
  return this->listenPort > 0;
}

bool Socket::isClient(){
  return this->listenPort == 0;
}

bool Socket::registerTCPHandler(TCPHandler* handler){

  if (tcp != NULL && handler == tcp) return true;
  if (tcp != NULL){ return false; }

  //tcp is NULL, so let's register
  this->tcp = handler;
  if (handler->registerSocket(this)){
    this->stash = handler->getStash(this);
    if (isHost()) setState(LISTEN); //auto promote to listen if we are a host

    //configure our send buffer
    Buffer* ipBuffer = tcp->getIPHandler()->getSendPayloadBuffer();
    if (this->sendBuffer == NULL)
      this->sendBuffer = new OffsetBuffer(ipBuffer,20);
    else
      this->sendBuffer->reinit(ipBuffer,20);

    return true;
  }
  
  //tcp register failed, so back out
  this->tcp = NULL;
  return false;
}

void Socket::unregisterTCPHandler(){
  if (tcp != NULL){
    forceClose(); //the first call is to try and close cleanly
    tcp->unregisterSocket(this);
    tcp = NULL;
    this->stash = NULL;
    forceClose();  //now that tcp is null, we are gauranteed to go to CLOSED
  }
}

uint16_t Socket::getLocalPort(){
  return this->localPort;
}

uint8_t Socket::getState(){
  return this->state & 0x0F;
}

uint16_t Socket::getStateTime(){
  return this->stateTime;
}

bool Socket::connect(){

  //let's make sure we have a valid IP address before we start
  if (!resolveIP()) return false;

  //if we are still trying to resolve, set a flag so that
  //connect is called once resolution is complete
  if (this->state == RESOLVING){
    this->connectOnResolution = true;
    return true;
  }

  if (this->state != CLOSED)
    return false;

  if (tcp == NULL)
    return false;

  //establish a local port
  this->localPort = tcp->getIPHandler()->getPort();
  
  //send a SYN to start the handshake
  if(!sendSegment(SYN,0)) return false;
  attempts = 1;

  //update our state
  setState(SYN_SENT);

  return true;
}

bool Socket::readyToSend(){
  if ((this->state & 0x0F) != ESTABLISHED) return false;
  if ((this->state & 0xF0) == AWAITING_ACK) return false;
  return true;
}

Buffer* Socket::getSendDataBuffer(){
  if (this->tcp == NULL)
    return NULL;
  return sendBuffer;
}

uint16_t Socket::read(uint8_t* data, uint16_t length, uint16_t offset){
  if (recvBuffer == NULL) return 0;
  if (recvBuffer->size() - offset < length) 
    length = recvBuffer->size() - offset;

  if (recvBuffer->read(offset,data,length))
    return length;

  return 0;
}

bool Socket::localClosed(){
  if (this->state == FIN_WAIT_1 ||
      this->state == FIN_WAIT_2 ||
      this->state == CLOSING ||
      this->state == LAST_ACK ||
      this->state == TIME_WAIT ||
      this->state == LISTEN ||
      this->state == CLOSED)
    return true;
  return false;
}

bool Socket::remoteClosed(){
  if (this->state == CLOSE_WAIT ||
      this->state == LAST_ACK ||
      this->state == CLOSING ||
      this->state == TIME_WAIT ||
      this->state == LISTEN ||
      this->state == CLOSED)
    return true;
  return false;
}

bool Socket::send(uint16_t length){

  //just in case a bonehead tries to send 0 bytes
  if (length == 0) return true;

  //quick error checks
  if (!readyToSend()) return false;

  if (length > this->getMaxSendPayload())
    return false;

  //make this our first attempt
  attempts = 1;

  //copy the data to the stash in case we need to resend
  Buffer* sb = getSendDataBuffer();
  if (sb == NULL) return false;
  if (!sb->copyTo(stash,0,0,length)) return false;

  //send the data out
  if (sendSegment(ACK | PSH,length)){
    this->stateTime = host_millis();
    return true;
  }
  return false;
}

bool Socket::send(uint8_t* data, uint16_t length){

  //copy the data to the tx buffer
  Buffer* sb = getSendDataBuffer();
  if (sb == NULL) return false;
  if (!sb->write(0,data,length)) return false;

  //send it
  return send(length);
}

bool Socket::send(char* data){
#ifdef DEBUG
  printf(data);
#endif
  return send((uint8_t*)data,strlen(data));
}

bool Socket::close(){
  if (localClosed())
    return true;

  if(sendSegment(FIN | ACK,0)){
    attempts = 1;

    if ((this->state & 0x0F) == ESTABLISHED)
      setState(FIN_WAIT_1);
    else if (this->state == SYN_RECEIVED)
      setState(FIN_WAIT_1);
    else if (this->state == CLOSE_WAIT)
      setState(LAST_ACK);

    //fire the onLocalClosed() event
    onLocalClosed();
    return true;
  }
  return false;
}

bool Socket::reset(uint32_t seq, uint32_t ack){

  if (ack == remoteSeq){
    if (!sendSegment(RST | ACK,0,seq,ack)) 
      return false;
  }
  else{
    if (!sendSegment(RST,0,seq,ack)) 
      return false;
  }
  onReset(false); //fire event
  return true;
}

//this is the equivalent of forcing a reset
void Socket::forceClose(){

  //just in case, call close
  if (this->state != TIME_WAIT)
    reset(localSeq,remoteSeq);

  //now immediately promote to a closed() state
  closed();
}

/* ====================================================================== */
/*  P R I V A T E    H E L P E R S   F O R     S T A T E   C H A N G E S  */
/* ====================================================================== */

void Socket::setState(uint8_t state){

  uint8_t priorState = this->state;

  this->state = state;
  this->stateTime = host_millis();

  //fire event for CLOSED
  if (state == CLOSED && priorState != RESOLVING)
    onClosed();

  //fire event for ESTABLISHED
  if (state == ESTABLISHED){
    onEstablished();
    onReadyToSend();
  }
}

void Socket::closed(){
  if (tcp != NULL && isHost()){
    setState(LISTEN);
  }
  else{
    setState(CLOSED);
  }
}

/* ====================================================================== */
/*                        T C P    C A L L B A C K S                      */
/* ====================================================================== */
void Socket::checkState(){

  //if we are closed, return
  if (this->state == CLOSED) return;

  //if we are in listen, return
  if (this->state == LISTEN) return;

  //see if we have exceeded the Socket timeout
  uint32_t elapsed = host_millis() - stateTime;
  if (this->timeout > 0 && elapsed > timeout){
    if (!this->localClosed()){
      close();
      return;
    }
    forceClose();
  }//end if timeout elapsed

  //if we are established and NOT awaiting an ack, return
  if (this->state == ESTABLISHED) return;

  //if we are trying to resolve, check to see if there is a DNS response
  if (this->state == RESOLVING){
    resolveIP();
    return;
  }

  //if less than ACK_WAIT_DURATION has elapsed, return
  if (elapsed < ACK_WAIT_DURATION) return;

  //if we are in ESTABLISHED and we are awaiting an ACK,
  //then resend the last packet
  if (this->state == (ESTABLISHED | AWAITING_ACK)){
    if (attempts++ > 10) { forceClose(); return; }
    if (!resendData())
      forceClose();
  }

  //if we are in syn_sent, resend syn
  if(this->state == SYN_SENT){
    if (attempts++ > 10) { forceClose(); return; }
    this->localSeq--; //backup the seq by 1
    sendSegment(SYN,0);
    return;
  }

  //if we are in syn_recv, resend syn+ack
  if (this->state == SYN_RECEIVED){
    if (attempts++ > 10) { forceClose(); return; }
    this->localSeq--; //backup the seq by 1
    sendSegment(SYN | ACK,0);
    return;
  }

  //if we are in fin_wait_1, resend fin
  //if we are in closing, resend fin
  if (this->state == FIN_WAIT_1 || this->state == CLOSING){
    if (attempts++ > 10) { forceClose(); return; }
    this->localSeq--; //backup the seq by 1
    sendSegment(FIN | ACK,0);
    return;
  }

  //TIME_WAIT_DURATION to close
  if (elapsed < TIME_WAIT_DURATION) return;

  //if we are in TIME_WAIT, change to CLOSE
  if (this->state == TIME_WAIT)
    closed();
}

void Socket::handleSegment(uint8_t* sourceIP, Buffer* buf){

  //by default, configure the receive payload buffer to zero bytes
  if (this->recvBuffer == NULL)
    this->recvBuffer = new OffsetBuffer(buf,buf->size());
  else
    this->recvBuffer->reinit(buf,buf->size());

  //load our options, ACK value, remote seq
  uint8_t opt;
  uint32_t ack;
  uint32_t seq;
  if (!buf->read8(13,&opt)) return;
  if (!buf->readNet32(8,&ack)) return;
  if (!buf->readNet32(4,&seq)) return;

  //load the window size
  if (!buf->readNet16(14,&this->remoteWindow)) return;
 
  //before calcing the checksum, we need to have the remoteIP
  //if we're in listen mode then the IP won't be set yet
  if (this->state == LISTEN && opt == SYN){
    memcpy(this->remoteIP,sourceIP,4);
  }

  //now we are ready to verify the checksum
  uint16_t checksum = calcChecksum(buf,buf->size());
  uint16_t givenChecksum;
  if (!buf->readNet16(16,&givenChecksum)) return;
  if (givenChecksum != checksum) return; //discard the paket

#ifdef DEBUG
  Serial.print("In <- Opt: ");
  Serial.print(opt);
  Serial.print("|Ack: ");
  Serial.print(ack);
  Serial.print("|Seq: ");
  Serial.print(seq);
  Serial.print("|LSeq: ");
  Serial.print(localSeq);
  Serial.print("|RSeq: ");
  Serial.print(remoteSeq);
  Serial.print("|State: ");
  Serial.println(this->state);
#endif

  //check for states where we need to reset
  //case #1 from RFC 793 (see page 35)
  //we should expect no packets to be received on a closed connection
  if (this->state == CLOSED && (opt & RST) != RST){

    if ((opt & ACK) == ACK)
      reset(ack,0);
    else{
      reset(0,seq+buf->size());
    }
    return;
  }
  //and we should expect nothing but a SYN if we are in the listen state
  if (this->state == LISTEN  && (opt & SYN) != SYN && (opt & RST) != RST){
    if ((opt & ACK) == ACK)
      reset(ack,0);
    else{
      reset(0,seq+buf->size());
    }
    return;
  }

  //case #2 from RFC 793
  //if we are in any non-syncrhonized state and the incoming segment
  //acknolwedges something not yet sent then send a reset
  if ((this->state == LISTEN || 
       this->state == SYN_SENT || 
       this->state == SYN_RECEIVED) && 
      (opt & ACK) == ACK && ack > localSeq){
    reset(ack,0);
    return;
  }

  //case #3 from RFC 793  -- verify the ack value
  //if we are in a synchronized state any unaccptable segment (out of
  //window sequence or unacceptable ack) send only an empty ACK
  if ((this->state == ESTABLISHED ||
       this->state == FIN_WAIT_1 ||
       this->state == FIN_WAIT_2 ||
       this->state == CLOSE_WAIT ||
       this->state == CLOSING ||
       this->state == LAST_ACK ||
       this->state == TIME_WAIT)){
    if ((opt & ACK) == ACK && ack > localSeq){
      sendSegment(ACK,0);
      return;
    }
    if (seq > remoteSeq){ //did we miss a packet?
      sendSegment(ACK,0);
      return;
    }
  }
  
  //we received a reset packet so go into reset processing
  if ((opt & RST) == RST){
    if (this->state == SYN_SENT && ack == localSeq){
      closed();
      onReset(true);
    }
    else if (this->remoteSeq == seq){
      if (this->state == LISTEN) return;
      if (this->state == SYN_RECEIVED){
	closed();
	return;
      }
      closed();
      onReset(true);
    }
    return;
  }

  //someone is trying to connect 
  if (this->state == LISTEN && opt == SYN){ //no ack, just a pure SYN

    //setup the remote seq
    this->remoteSeq = seq;
    this->remoteSeq++;
    
    //get the remote port
    if(!buf->readNet16(2,&(this->remotePort))) return;
    
    //get the size of the header
    uint8_t tcp_header_size;
    if (!buf->read8(12,&tcp_header_size)) return;
    tcp_header_size = tcp_header_size >> 4;
    
    //if we have tcp options set, check for an MSS
    if (tcp_header_size == 6){
      uint32_t tcpopt = 0;
      buf->readNet32(20,&tcpopt);
      
      //the remote host is sending us a maximum segment size
      if (tcpopt >> 16 == 0x0204){ //MSS option
	this->remoteMSS = tcpopt << 16 >> 16;
      }
    }

    //set the remote port
    if(!buf->readNet16(0,&this->remotePort)) return;


    //send back an ACK+SYN
    if (sendSegment(SYN | ACK,0)){
      setState(SYN_RECEIVED);
      attempts = 1;
    }
    return;
  }

  //someone is trying to accept a connect request 
  if (this->state == SYN_SENT && opt == SYN){ //no ack, just a pure SYN
    //get the remote seq
    this->remoteSeq = seq;
    this->remoteSeq++;
    
    //get the size of the header
    uint8_t tcp_header_size;
    if (!buf->read8(12,&tcp_header_size)) return;
    tcp_header_size = tcp_header_size >> 4;
    
    //if we have tcp options set, check for an MSS
    if (tcp_header_size == 6){
      uint32_t tcpopt = 0;
      buf->readNet32(20,&tcpopt);
      
      //the remote host is sending us a maximum segment size
      if (tcpopt >> 16 == 0x0204){ //MSS option
	this->remoteMSS = tcpopt << 16 >> 16;
      }
    }

    setState(SYN_RECEIVED);

    //send back an ACK
    sendSegment(ACK,0);
      
    return;
  }
 
  //see if we received a SYN+ACK so we can promote ourselves to ESTABLISHED
  if (this->state == SYN_SENT &&               //we've sent a SYN
      opt  == (ACK | SYN) &&                   //we're getting a SYN+ACK
      ack == localSeq){                        //and the sequence matches

    //get the remote seq
    this->remoteSeq = seq;
    this->remoteSeq++;
    
    //get the size of the header
    uint8_t tcp_header_size;
    if (!buf->read8(12,&tcp_header_size)) return;
    tcp_header_size = tcp_header_size >> 4;
    
    //if we have tcp options set, check for an MSS
    if (tcp_header_size == 6){
      uint32_t tcpopt = 0;
      buf->readNet32(20,&tcpopt);
      
      //the remote host is sending us a maximum segment size
      if (tcpopt >> 16 == 0x0204){ //MSS option
	this->remoteMSS = tcpopt << 16 >> 16;
      }
    }

    //send back an ACK
    sendSegment(ACK,0);

    setState(ESTABLISHED); //change the state to ESTABLISHED

    return;
  }
      
  if ((this->state & 0x0F) == ESTABLISHED){
    //when established, we should always get an ack
    if ((opt & ACK) != ACK) return;

    //check to see if we received an ack and can clear our awaiting ack field
    bool clearAck = false;
    if (ack == localSeq && (this->state & AWAITING_ACK) == AWAITING_ACK){
      clearAck = true; //we'll do this after processing events
    }

    //check to make sure the incoming seq is right
    if (seq != this->remoteSeq){ //if the seq is not right
      sendSegment(ACK,0); //let them know where we expect to be
      return; //abort
    }

    //whether or not the push bit is set, we must check for data

    //calculate the payload size
    uint8_t tcp_header_size; //size is in 4 byte words
    if (!buf->read8(12,&tcp_header_size)) return;
    tcp_header_size = tcp_header_size >> 4; //in the first 4 bits so shift
    uint16_t payloadSize = buf->size() - tcp_header_size * 4;

    //if we have data
    if (payloadSize > 0){

      //update the recvBuffer with our data; starts after the tcp header
      this->recvBuffer->reinit(buf,tcp_header_size * 4);
      
      //if we have a FIN then we need to increase by 1
      if ((opt & FIN) == FIN){ this->remoteSeq++; }
      
      //fire the onDataReceived event first
      if (recvBuffer->size() > 0){
	if (this->onDataReceived(recvBuffer)){
	  //update our remoteSeq if we successfully handled data
	  this->remoteSeq = seq + payloadSize;
	  //set the stateTime so we don't timeout
	  this->stateTime = host_millis();
	}//end if
      }//end if that payload is greater than 0 bytes

      //acknowledge where we expect to be next
      sendSegment(ACK,0);
    }//end if we have  payload
      
    //if the remote side wants to close
    if ((opt & FIN) == FIN){
      
      //if the payloadSize is equal to zero
      //then we haven't send an ack yet
      if (payloadSize == 0){
	this->remoteSeq++; //increment our seq
	sendSegment(ACK,0); //acknowledge receipt of FIN
      }

      //figure out the next state
      if (this->state == FIN_WAIT_1){
	if ((opt & ACK) == ACK && ack == localSeq)
	  setState(TIME_WAIT);
	else
	  setState(CLOSING);
      }
      else if (this->state == FIN_WAIT_2)
	setState(TIME_WAIT);
      else if ((this->state & 0x0F) == ESTABLISHED)
	setState(CLOSE_WAIT);
      
      //fire the necessary event
      onRemoteClosed();      
    }//end FIN

    //now that we have processed received data, let event listener
    //know that it's now good to send data (but only if ACK flag cleared)
    if (clearAck){
      this->state = this->state & 0x0F; //clear the AWAITING_ACK flag
      onReadyToSend(); //fire ready to send event
    }

    return;
  }//end established mode

  
  //an ACK but not in ESTABLISHED or SYN_SENT (no FIN)
  if ((opt & ACK) == ACK && ack == localSeq){
    if (this->state == SYN_RECEIVED)
      setState(ESTABLISHED);
    else if (this->state == FIN_WAIT_1)
      setState(FIN_WAIT_2);
    else if (this->state == CLOSING)
      setState(TIME_WAIT);
    else if (this->state == LAST_ACK){
      closed();
    }
    return;
  }

  //as a last resort, we need to close the connection
  forceClose();
  
}

/* ====================================================================== */
/*                        S E N D    H E L P E R S                        */
/* ====================================================================== */
/*
 * The number of bytes that can be handled by the local size of the
 * connection.  This is determined by calling getApplicationWindowSize()
 * If the value returned is greater than the maximum segment size for
 * the TCP segment, then this function returns the maximum segment size
 */
uint16_t Socket::getWindowSize(){
  uint16_t appSize = getApplicationWindowSize();
  if (appSize > tcp->getMaxSegmentSize())
    return tcp->getMaxSegmentSize();
  return appSize;
}

/*
 * The number of octets/bytes that the application (on the local side)
 * is ready to process
 */
uint16_t Socket::getApplicationWindowSize(){
  return tcp->getMaxSegmentSize();
}

//resends the last data packet
bool Socket::resendData(){

  //rewind our sequence
  this->localSeq -= lastDataLength;

  //populate the tx buffer with data from the stash
  Buffer* sb = getSendDataBuffer();
  if (sb == NULL) return false;
  if (!sb->copyFrom(stash,0,0,lastDataLength)) return false;

  //send the segment again using data from the stash
  return sendSegment(ACK,lastDataLength);
}
 
bool Socket::sendSegment(uint8_t control,uint16_t length,
			 uint32_t seq,uint32_t ack){


  //if this is a reset packet, then we have special values
  //for the seq and ack fields otherwise, go with the regulars
  if ((control & RST) != RST){
    seq = localSeq;
    ack = remoteSeq;
  }

#ifdef DEBUG
  Serial.print("Out -> Opt: ");
  Serial.print(control);
  Serial.print("|Ack: ");
  Serial.print(ack);
  Serial.print("|Seq: ");
  Serial.println(seq);

#endif

  Buffer* buf = tcp->getIPHandler()->getSendPayloadBuffer();
  if (!buf->writeNet16(0,localPort)) return false;
  if (!buf->writeNet16(2,remotePort)) return false;
  if (!buf->writeNet32(4,seq)) return false; 
  if (!buf->writeNet32(8,ack)) return false; 
  
  uint8_t optionLength = 0;
  if ((control & SYN) == SYN) {
    //create room in the header for a 4 byte option
    if (!buf->write8(12,6 << 4)) return false;
    //set the maximum segment size
    //this excludes the size of the TCP header
    if (!buf->write8(20,0x02)) return false;    //MSS option announcement
    if (!buf->write8(21,0x04)) return false;    //MSS option announcement
    if (!buf->writeNet16(22,tcp->getMaxSegmentSize())) return false; 
    localSeq++;
    optionLength = 4;
  }
  else if ((control & FIN) == FIN){
      localSeq++;
  }
  else {
    //the header can just be 5 words instead of 6 as we have no options to set
    if (!buf->write8(12,5 << 4)) return false;
    if (length > 0){
      this->state = this->state | AWAITING_ACK;
      lastDataLength = length;
      localSeq += length;
    }
  }

  if (!buf->write8(13,control)) return false;  //control values
  if (!buf->writeNet16(14,this->getWindowSize())) return false; //window size 
  if (!buf->writeNet16(18,0x0000)) return false;   //urg ptr

  return transmit(length,optionLength);
}

uint16_t Socket::calcChecksum(Buffer* buf, uint16_t len){ 

  uint32_t pseudo = 0;  
  
  //sum the 16 bit words in the pseudo header
  //protocol
  pseudo += TCP_PROTOCOL;
  
  //tcp length
  pseudo += len;

  //source ip
  uint8_t* localIP = tcp->getIPHandler()->getIPAddress();
  pseudo += HTONS(*((uint16_t*)localIP));
  pseudo += HTONS(*((uint16_t*)(localIP+2)));
  
  //destination ip
  pseudo += HTONS(*((uint16_t*)remoteIP));
  pseudo += HTONS(*((uint16_t*)(remoteIP+2)));

  return buf->checksum(len,16,pseudo);
}

bool Socket::transmit(uint16_t length, uint8_t option_length){

  //see if we have a route to the host
  if (tcp->getIPHandler()->getMACForIP(this->remoteIP) == NULL){
    //if not, request a route
    tcp->getIPHandler()->getARPHandler()->requestMACAddress(this->remoteIP);
    return true;
  }

  Buffer* buf = tcp->getIPHandler()->getSendPayloadBuffer();
  uint16_t len = length + TCP_HEADER_LENGTH + option_length;  

  //calc the checksum
  uint16_t checksum = calcChecksum(buf,len);

  //now write the checksum to the buffer
  if (!buf->writeNet16(16,checksum)) return false;
  
  //send the IP packet
  return tcp->getIPHandler()->sendPacket(this->remoteIP,TCP_PROTOCOL,len);

}
