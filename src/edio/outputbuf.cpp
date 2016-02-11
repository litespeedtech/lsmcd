/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include "outputbuf.h"

#include <sys/types.h>
#include <sys/uio.h>

OutputBuf::OutputBuf()
{
}
OutputBuf::~OutputBuf()
{
}

int OutputBuf::cache(const char *pBuf, int total, int written)
{
    if (written >= total)
        return total;
    int cached = total - written;
    int ret = append(pBuf + written, cached);
    if (ret > 0)
        written += ret;
    return written;
}

int OutputBuf::cache(const struct iovec *vector, int count, int written)
{
    int w1 = written;
    const char *pCurBuf = NULL;
    int  iCurSize = 0;
    while (count > 0)
    {
        int blockSize = vector->iov_len;
        if (blockSize <= w1)
        {
            w1 -= blockSize;
            --count;
            ++vector;
        }
        else
        {
            pCurBuf = (const char *)vector->iov_base + w1;
            iCurSize = blockSize - w1;
            break;
        }
    }
    while (count > 0)
    {
        int cached = iCurSize;
        if (cached <= 0)
            break;
        int ret = append(pCurBuf, cached);
        if (ret > 0)
            written += ret;
        else
            break;
        if (cached < iCurSize)
            break;
        --count;
        if (count > 0)
        {
            ++vector;
            pCurBuf = (const char *)vector->iov_base;
            iCurSize = vector->iov_len;
        }
        else
            break;
    }
    return written;
}

