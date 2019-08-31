/*
 * This library builds on the IP API by providing TCPHandler
 * (Transmission Control Protocol) capabilities.
 *
 * A TCP connection is defined by a local port, local IP, remote port,
 * and remote IP.  Together, this combination defines a Socket.  A 
 * socket's state is managed according to the TCP protocol.  The Socket
 * class takes care of managing the mechanics of the state.  This class
 * simply handles a raw TCP segment by determining which Socket the segment
 * should be delegated to and performing the delegation.
 *
 * TCP provides for reliable transmission.  Therefore, if a packet is lost
 * in transmission, it will be resent.  In order to provide this functionality,
 * any data sent must temporarily be copied into memory and stored there
 * until receipt of the data has been verified.  When creating the 
 * TCPHandler, the caller must provide a Buffer for such purposes.  
 * 
 * This outbound buffer will be evenly divided amongst all available sockets,
 * which is specified by the socketCapacity parameter to the constructor.
 * Therefore, if the caller provides an outbound buffer of size 1000 and 
 * requests a socket capacity of 4, then each socket will get 250 bytes
 * of the outbound buffer.  As a result, each socket will be able to send
 * a maximum of 250 bytes at a time.
 * 
 * If a calling application wishes to initiate a connection, it should
 * follow these steps:
 *     1. Create a socket establishing the remote IP and port to connect to
 *     2. Register the socket with the TCPHandler
 *             - either call Socket.registerTCPHandler(TCPHandler*) or
 *               TCPHandler.registerSocket(Socket*).  The result of either
 *               call is the same
 *     3. Call connect() on the Socket instance.
 *     4. Call readyToSend() on the Socket instance
 *           If true, call send(...) to send to the remote host
 *     5. When ready to close the socket, call readyToSend().  If it returns
 *        true, call close() on the socket.
 *
 *     remoteClosed() will indicate if the remote side has closed
 *     localClosed() will indicate the local side has closed
 *
 *     The socket will enter TIME_WAIT for 4 minutes once both sides
 *     have closed after which the socket state will become CLOSED
 *
 *     The client may call connect only if the socket is in a closed state.
 *     Call forceClose() to force the state to closed.  Calling connect()
 *     will connect again but will use a new local port number.
 *
 *     When done, call unregisterTCPHandler on the socket or call
 *     unregisterSocket(Socket*) on the TCPHandler.  If the socket is open,
 *     it will a FIN segment will be sent and forceClose() will be called.
 *     Afterward, the socket instance may be safely deleted.
 *
 * To accept server connections (listen on a port), follow these steps:
 *     1. Create a socket establishing the port to listen on.
 *     2. Register the socket with the TCPHandler
 *             -- see above for the various ways to do this
 *             -- this will immediately put the socket into a listen state
 *     3. Call close() on the socket when done.
 *     4. Rather than going to a CLOSED state, the socket will re-enter
 *        the listen state.
 *     5. Call forceClose() to not wait for the remote side to disconnect
 *        and immediately return to a listen state
 *
 *     If you wish to listen/support multiple connections at once, 
 *     create a Socket for each instance and register all of them with
 *     the TCPHandler.
 *
 * If implementing a specific protocol atop TCP, it may be useful to
 * extend the Socket class and override the following functions which
 * are called upon certain events:
 *
 *      onEstablished()
 *      onRemoteClosed()
 *      onDataReceived()
 *      onReadyToSend()
 */

#include <stdint.h>
#include <stdio.h>
#include "TCPHandler.h"

TCPHandler::TCPHandler(IPHandler *ipHandler, uint8_t socketCapacity,
		       Buffer* outboundBuffer){
  this->ip = ipHandler;
  this->socketCapacity = socketCapacity;

  // --- compute the max outbound len per socket ---  
  //the max outbound lengths cannot be larger than the IP
  //outbound buffer less the size of the TCP HEADER
  //this calculation assumes no TCP options are in use for data packets
  uint16_t stashSize = outboundBuffer->size();
  uint16_t s = ip->getSendPayloadBuffer()->size() - TCP_HEADER_LENGTH;
  if (stashSize > s) stashSize = s;
  this->maxOutboundLen = stashSize / socketCapacity;

  //setup our registeredSockets array
  this->registeredSockets = (registeredSocket*)
    malloc(sizeof(registeredSocket)*socketCapacity);
  if (this->registeredSockets == NULL)
    this->socketCapacity = 0;
  for(int i=0; i<socketCapacity; i++){
    registeredSockets[i].socket = NULL;
    registeredSockets[i].stashBuffer = 
      new OffsetBuffer(outboundBuffer,this->maxOutboundLen*i,
		       this->maxOutboundLen);
  }

  //register our protocol with the TCP handler
  ipHandler->registerProtocol(TCP_PROTOCOL,this);

  timerIndex = 0;
}

