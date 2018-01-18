
#include "fdpasslstnr.h"
#include "usocklistener.h"
#include <log4cxx/logger.h>
#include <edio/multiplexer.h>
#include <edio/multiplexerfactory.h>
#include <socket/coresocket.h>
#include <socket/gsockaddr.h>
#include <util/mysleep.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <stdio.h>


FdPassLstnr::FdPassLstnr()
    : m_pMultiplexer( NULL )
{
}

FdPassLstnr::~FdPassLstnr()
{
    if ( getfd() != -1 )
    {
        //Stop();
    }
}

int FdPassLstnr::Start(int fd, UsocklListener *pLstnr)
{
    GSockAddr addr;
    if ( addr.set( m_addrStr.c_str(), 0 ) )
        return -1;
    if ( fd == -1)
    {
        int ret = CoreSocket::listen( addr, 100, &fd, 0, 0 );
        if ( ret != 0 )
        {
            LS_ERROR("Replication failed to start up listener socket");
            return ret;
        }
    }
    m_pusockLstnr = pLstnr;
    return SetSockAttr( fd, addr );
}

int FdPassLstnr::SetSockAttr( int fd, GSockAddr &addr )
{
    setfd( fd );
    ::fcntl( fd, F_SETFD, FD_CLOEXEC );
    ::fcntl( fd, F_SETFL, getMultiplexer()->getFLTag());
    int nodelay = 1;
    ::setsockopt( fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof( int ) );

    return getMultiplexer()->add( this, POLLIN|POLLHUP|POLLERR );
}

int FdPassLstnr::Stop()
{
    if ( getfd() != -1 )
    {
        //LOG_INFO(( "Stop listener %s.", getAddrStr() ));
        getMultiplexer()->remove( this );
        close( getfd() );
        setfd( -1 );
    }
    return 0;
}

int FdPassLstnr::SetListenerAddr( const char * pAddr )
{
    GSockAddr addr;
    if ( !pAddr )
        return -1;
    if ( addr.set( pAddr, 0 ) )
        return -1;
    m_addrStr = pAddr;
    return 0;
}
#include <memcache/lsmemcache.h>

//only master calls this function
int FdPassLstnr::handleEvents( short events )
{
    struct sockaddr peerAddr;
    int         fwFd;
    socklen_t   len;

    if ( !(events & POLLIN) )
        return 0;
    len = sizeof(peerAddr);
    fwFd = accept( getfd(), (struct sockaddr *)(&peerAddr), &len );
    if ( fwFd == -1 )
        return -1;

    DispatchData_t dispData;
    if (-1 == ::read(fwFd, &dispData, sizeof(dispData)))
    {
        // not checking for short read?
        return -1;
    }
    m_pusockLstnr->forwardFd(dispData, fwFd);
    my_sleep(100);
    close(fwFd);
    return 0;
}
