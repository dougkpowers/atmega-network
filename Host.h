/*
 * These are various functions whose implementation will likely vary
 * from host to host.  For the sake of writing code that can easily
 * be ported from one to another host, these host dependent implementations
 * have been consolidated here.
 */

#ifndef HOST_H
#define HOST_H

#include <stdint.h>
#include <stdio.h>

#define HTONS(s) (s >> 8 | s << 8)
#define NTOHS(s) HTONS(s)
#define HTONL(w) (w>>24|w<<8>>24<<8|w>>8<<24>>8|w<<24)
#define NTOHL(w) HTONL(w)

class Host {

 public: 
  static void init();

  static uint16_t htons (uint16_t value);
  static uint16_t ntohs (uint16_t value);
  static uint32_t htonl (uint32_t value);
  static uint32_t ntohl (uint32_t value);
  
  static uint32_t getMillis();
};

#endif
