/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef CORESOCKET_H
#define CORESOCKET_H

#include <sys/types.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <socket/sockdef.h>


/**
  *It is a wrapper for socket related system function call.
  *@author George Wang
  */

#define INVALID_FD  -1
class GSockAddr;
class CoreSocket
{
private:    //class members
    int m_ifd;      //file descriptor
protected:

    CoreSocket(int fd = INVALID_FD)
        : m_ifd(fd)
    {}
    CoreSocket(int domain, int type, int protocol = 0)
    {
        int fd = ::socket(domain, type, protocol);
        setfd(fd);
    }
    void    setfd(int ifd) { m_ifd = ifd;  }

public:
    enum
    {
        FAIL = -1,
        SUCCESS = 0
    };

    virtual ~CoreSocket()
    {
        if (getfd() != INVALID_FD)
            close();
    }
    int     getfd() const   { return m_ifd;     }

    int     close();

    int     getSockName(SockAddr *name, socklen_t *namelen)
    {   return ::getsockname(getfd(), name, namelen);   }

    int     getSockOpt(int level, int optname, void *optval, socklen_t *optlen)
    {   return ::getsockopt(getfd(), level, optname, optval, optlen);   }

    int     setSockOpt(int level, int optname, const void *optval,
                       socklen_t optlen)
    {   return ::setsockopt(getfd(), level, optname, optval, optlen);   }

    int     setSockOpt(int optname, const void *optval, socklen_t optlen)
    {   return setSockOpt(SOL_SOCKET, optname, optval, optlen);         }

    int     setSockOptInt(int optname, int val)
    {   return setSockOpt(optname, (void *)&val, sizeof(val));  }

    int     setTcpSockOptInt(int optname, int val)
    {   return ::setsockopt(getfd(), IPPROTO_TCP, optname, &val, sizeof(int)); }

    int     setReuseAddr(int reuse)
    {   return setSockOptInt(SO_REUSEADDR, reuse);  }

    int     setRcvBuf(int size)
    {   return setSockOptInt(SO_RCVBUF, size);   }

    int     setSndBuf(int size)
    {   return setSockOptInt(SO_SNDBUF, size);   }

    static int  connect(const char *pURL, int iFLTag, int *fd,
                        int dnslookup = 0,
                        int nodelay = 1, const GSockAddr *pLocalAddr = NULL);
    static int  connect(const GSockAddr &server, int iFLTag, int *fd,
                        int nodelay = 1, const GSockAddr *pLocalAddr = NULL);
    static int  bind(const GSockAddr &server, int type, int *fd);
    static int  listen(const char *pURL, int backlog, int *fd, int nodelay,
                       int sndBuf = -1, int rcvBuf = -1);
    static int  listen(const GSockAddr &addr, int backlog, int *fd,
                       int nodelay,
                       int sndBuf = -1, int rcvBuf = -1);
    static int  cork(int fd)
    {
#if defined(linux) || defined(__linux) || defined(__linux__) || defined(__gnu_linux__)
        int cork = 1;
        return ::setsockopt(fd, IPPROTO_TCP, TCP_CORK, &cork, sizeof(int));

#elif defined(__FreeBSD__)
        int nopush = 1;
        return ::setsockopt(fd, IPPROTO_TCP, TCP_NOPUSH, &nopush, sizeof(int));
#else
        int nodelay = 0;
        return ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(int));
#endif

    }
    static int  uncork(int fd)
    {
#if defined(__FreeBSD__)
        int nopush = 0;
        return ::setsockopt(fd, IPPROTO_TCP, TCP_NOPUSH, &nopush, sizeof(int));
        //return ::setsockopt( fd, IPPROTO_TCP, TCP_NOPUSH, &nopush, sizeof( int ) );
#endif
//#else
        int nodelay = 1;
        return ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(int));
//#endif
    }
    
    static int enableFastOpen(int fd, int queLen);
    
};

#endif
