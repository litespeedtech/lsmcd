/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef CACHEOS_H
#define CACHEOS_H


/**
  *@author George Wang
  */

class IOVec;

class CacheOS
{
public:
    CacheOS() {}
    virtual ~CacheOS() {}
    //        pRet return total bytes written to output stream
    // return LS_FAIL if error occure
    //        other total bytes cached and written to output stream

    virtual int cacheWrite(const char *pBuf, int size,
                           int *pRet = 0) = 0;
    virtual int cacheWritev(IOVec &vector, int total,
                            int *pRet = 0) = 0;
    virtual bool canHold(int size) = 0;

};
#endif
