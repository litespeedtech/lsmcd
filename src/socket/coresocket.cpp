/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */

#include "coresocket.h"

#include "gsockaddr.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <string.h>
#include <unistd.h>



/**
  * @return 0, succeed
  *         -1,  invalid URL.
  *         EINPROGRESS, non blocking socket connecting are in progress
  *         Other error code occured when call socket(), connect().
*/

int  CoreSocket::connect(const char *pURL, int iFLTag, int *fd,
                         int dnslookup,
                         int nodelay, const GSockAddr *pLocalAddr)
{
    int ret;
    GSockAddr server;
    *fd = -1;
    int tag = NO_ANY;
    if (dnslookup)
        tag |= DO_NSLOOKUP;
    ret = server.set(pURL, tag);
    if (ret != 0)
        return -1;
    return connect(server, iFLTag, fd, nodelay, pLocalAddr);
}

int  CoreSocket::connect(const GSockAddr &server, int iFLTag, int *fd,
                         int nodelay, const GSockAddr *pLocalAddr)
{
    int type = SOCK_STREAM;
    int ret;
    *fd = ::socket(server.family(), type, 0);
    if (*fd == -1)
        return -1;
    if (iFLTag)
        ::fcntl(*fd, F_SETFL, iFLTag);
    if ((nodelay) && ((server.family() == AF_INET) ||
                      (server.family() == AF_INET6)))
        ::setsockopt(*fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(int));
    if (pLocalAddr)
    {
        int flag = 1;
        if (setsockopt(*fd, SOL_SOCKET, SO_REUSEADDR,
                       (char *)(&flag), sizeof(flag)) == 0)
            ::bind(*fd, pLocalAddr->get(), pLocalAddr->len());
    }
    ret = ::connect(*fd, server.get(), server.len());
    if (ret != 0)
    {
        if (!((iFLTag & O_NONBLOCK) && (errno == EINPROGRESS)))
        {
            ::close(*fd);
            *fd = -1;
        }
        return -1;
    }
    return ret;

}

int CoreSocket::listen(const char *pURL, int backlog, int *fd, int nodelay,
                       int sndBuf, int rcvBuf)
{
    int ret;
    GSockAddr server;
    ret = server.set(pURL, 0);
    if (ret != 0)
        return -1;
    return listen(server, backlog, fd, nodelay, sndBuf, rcvBuf);
}

int CoreSocket::listen(const GSockAddr &server, int backLog, int *fd,
                       int nodelay, int sndBuf, int rcvBuf)
{
    int ret;
    ret = bind(server, SOCK_STREAM, fd);
    if (ret)
        return ret;

    if (sndBuf > 4096)
        ::setsockopt(*fd, SOL_SOCKET, SO_SNDBUF, &sndBuf, sizeof(int));
    if (rcvBuf > 4096)
        ::setsockopt(*fd, SOL_SOCKET, SO_RCVBUF, &rcvBuf, sizeof(int));
    if ((nodelay) && ((server.family() == AF_INET) ||
                      (server.family() == AF_INET6)))
        ::setsockopt(*fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(int));


    ret = ::listen(*fd, backLog);

    if (ret == 0)
        return 0;
    ret = errno;
    ::close(*fd);
    *fd = -1;
    return ret;

}


int CoreSocket::bind(const GSockAddr &server, int type, int *fd)
{
    int ret;
    if (!server.get())
        return EINVAL;
    *fd = ::socket(server.family(), type, 0);
    if (*fd == -1)
        return errno;
    int flag = 1;
    if (setsockopt(*fd, SOL_SOCKET, SO_REUSEADDR,
                   (char *)(&flag), sizeof(flag)) == 0)
    {
        ret = ::bind(*fd, server.get(), server.len());
        if (!ret)
            return ret;
#ifdef IPV6_V6ONLY
        if ((EADDRINUSE == errno) && (server.family() == AF_INET6))
        {
            if (IN6_IS_ADDR_UNSPECIFIED(&(server.getV6()->sin6_addr)))
            {
                if (setsockopt(*fd, IPPROTO_IPV6, IPV6_V6ONLY,
                               (char *)(&flag), sizeof(flag)) == 0)
                {
                    ret = ::bind(*fd, server.get(), server.len());
                    if (!ret)
                        return ret;
                }
            }
        }
#endif
    }
    ret = errno;
    ::close(*fd);
    *fd = -1;
    return ret;

}


int CoreSocket::close()
{
    int iRet;
    for (int i = 0; i < 3; i++)
    {
        iRet = ::close(getfd());
        if (iRet != EINTR)   // not interupted
        {
            setfd(INVALID_FD);
            return iRet;
        }
    }
    return iRet;
}


int CoreSocket::enableFastOpen(int fd, int queLen)
{
    int ret = 1;
#if defined(linux) || defined(__linux) || defined(__linux__)
#ifndef TCP_FASTOPEN
#define TCP_FASTOPEN 23
#endif
    static int isTfoAvail = 1;
    if (!isTfoAvail)
        return -1;
    
    ret = setsockopt(fd, SOL_TCP, TCP_FASTOPEN, &queLen, sizeof(queLen));
    if ((ret == -1)&&(errno == ENOPROTOOPT))
        isTfoAvail = 0;
#endif
    return ret;
}

