/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include "outputstream.h"

#include <util/iovec.h>

#include <sys/uio.h>
#include <stdio.h>
#include <errno.h>

int OutputStream::writevToWrite(const struct iovec *vector, int count)
{
    int ret = 0;
    int bufSize;
    int written = 0;
    for (; count > 0; --count, ++vector)
    {
        const char *pBuf = (const char *) vector->iov_base;
        bufSize = vector->iov_len;
        if ((pBuf != NULL) && (bufSize > 0))
            written = write(pBuf, bufSize);
        if (written > 0)
            ret += written;
        else if (written < 0)
            ret = written;
        if (written < bufSize)
            break;
    }
    return ret;
}

int OutputStream::writev(IOVec &vec)
{

    return writev(vec.get(), vec.len());
}

int OutputStream::writev(IOVec &vec, int total)
{
    return writev(vec.get(), vec.len());
}


