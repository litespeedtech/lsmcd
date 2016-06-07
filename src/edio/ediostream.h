/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef EDIOSTREAM_H
#define EDIOSTREAM_H


/**
  *@author George Wang
  */
#include <errno.h>
#include <inttypes.h>
#include <unistd.h>
#include <edio/eventreactor.h>
#include <edio/flowcontrol.h>
#include <edio/inputstream.h>
#include <edio/outputstream.h>
#include <util/iovec.h>
#include <edio/multiplexer.h>


class EdIS : public InputStream, virtual public InputFlowControl
{
public:
    EdIS()  {}
    virtual ~EdIS() {}

    virtual int  onRead() = 0;
};

class EdOS : public OutputStream, virtual public OutputFlowControl
{
public:
    EdOS()  {}
    virtual ~EdOS() {}

    virtual int  onWrite() = 0;

};

class Multiplexer;
class LoopBuf;
class EdStream : public EventReactor, public EdIS,
    public EdOS
{
    Multiplexer *m_pMplex;

    virtual int handleEvents(short event);
    int regist(Multiplexer *pMplx, int event = 0);
protected:
    void setMultiplexer(Multiplexer *pMplx)
    {   m_pMplex = pMplx; }
public:
    EdStream()
        : m_pMplex(0) {};
    EdStream(int fd, Multiplexer *pMplx, int events = 0)
        : EventReactor(fd)
        , m_pMplex(pMplx)
    {   regist(pMplx, events);  };
    ~EdStream();
    void init(int fd, Multiplexer *pMplx, int events)
    {
        setfd(fd);
        m_pMplex = pMplx;
        regist(pMplx, events);
    }

    virtual void continueRead()     {   m_pMplex->continueRead(this);   }
    virtual void suspendRead()      {   m_pMplex->suspendRead(this); }

    int read(char *pBuf, int size);
    int readv(struct iovec *vector, size_t count);
    virtual int onRead() = 0;

    virtual void continueWrite()    {   m_pMplex->continueWrite(this);  }
    virtual void suspendWrite()     {   m_pMplex->suspendWrite(this);}
    virtual int onWrite() = 0;
    int write(const char *buf, int len)
    {
        int ret;
        while (1)
        {
            ret = ::write(getfd(), buf, len);
            if (ret == -1)
            {
                if (errno == EAGAIN)
                    ret = 0;
                if (errno == EINTR)
                    continue;
            }
            if (ret < len)
                resetRevent(POLLOUT);
            else
                setRevent(POLLOUT);
            return ret;
        }
    }

    int writev(const struct iovec *iov, int count)
    {
        int ret;
        while (1)
        {
            ret = ::writev(getfd(), iov, count);
            if (ret == -1)
            {
                if (errno == EINTR)
                    continue;
                if (errno == EAGAIN)
                {
                    resetRevent(POLLOUT);
                    ret = 0;
                }
            }
            else
                setRevent(POLLOUT);
            return ret;
        }
    }

    int writev(IOVec &vector)
    {
        return writev(vector.get(), vector.len());
    }

    int writev(IOVec &vector, int total)
    {   return writev(vector);        }

    int write(LoopBuf *pBuf);

    Multiplexer *getMultiplexer() const
    {   return m_pMplex;  }

    virtual int onHangup();
    virtual int onError() = 0;
    virtual int onEventDone(short event) = 0;
//    virtual bool wantRead() = 0;
//    virtual bool wantWrite() = 0;
//    void updateEvents();
    int close();
    int flush()    {   return LS_OK;    }
    int getSockError(int32_t *error);

    LS_NO_COPY_ASSIGN(EdStream);
    
};


#endif
