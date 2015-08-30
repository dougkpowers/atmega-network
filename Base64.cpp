#include "Base64.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


static char codec[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
		       'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
		       'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
		       'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
		       'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
		       'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
		       'w', 'x', 'y', 'z', '0', '1', '2', '3',
		       '4', '5', '6', '7', '8', '9', '+', '/'};

char* Base64::Encode(char* input){

  //setup
  uint16_t length = strlen(input);
  uint16_t neededChars = length * 8 / 6;
  if ((length * 8) % 6 > 6) neededChars+=2;
  else if ((length * 8) % 6 > 0) neededChars+=1;
  uint16_t outputLength = neededChars;
  if (neededChars % 4 > 0)  //make it evenly divisible by 4
    outputLength += (4-(neededChars % 4));

  char* result = (char*)malloc(outputLength + 1);
  if (result == NULL) return NULL;
  result[outputLength] = '\0';


  //encode
  for(int i=0; i<outputLength; i++){
    uint16_t startIndex = (i / 4) * 3;
    uint32_t bank = 0;
    if (startIndex < length)
      bank = ((uint32_t)(input[startIndex])) << 16;
    if (startIndex + 1 < length)
      bank |= (((uint32_t)(input[startIndex+1])) << 8);
    if (startIndex + 2 < length)
      bank |= ((uint32_t)(input[startIndex+2]));

    char enc = '=';
    
    if(i < neededChars){
      uint8_t index = 0;
      switch (i % 4){
      case 0:
	index = (bank <<  8 >> 26);
	break;
      case 1:
	index = (bank << 14 >> 26);
	break;
      case 2:
	index = (bank << 20 >> 26);
	break;
      case 3:
	index = (bank << 26 >> 26);
	break;
      }
      enc = codec[index];
    }

    result[i] = enc;
  }

  return result;
}

char* Base64::Decode(char* input, bool inPlace){
  
  //quick error check; make sure we have a 4 byte boundary
  uint16_t len = strlen(input);
  if (len % 4 != 0)
    return NULL;

  char* buffer = input;
  uint16_t outlen = len*6/8;
  if (!inPlace){
    buffer = (char*)malloc(outlen+1);
    if (buffer == NULL)
      return NULL;
  }

  //pull out 4 bytes at a time
  for(int i=0,b=0; i<len; i+=4, b+=3){
    uint32_t bank = 0;

    //figure out 3 decoded bytes from these 4 bytes
    for(uint8_t q=0; q<64; q++){
      if (codec[q] == input[i]){
	bank |= (((uint32_t)q) << 26);
      }
      if (codec[q] == input[i+1]){
	bank |= (((uint32_t)q) << 20);
      }
      if (codec[q] == input[i+2]){
	bank |= (((uint32_t)q) << 14);
      }
      if (codec[q] == input[i+3]){
	bank |= (((uint32_t)q) << 8);
      }
    }//end for each codec value

    //populate the buffer
    buffer[b]   = (bank >> 24);
    buffer[b+1] = (bank >> 16);
    buffer[b+2] = (bank >> 8);

  }

  buffer[outlen] = '\0';
  return buffer;

}
