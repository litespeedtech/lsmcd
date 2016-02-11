/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include "cacheos.h"

#include <util/iovec.h>

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>

int CacheOS::cacheWritev(IOVec &vec, int total, int *pRet)
{
    int ret = 0;
    int bufSize;
    int written = 0;
    int count = vec.len();
    const struct iovec *vector = vec.get();
    for (; count > 0; --count, ++vector)
    {
        const char *pBuf = (const char *) vector->iov_base;
        bufSize = vector->iov_len;
        if ((pBuf != NULL) && (bufSize > 0))
            written = cacheWrite(pBuf, bufSize, pRet);
        if (written > 0)
            ret += written;
        if (ret < 0)
            break;
        if (written < bufSize)
            break;
    }
    return ret;

}


