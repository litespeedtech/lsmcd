/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include "ediostream.h"
#include <util/iovec.h>
#include <util/loopbuf.h>

#include <log4cxx/logger.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

EdStream::~EdStream()
{
    close();
}

int EdStream::regist(Multiplexer *pMultiplexer, int events)
{
    if (pMultiplexer)
        return pMultiplexer->add(this, events);
    return LS_OK;
}

int EdStream::close()
{
    if (getfd() != -1)
    {
        if (m_pMplex)
        {
            m_pMplex->remove(this);
        }
        //::shutdown( getfd(), SHUT_RDWR );
        ::close(getfd());
        setfd(-1);
    }
    return LS_OK;
}


//void EdStream::updateEvents()
//{
//    int iEvents = getEvents();
//    int newEvent = iEvents & ~(POLLIN|POLLOUT);
//    if ( wantRead() )
//        newEvent |= POLLIN;
//    if ( wantWrite() )
//        newEvent |= POLLOUT;
//    if ( newEvent != iEvents )
//    {
//        setMask( newEvent );
//    }
//}

int EdStream::handleEvents(short event)
{
    int ret = 0;
    LS_DBG_L("EdStream::handleEvent(), fd: %d, event: %hd", getfd(), event);
    if (event & POLLIN)
    {
        ret = onRead();
        if (!getAssignedRevent())
            goto EVENT_DONE;
    }
    if (event & POLLHUP)
    {
        if ((ret != -1) || (getHupCounter() > 50))
            ret = onHangup();
        else if (getHupCounter() > 100)
            abort();
        if (!getAssignedRevent())
            goto EVENT_DONE;
    }
    if ((ret != -1) && (event & POLLHUP))
    {
        ret = onHangup();
        if (!getAssignedRevent())
            return LS_OK;
    }
    if ((ret != -1) && (event & POLLOUT))
    {
        ret = onWrite();
        if (!getAssignedRevent())
            goto EVENT_DONE;
    }
    if ((ret != -1) && (event & POLLERR))
    {
        ret = onError();
        if (!getAssignedRevent())
            return LS_OK;
    }
EVENT_DONE:    
    if (ret != -1)
        onEventDone(event);
    return LS_OK;
}

int EdStream::read(char *pBuf, int size)
{
    int ret = ::read(getfd(), pBuf, size);
    if (ret < size)
        resetRevent(POLLIN);
    if (!ret)
    {
        errno = ECONNRESET;
        return LS_FAIL;
    }
    if ((ret == -1) && ((errno == EAGAIN) || (errno == EINTR)))
        return LS_OK;
    return ret;
}

int EdStream::readv(struct iovec *vector, size_t count)
{
    int ret = ::readv(getfd(), vector, count);
    if (!ret)
    {
        errno = ECONNRESET;
        return LS_FAIL;
    }
    if (ret == -1)
    {
        resetRevent(POLLIN);
        if ((errno == EAGAIN) || (errno == EINTR))
            return LS_OK;
    }
    return ret;

}


int EdStream::onHangup()
{
    //::shutdown( getfd(), SHUT_RD );
    return LS_OK;
}

/** No descriptions */
int EdStream::getSockError(int32_t *error)
{
    socklen_t len = sizeof(int32_t);
    return getsockopt(getfd(), SOL_SOCKET, SO_ERROR, error, &len);
}

int EdStream::write(LoopBuf *pBuf)
{
    if (pBuf == NULL)
    {
        errno = EFAULT;
        return LS_FAIL;
    }
    IOVec iov;
    pBuf->getIOvec(iov);
    int ret = writev(iov);
    if (ret > 0)
        pBuf->pop_front(ret);
    return ret;
}


