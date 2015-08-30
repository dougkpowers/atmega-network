#ifndef BASE64_H
#define BASE64_H

class Base64{

 public:
  static char* Encode(char* input);
  static char* Decode(char* output, bool inPlace = true);

};

#endif
