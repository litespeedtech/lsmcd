/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef GSOCKADDR_H
#define GSOCKADDR_H


/**
  *@author George Wang
  */
#include <inttypes.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#define DO_NSLOOKUP     1
#define NO_ANY          2
#define ADDR_ONLY       4

class GSockAddr
{
private:
    union
    {
        struct sockaddr      *m_pSockAddr;
        struct sockaddr_in   *m_v4;
        struct sockaddr_in6 *m_v6;
        struct sockaddr_un   *m_un;
    };

    int m_len;
    int allocate(int family);
    void release();

public:
    GSockAddr()
    {
        ::memset(this, 0, sizeof(GSockAddr));
    }
    explicit GSockAddr(int family)
    {
        ::memset(this, 0, sizeof(GSockAddr));
        allocate(family);
    }
    GSockAddr(const in_addr_t addr, const in_port_t port)
    {
        ::memset(this, 0, sizeof(GSockAddr));
        set(addr, port);
    }
    explicit GSockAddr(const struct sockaddr *pAddr)
    {
        ::memset(this, 0, sizeof(GSockAddr));
        allocate(pAddr->sa_family);
        memmove(m_pSockAddr, pAddr, m_len);
    }
    GSockAddr(const GSockAddr &rhs);
    GSockAddr &operator=(const GSockAddr &rhs)
    {   return operator=(*(rhs.m_pSockAddr));    }

    GSockAddr &operator=(const struct sockaddr &rhs);
    GSockAddr &operator=(const in_addr_t addr);
    operator const struct sockaddr *() const   {   return m_pSockAddr;  }
    explicit GSockAddr(const struct sockaddr_in &rhs)
    {
        ::memset(this, 0, sizeof(GSockAddr));
        allocate(AF_INET);
        memmove(m_pSockAddr, &rhs, sizeof(rhs));
    }

    ~GSockAddr()                {   release();          }
    struct sockaddr *get()     {   return m_pSockAddr; }

    int family() const
    {   return m_pSockAddr->sa_family;  }

    int len() const
    {   return m_len;   }

    const struct sockaddr *get() const
    {   return m_pSockAddr; }
    const struct sockaddr_in *getV4() const
    {   return (const struct sockaddr_in *)m_pSockAddr;  }
    const struct sockaddr_in6 *getV6() const
    {   return (const struct sockaddr_in6 *)m_pSockAddr; }
    const char *getUnix() const
    {   return ((sockaddr_un *)m_pSockAddr)->sun_path;  }
    
    static inline void mappedV6ToV4( struct sockaddr * pAddr)
    {
        if (( AF_INET6 == pAddr->sa_family )&&
            ( IN6_IS_ADDR_V4MAPPED( &((struct sockaddr_in6 *)pAddr)->sin6_addr )))
        {
            pAddr->sa_family = AF_INET;
            memmove(&((struct sockaddr_in *)pAddr)->sin_addr.s_addr,
                    &((char *)pAddr)[20], 4);
        }
    }
    
    
    void set(const in_addr_t addr, const in_port_t port);

    void set(const in6_addr *addr, const in_port_t port,
             uint32_t flowinfo = 0);
    int set(const char *pURL, int tag);
    int set(int family, const char *pURL, int tag = 0);
    int setHttpUrl(const char *pHttpUrl, const int len);
    int parseAddr(const char *pString);
    /** return the address in string format. */
    static const char *ntop(const struct sockaddr *pAddr, char *pBuf, int len);
    static int getPort(const struct sockaddr *pAddr);

    const char *toAddrString(char *pBuf, int len) const
    {   return ntop(m_pSockAddr, pBuf, len);      }
    /** return the address and port in string format. */
    const char *toString(char *pBuf, int len) const;
    const char *toString() const;
    int getPort() const;
    void setPort(short port);

    static int compareAddr(const struct sockaddr *pAddr1,
                           const struct sockaddr *pAddr2);
};

#endif
