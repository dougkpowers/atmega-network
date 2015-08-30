#ifndef SMTPCLIENT_H
#define SMTPCLIENT_H

#include <Socket.h>

#define EMAIL_INIT 0
#define EMAIL_SENDING 1
#define EMAIL_SENT 2
#define EMAIL_FAIL 3

class SMTPClient: public Socket{
  
  char* user;
  char* pass;
  uint8_t status;
  uint8_t protocolState;

  char* to;
  char* from;
  char* subject;
  char* body;

  char recvLine[40];

  void onLineReceived(char* line);
  bool checkMessage(char*,char*);
  bool checkCode(char*,uint16_t);

 protected:

  void onEstablished();
  bool onDataReceived(Buffer* buf);
  void onReadyToSend();
  void onRemoteClosed();
  void onClosed();
  void onReset(bool resetByRemoteHost);

 public:
  SMTPClient(char* server, DNSHandler* dns, uint16_t port = 25,
	     char* username = NULL,
	     char* password = NULL);

  ~SMTPClient();

  bool readyToEmail();
  bool email(char* from, char* to, char* subject, char* body);
  uint8_t getStatus();
};

#endif
