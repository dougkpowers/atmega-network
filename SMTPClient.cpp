#include <hostutil.h>
#include <string.h>
#include <Base64.h>
#include "SMTPClient.h"

/* ====================================================================== */
/*                           P U B L I C     A P I                        */
/* ====================================================================== */
SMTPClient::SMTPClient(char* server, DNSHandler* dns,
		       uint16_t port, char* user, char* pass)
  :Socket(server,port,dns){

  if (user != NULL && pass != NULL){
    this->user = Base64::Encode(user);
    if (this->user != NULL){
      this->pass = Base64::Encode(pass);
      if (this->pass == NULL){
	free(this->user);
	this->user = NULL;
      }
    }
  }
  else{
    this->user = NULL;
    this->pass = NULL;
  }
  
  to = NULL;
  from = NULL;
  subject = NULL;
  body = NULL;

  this->status = EMAIL_INIT;
}

SMTPClient::~SMTPClient(){
  if (this->user != NULL) free (this->user);
  if (this->pass != NULL) free (this->pass);
}

bool SMTPClient::email(char* from, char* to, char* subject, char* body){
  if (!readyToEmail())
    return false;
  
  this->status = EMAIL_SENDING;
  this->to = to;
  this->from = from;
  this->subject = subject;
  this->body = body;

  this->connect();
}

uint8_t SMTPClient::getStatus(){
  return status;
}

bool SMTPClient::readyToEmail(){
  if (status == EMAIL_SENDING) return false;
  if (getState() != CLOSED) return false;
  return true;
}

/* ====================================================================== */
/*                           E V E N T    M G M T                         */
/* ====================================================================== */
//states for our SMTP state machine
#define SMTP_INIT 0
#define SMTP_SERVER_READY 1
#define SMTP_CLIENT_GREETED 2
#define SMTP_READY_FOR_AUTH 3
#define SMTP_CLIENT_SET_AUTH 4
#define SMTP_READY_FOR_USER 5
#define SMTP_CLIENT_SET_USER 6
#define SMTP_READY_FOR_PASS 7
#define SMTP_CLIENT_SET_PASS 8
#define SMTP_READY_FOR_FROM 9
#define SMTP_CLIENT_SET_FROM 10
#define SMTP_READY_FOR_RCPT 11
#define SMTP_CLIENT_SET_RCPT 12
#define SMTP_READY_FOR_DATA 13
#define SMTP_CLIENT_SET_DATA 14
#define SMTP_READY_FOR_EMAIL 15
#define SMTP_CLIENT_SET_EMAIL 16
#define SMTP_READY_FOR_QUIT 17

void SMTPClient::onEstablished(){

  if (to == NULL ||
      from == NULL || 
      (subject == NULL && body == NULL))
    close();

  protocolState = SMTP_INIT;

  memset(recvLine,0,sizeof(recvLine));

}

bool SMTPClient::checkCode(char* line, uint16_t code){
  char* space = strstr(line," ");
  if (space == NULL) return false;
  uint16_t number = 0;
  for(char* ptr = line; ptr < space; ptr++){
    if (*ptr < '0' || *ptr > '9') return false;
    number = number * 10;
    number += (*ptr - '0');
  }

  if (number == code) return true;
  return false;
}

bool SMTPClient::checkMessage(char* line, char* message){
  char* start = strstr(line," ");
  if (start == NULL) return false;
  start++;

  char* finish = strstr(start," ");
  if (finish == NULL)
    finish = line + strlen(line);
  
  if ((finish - start) != strlen(message)) return false;

  int i;
  for(char* ptr = start, i=0; ptr < finish; ptr++,i++){
    if (*ptr != message[i]) return false;
  }

  return true;
}

bool SMTPClient::onDataReceived(Buffer* buf){

  //figure out where we are in the line buffer
  int lineIdx =strlen(recvLine);

  //read each char and put it in our buffer
  for(int bufIdx=0; bufIdx<buf->size(); bufIdx++){
    char c;
    if (!buf->read8(bufIdx,(uint8_t*)&c)) return false;

    //if we found a new-line, process the line
    if (c == '\n' || c == '\r'){
      
      //terminate the line where the carriage return would be
      if (lineIdx < sizeof(recvLine) - 1)
	recvLine[lineIdx] = '\0';

      //then fire the onLineReceived event
      if (strlen(recvLine) > 0)
	onLineReceived(recvLine);

      //now reset our recv buffer
      recvLine[0] = '\0';
      lineIdx = 0;
    }
    //otherwise, add the char to the current line
    else if (lineIdx < sizeof(recvLine) - 1)
      recvLine[lineIdx++] = c;
  }
  return true;
}

