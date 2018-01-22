
#include "memcachelistener.h"
#include "memcacheconn.h"


#include <edio/multiplexer.h>
#include <util/pool.h>

#include <socket/coresocket.h>
#include <socket/gsockaddr.h>

#include <fcntl.h>
#include <errno.h>
#include <netinet/tcp.h>

MemcacheListener::MemcacheListener()
    : m_pMultiplexer( NULL )
{
}

MemcacheListener::~MemcacheListener()
{
    if ( getfd() != -1 )
    {
        Stop();
    }
}


int MemcacheListener::Start()
{
    GSockAddr addr;
    if ( addr.set( _AddrStr.c_str(), 0 ) )
        return -1;
    if (addr.get()->sa_family == AF_UNIX)
        unlink(_AddrStr.c_str() + 5);

    int fd;
    int ret = CoreSocket::listen( addr, 100, &fd, 0, 0 );
    if ( ret != 0 )
    {
        return ret;
    }
    return SetSockAttr( fd, addr );
}

int MemcacheListener::SetSockAttr( int fd, GSockAddr &addr )
{
    setfd( fd );
    ::fcntl( fd, F_SETFD, FD_CLOEXEC );
    ::fcntl( fd, F_SETFL, getMultiplexer()->getFLTag());
    int nodelay = 1;
    ::setsockopt( fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof( int ) );

    return getMultiplexer()->add( this, POLLIN|POLLHUP|POLLERR );
}

int MemcacheListener::Stop()
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

int MemcacheListener::SetListenerAddr( const char * pAddr )
{
    GSockAddr addr;
    if ( !pAddr )
        return -1;
    if ( addr.set( pAddr, 0 ) )
        return -1;
    _AddrStr = pAddr;
    return 0;
}


int MemcacheListener::handleEvents( short events )
{
    char        peer_addr[24];
    int         fd;
    socklen_t   len;
    MemcacheConn * conn;

    if ( !(events & POLLIN) )
        return 0;
    while( true )
    {
        len = 24;
        fd = accept( getfd(), (struct sockaddr *)(peer_addr), &len );
        if ( fd == -1 )
        {
            break;
        }
        conn = GetConn();
        if ( !conn )
        {
            close( fd );
        }
        conn->SetMultiplexer( getMultiplexer() );
        ::fcntl( fd, F_SETFL, getMultiplexer()->getFLTag());
        if ( conn->InitConn( fd, (struct sockaddr *)peer_addr ) )
        {
            close( fd );
            break;
        }
    }
    return 0;
    
}


void MemcacheListener::RecycleConn( MemcacheConn * pConn )
{
    delete pConn;
}

MemcacheConn * MemcacheListener::GetConn()
{
    MemcacheConn * pConn;
    pConn = new MemcacheConn();
    return pConn;
    
}



