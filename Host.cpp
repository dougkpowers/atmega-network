/*
 * These are various functions whose implementation will likely vary
 * from host to host.  For the sake of writing code that can easily
 * be ported from one to another host, these host dependent implementations
 * have been consolidated here.
 */

#include "hostutil.h"
#include "Host.h"

#if ARDUINO >= 100
#include <Arduino.h> // Arduino 1.0
#else
#include <Wprogram.h> // Arduino 0022
#endif

//make these functions available in pure c
void hostinit() { Host::init(); }
uint16_t htons (uint16_t value) { return Host::htons(value); }
uint16_t ntohs (uint16_t value) { return Host::ntohs(value); }
uint32_t htonl (uint32_t value) { return Host::htonl(value); }
uint32_t ntohl (uint32_t value) { return Host::ntohl(value); }
uint32_t host_millis() {return Host::getMillis(); }

//these functions simply call the macro
uint16_t Host::htons (uint16_t value){ return HTONS(value); }
uint16_t Host::ntohs (uint16_t value){ return NTOHS(value); }
uint32_t Host::htonl (uint32_t value){ return HTONL(value); }
uint32_t Host::ntohl (uint32_t value){ return NTOHL(value); }

//return the Arduino version of millis
uint32_t Host::getMillis(){
  return millis();
}

int put_serial(char c, FILE *t){
  if (t != stdout && t!= stderr) return EOF;
  Serial.print(c);
  return c;
}

void Host::init(){
  static bool initialized = false;
  if (initialized) return;

  //see http://www.nongnu.org/avr-libc/user-manual/group__avr__stdio.html
  fdevopen(&put_serial, NULL);

  //seed the RNG with some random values read from analog pin 3
  for(uint16_t i=0; i<1000; i++){
    word val = analogRead(3);
    if (val > 100 && val < 1000){
      randomSeed(val);
      break;
    }
  }
  
  initialized = true;
}
