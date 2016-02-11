/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef OUTPUTBUF_H
#define OUTPUTBUF_H


/**
  *@author George Wang
  */

#include <util/loopbuf.h>
struct iovec;
class OutputBuf : public LoopBuf
{
public:
    OutputBuf();
    OutputBuf(int initSize)
        : LoopBuf(initSize)
    {}
    ~OutputBuf();

    int cache(const char *pBuf, int total, int written);
    int cache(const struct iovec *vector, int count, int written);

};

#endif
