/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef BASE64_H
#define BASE64_H


/**
  *@author George Wang
  */

class Base64
{
public:
    Base64();
    ~Base64();
    static int decode(const char *encoded, int encodeLen, char *decoded);
    static int encode(const char *decoded, int decodedLen, char *encoded);
    static int encode_length(int len) { return (((len + 2) / 3) * 4); };
    static const unsigned char *getDecodeTable();

};

#endif
