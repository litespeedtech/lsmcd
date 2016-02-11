
#include "repllistener.h"
#include "replgroup.h"
#include "replconf.h"
#include "sockconn.h"

#include <log4cxx/logger.h>
#include <edio/multiplexer.h>
#include <edio/multiplexerfactory.h>
#include <socket/coresocket.h>
#include <socket/gsockaddr.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <stdio.h>

ReplListener::ReplListener()
    : m_pMultiplexer( NULL )
{
}

ReplListener::~ReplListener()
{
    if ( getfd() != -1 )
    {
        //Stop();
    }
}

int ReplListener::Start(int fd)
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
    return SetSockAttr( fd, addr );
}

int ReplListener::SetSockAttr( int fd, GSockAddr &addr )
{
    setfd( fd );
    ::fcntl( fd, F_SETFD, FD_CLOEXEC );
    ::fcntl( fd, F_SETFL, getMultiplexer()->getFLTag());
    int nodelay = 1;
    ::setsockopt( fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof( int ) );

    return getMultiplexer()->add( this, POLLIN|POLLHUP|POLLERR );
}

int ReplListener::Stop()
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

int ReplListener::SetListenerAddr( const char * pAddr )
{
    GSockAddr addr;
    if ( !pAddr )
        return -1;
    if ( addr.set( pAddr, 0 ) )
        return -1;
    m_addrStr = pAddr;
    return 0;
}


const char* ReplListener::getListenerAddr ()
{
    return m_addrStr.c_str();
}    
    
//only master calls this function
int ReplListener::handleEvents( short events )
{
    struct sockaddr peerAddr;
    int         fd;
    socklen_t   len;
    ServerConn * pConn;

    if ( !(events & POLLIN) )
        return 0;
    len = sizeof(peerAddr);
    fd = accept( getfd(), (struct sockaddr *)(&peerAddr), &len );
    if ( fd == -1 )
        return -1;

    const char *pPeerIp = inet_ntoa( ((struct sockaddr_in *)&peerAddr)->sin_addr);;
    int port = (unsigned short)ntohs(((struct sockaddr_in *)&peerAddr)->sin_port);
    
    if ( !ConfWrapper::getInstance()->isAllowedIP(pPeerIp) )
    {
        close(fd);
        LS_ERROR ("Replication listener rejects the connection %s", pPeerIp);
        return -1;
    }

    pConn = new ServerConn();
    pConn->SetMultiplexer( getMultiplexer() );
    ::fcntl( fd, F_SETFD, FD_CLOEXEC );
    ::fcntl( fd, F_SETFL, getMultiplexer()->getFLTag());
    int nodelay = 1;
    ::setsockopt( fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof( int ) );
    
    if ( pConn->svrInitConn( fd, pPeerIp, port) )
    {
        close( fd );
        delete pConn;
        LS_ERROR("Replication listener failed to initialize the client connection");
        return -1;
    }
    getReplGroup()->getSockConnMgr()->addAcceptedNode(pConn->getPeerAddr(), pConn);
    LS_DBG_L("Local listener accepts client connection %s", pConn->getPeerAddr());
    return 0;
    
}


void ReplListener::RecycleConn( ReplConn * pConn )
{
    delete pConn;
}
