/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef INPUTSTREAM_H
#define INPUTSTREAM_H


/**
  *@author George Wang
  */
#include <stddef.h>
struct iovec;
class InputStream
{
public:
    InputStream() {};
    virtual ~InputStream() {};
    virtual int read(char *pBuf, int size) = 0;
    virtual int readv(struct iovec *vector, size_t count) = 0;
};
class CacheableIS : public InputStream
{
public:
    virtual int cacheInput(int iPreferredLen = -1) = 0;
    virtual int readLine(char *pBuf, int size) = 0;
};

class UnreadableIS : public CacheableIS
{
public:
    virtual int unread(const char *pBuf, int size) = 0;
};
#endif