TCPHandler::~TCPHandler(){
  for(int i=0; i<this->socketCapacity; i++){
    delete this->registeredSockets[i].stashBuffer;
    if (this->registeredSockets[i].socket != NULL){
      this->registeredSockets[i].socket->close();
      this->registeredSockets[i].socket->unregisterTCPHandler();
    }
  }
}

Buffer* TCPHandler::getStash(Socket* socket){
  for(int i=0; i<this->socketCapacity; i++){
    if (this->registeredSockets[i].socket == socket){
      return this->registeredSockets[i].stashBuffer;
    }
  }
  return NULL;
}

bool TCPHandler::registerSocket(Socket* socket){

  //first see if the socket is already registered
  for(int i=0; i<this->socketCapacity; i++){
    if (this->registeredSockets[i].socket == socket){
      return true; //we're already registered
    }
  }

  //now look for an available slot
  for(int i=0; i<this->socketCapacity; i++){
    if (this->registeredSockets[i].socket == NULL){
      //allocate a stash buffer

      //put the socket into our array
      this->registeredSockets[i].socket = socket;

      //now try to reciprocate the registration
      if (this->registeredSockets[i].socket->registerTCPHandler(this)){
	if (timerIndex == 0){
	  timerIndex = ip->registerTimer(this,1000);
	  if (timerIndex == 0){
	    //back out since we have no timer

	    //remove the socket from our array
	    this->registeredSockets[i].socket = NULL;
	    return false;
	  }
	}
	return true;
      }
      // back out since the socket registration failed

      //remove the socket from our array
      this->registeredSockets[i].socket = NULL;
      return false;
    }
  }//end for

  //we're out of room folks
  return false;  
}

void TCPHandler::unregisterSocket(Socket* socket){
  uint8_t nullCount = 0;
  for(uint8_t i=0; i<this->socketCapacity; i++){
    if (this->registeredSockets[i].socket == socket){
      this->registeredSockets[i].socket = NULL;
      nullCount++;
    }
    else if (this->registeredSockets[i].socket == NULL)
      nullCount++;
  }//end for

  if (nullCount == this->socketCapacity){
    ip->unregisterTimer(timerIndex);
    timerIndex = 0;
  }
}//end unregisterSocket

IPHandler* TCPHandler::getIPHandler(){
  return ip;
}

void TCPHandler::handleTimer(uint8_t index){
  for(int i=0; i<this->socketCapacity; i++){
    if (this->registeredSockets[i].socket != NULL)
      this->registeredSockets[i].socket->checkState();
  }//end for
}

void TCPHandler::handlePacket(uint8_t* sourceIP, Buffer *packet){

  uint16_t sourcePort;
  uint16_t localPort;
  
  if (!packet->readNet16(0,&sourcePort)) return;
  if (!packet->readNet16(2,&localPort )) return;

  //find the socket that should handle the combination of
  //local ip, local port, remote ip, and remote port
  for(int i=0; i<socketCapacity; i++){
    if (registeredSockets[i].socket != NULL &&
	registeredSockets[i].socket->equals(sourceIP, sourcePort, localPort)){
      registeredSockets[i].socket->handleSegment(sourceIP,packet);
      return;
    }
  }//end for

  //see if we have a socket listening on the target port
  for(int i=0; i<socketCapacity; i++){
    if (registeredSockets[i].socket != NULL &&
	registeredSockets[i].socket->equals(localPort)){
      registeredSockets[i].socket->handleSegment(sourceIP,packet);
      return;
    }
  }//end for
  
}//end handlePacket

/*
 * The largest amount of data we can receive is defined by the maximum
 * size of our receive buffer.
 *
 * This is defined as the size of the IP payload buffer less the size of
 * the maximum size of the TCP header.  Unfortunately, the TCP header
 * could be variable length as it ends with a variabl length options
 * section.  We simply make the assumption that we will never receive
 * TCP options in excess of 4 bytes.  If we do, and data is transmitted
 * at the MSS, then data may be lost, the TCP checksum will not compute,
 * and the data will never be passed to the socket.
  */
uint16_t TCPHandler::getMaxSegmentSize(){
  return ip->getMaxReceivePayload() - TCP_HEADER_LENGTH - 4;
}

