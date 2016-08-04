/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef BUFFEREDOS_H
#define BUFFEREDOS_H


#include <lsdef.h>

#include <edio/outputstream.h>
#include <edio/outputbuf.h>


class BufferedOS : public OutputStream
{
protected:
    OutputStream   *m_pOS;
    OutputBuf       m_buf;

    int writeEx(const char *pBuf, int size, int avoidCache);
    int writevEx(IOVec &vector, int avoidCache);
public:
    BufferedOS()
        : m_pOS(0)
    {}
    explicit BufferedOS(OutputStream *pOS, int initSize = 4096);
    ~BufferedOS();

    void setOS(OutputStream *pOS)
    {   m_pOS = pOS;    }

    LoopBuf *getBuf()               { return &m_buf;  }
    const LoopBuf *getBuf() const   { return &m_buf;  }

    bool isEmpty() const    { return m_buf.empty(); }

    int cacheWrite(const char *pBuf, int size)
    {
        if (m_buf.size() + size < 9000)
        {
            m_buf.append(pBuf, size);
            return size;
        }
        else
            return writeEx(pBuf, size, 0);
    }

    int cacheWritev(IOVec &vector, int total);

    int cacheWritev(IOVec &vector)
    {   return writevEx(vector, 0);       }

    virtual int write(const char *pBuf, int size);
    virtual int writev(const struct iovec *vector, int len);
    virtual int writev(IOVec &vector);
    virtual int writev(IOVec &vector, int total)
    {   return writev(vector);    }
    virtual int flush();
    virtual int close();
    
    LS_NO_COPY_ASSIGN(BufferedOS);
};

#endif