void SMTPClient::onLineReceived(char* line){
#ifdef DEBUG
  printf("%s\n",line);
#endif
  //if we get an error code back, close the connection
  if (checkCode(line,500))
    close();

  switch(protocolState){
  case SMTP_INIT:
    if (checkCode(line,220)){
      protocolState = SMTP_SERVER_READY;
    }
    else
      close();
    break;
  case SMTP_CLIENT_GREETED:
    if (checkCode(line,250))
      protocolState = SMTP_READY_FOR_AUTH;
    else
      close();
    break;
  case SMTP_CLIENT_SET_AUTH:
    if (checkCode(line,334) && checkMessage(line,"VXNlcm5hbWU6"))
      protocolState = SMTP_READY_FOR_USER;
    else
      close();
    break;
  case SMTP_CLIENT_SET_USER:
    if (checkCode(line,334) && checkMessage(line,"UGFzc3dvcmQ6"))
      protocolState = SMTP_READY_FOR_PASS;
    else
      close();
    break;
  case SMTP_CLIENT_SET_PASS:
    if (checkCode(line,235))
      protocolState = SMTP_READY_FOR_FROM;
    else 
      close();
    break;
  case SMTP_CLIENT_SET_FROM:
    if (checkCode(line,250))
      protocolState = SMTP_READY_FOR_RCPT;
    else 
      close();
    break;
  case SMTP_CLIENT_SET_RCPT:
    if (checkCode(line,250))
      protocolState = SMTP_READY_FOR_DATA;
    else 
      close();
    break;
  case SMTP_CLIENT_SET_DATA:
    if (checkCode(line,354))
      protocolState = SMTP_READY_FOR_EMAIL;
    else 
      close();
    break;
  case SMTP_CLIENT_SET_EMAIL:
    if (checkCode(line,250)){
      protocolState = SMTP_READY_FOR_QUIT;
      status = EMAIL_SENT;
    }
    else 
      close();
    break;

  }//end switch

  if (readyToSend())
    onReadyToSend();

}

void SMTPClient::onReadyToSend(){
  Buffer* sb;
  uint16_t offset;

  switch(this->protocolState){
  case SMTP_SERVER_READY:
    sb = getSendDataBuffer();
    sb->write(0,"HELO ");
    sb->write(5,Socket::remoteDomain);
    sb->write8(5+strlen(this->remoteDomain),'\n');
    send(5+strlen(this->remoteDomain)+1);
    this->protocolState = SMTP_CLIENT_GREETED;
    break;
  case SMTP_READY_FOR_AUTH:
    send("auth login\n");
    this->protocolState = SMTP_CLIENT_SET_AUTH;
    break;
  case SMTP_READY_FOR_USER:
    sb = getSendDataBuffer();
    sb->write(0,this->user,strlen(this->user));
    sb->write8(strlen(this->user),'\n');
    send(strlen(this->user)+1);
    this->protocolState = SMTP_CLIENT_SET_USER;
    break;
  case SMTP_READY_FOR_PASS:
    sb = getSendDataBuffer();
    sb->write(0,this->pass,strlen(this->pass));
    sb->write8(strlen(this->pass),'\n');
    send(strlen(this->pass)+1);
    this->protocolState = SMTP_CLIENT_SET_PASS;
    break;
  case SMTP_READY_FOR_FROM:
    sb = getSendDataBuffer();
    sb->write(0,"MAIL FROM:");
    sb->write(10,from);
    sb->write8(10+strlen(from),'\n');
    send(10+strlen(from)+1);
    this->protocolState = SMTP_CLIENT_SET_FROM;
    break;
  case SMTP_READY_FOR_RCPT:
    sb = getSendDataBuffer();
    sb->write(0,"RCPT TO:");
    sb->write(8,to);
    sb->write8(8+strlen(to),'\n');
    send(8+strlen(to)+1);
    this->protocolState = SMTP_CLIENT_SET_RCPT;
    break;
  case SMTP_READY_FOR_DATA:
    send("DATA\n");
    this->protocolState = SMTP_CLIENT_SET_DATA;
    break;
  case SMTP_READY_FOR_EMAIL:
    
    offset = 0;
    sb = getSendDataBuffer();
    sb->write(offset,"From: "); offset += 6;
    sb->write(offset,from); offset += strlen(from);
    sb->write(offset,"\r\n"); offset += 2;

    if (subject != NULL){
      sb->write(offset,"Subject: "); offset += 9;
      sb->write(offset,subject); offset += strlen(subject);
      sb->write(offset,"\r\n"); offset+= 2;
    }

    sb->write(offset,"\r\n"); offset+= 2;
    sb->write(offset,body); offset += strlen(body);
    sb->write(offset,"\r\n"); offset += 2;

    sb->write(offset,".\r\n"); offset += 3;
    send(offset);

    this->protocolState = SMTP_CLIENT_SET_EMAIL;
    break;
  case SMTP_READY_FOR_QUIT:
    send("QUIT\r\n");
    close();
    break;
  }

}

void SMTPClient::onRemoteClosed(){
  close();
}

void SMTPClient::onClosed(){
  if (status == EMAIL_SENDING)
    status = EMAIL_FAIL;

  //we may be in time-wait which would prevent us from
  //being able to send another email desipte that a new
  //socket connection will always allocate a different port
  //so let's force ourselves straight to CLOSED
  if (getState() != CLOSED)
    forceClose();
}

void SMTPClient::onReset(bool resetByRemoteHost){
  if (status == EMAIL_SENDING)
    status = EMAIL_FAIL;
}
