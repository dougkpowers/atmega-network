#ifndef HOSTUTIL_H
#define HOSTUTIL_H

#include "Host.h"

#if defined(__cplusplus)                                                      
extern "C" {                                                                  
#endif                                                                        

  void hostinit();
  uint16_t htons (uint16_t value);
  uint16_t ntohs (uint16_t value);
  uint32_t htonl (uint32_t value);
  uint32_t ntohl (uint32_t value);
  uint32_t host_millis();

#if defined(__cplusplus)                                                      
}  
#endif

#endif
