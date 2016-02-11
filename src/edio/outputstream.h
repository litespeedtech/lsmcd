/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef OUTPUTSTREAM_H
#define OUTPUTSTREAM_H

/**
  *@author George Wang
  */
struct iovec;
class IOVec;
class OutputStream
{
public:
    OutputStream() {};
    virtual ~OutputStream() {};
    // Output stream interfaces
    virtual int write(const char *pBuf, int size) = 0;
    virtual int writev(const struct iovec *vector, int count) = 0;
    virtual int writev(IOVec &vector);   // = 0;
    virtual int writev(IOVec &vector, int total);
    virtual int flush() = 0;
    virtual int close() = 0;
    int writevToWrite(const struct iovec *vector, int count);

};


#endif
