/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */

#include "socketlistener.h"
#include "sockconnprocessor.h"

#include <edio/multiplexer.h>
#include <edio/multiplexerfactory.h>
#include <log4cxx/logger.h>
#include <socket/coresocket.h>
#include <socket/gsockaddr.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>


static void no_timewait( int fd )
{
    struct linger l = { 1, 0 };
    setsockopt( fd, SOL_SOCKET, SO_LINGER, &l, sizeof( l ) );
}


SocketListener::SocketListener( const char *pName, const char *pAddr )
    : EventReactor()
    , m_sName( pName )
    , m_sAddr( pAddr )
    , m_iPort( 0 )
    , m_iBinding( 0xffffffff )
    , m_pProcessor( NULL )
{
}


SocketListener::SocketListener()
    : m_sName( NULL )
    , m_sAddr( NULL )
    , m_iPort( 0 )
    , m_iBinding( 0xffffffff )
    , m_pProcessor( NULL )
{
}


SocketListener::~SocketListener()
{
    if ( m_pProcessor != NULL )
        delete m_pProcessor;
}

int SocketListener::getType() const
{   return m_pProcessor? m_pProcessor->getType() : 0;                 }


void SocketListener::setConnProc( SockConnProcessor *pProcessor, int iType )
{
    if ( m_pProcessor != NULL )
        delete m_pProcessor;
    m_pProcessor = pProcessor;
    m_pProcessor->setListener( this );
}


SockConnProcessor *SocketListener::getConnProc() const
{
    return m_pProcessor;
}


void SocketListener::clearConnProc()
{
    m_pProcessor = NULL;
}


int SocketListener::getPort() const
{
    return m_iPort;
}


int SocketListener::setSockAttr( int fd, GSockAddr &addr )
{
    Multiplexer *pMplx = MultiplexerFactory::getMultiplexer();
    setfd( fd );
    ::fcntl( fd, F_SETFD, FD_CLOEXEC );
    ::fcntl( fd, F_SETFL, pMplx->getFLTag());
    int nodelay = 1;
    ::setsockopt( fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof( int ) );
#ifdef TCP_DEFER_ACCEPT
    nodelay = 30;
    ::setsockopt( fd, IPPROTO_TCP, TCP_DEFER_ACCEPT, &nodelay, sizeof( int ) );
#endif
    m_iPort = addr.getPort();
    return pMplx->add( this, POLLIN|POLLHUP|POLLERR );
}


int SocketListener::assign( int fd, struct sockaddr * pAddr )
{
    GSockAddr addr( pAddr );
    char achAddr[128];
    if (( addr.family() == AF_INET )&&( addr.getV4()->sin_addr.s_addr == INADDR_ANY ))
    {
        snprintf( achAddr, 128, "*:%hu", addr.getPort() );
    }
    else if (( addr.family() == AF_INET6 )&&( IN6_IS_ADDR_UNSPECIFIED ( &addr.getV6()->sin6_addr ) ))
    {
        snprintf( achAddr, 128, "[::]:%hu", addr.getPort() );
    }
    else
        addr.toString( achAddr, 128 );
    LS_NOTICE( "Recovering server socket: [%s]", achAddr );
    setName( achAddr );
    setAddrStr( achAddr );
    return setSockAttr( fd, addr );
}


int SocketListener::start()
{
    int ret, fd;
    GSockAddr addr;
    SockConnProcessor *pProc;
    if (( pProc = getConnProc()) == NULL )
        return LS_FAIL;
    if ( addr.set( getAddrStr(), 0 ))
        return errno;
    if (( ret = pProc->startListening( addr, fd )) != 0 )
        return ret;
    return setSockAttr( fd, addr );;
}


int SocketListener::addConns( struct conn_data * pBegin,
                              struct conn_data *pEnd, int *iCount )
{
    struct conn_data * pCur = pBegin;
    int flag, fd, n = pEnd - pBegin;

    while( pCur < pEnd )
    {
        if ( m_pProcessor->checkAccess( pCur ))
        {
            no_timewait( pCur->fd );
            close( pCur->fd );
            pCur->fd = -1;
            --(*iCount);
            --n;
        }
        ++pCur;
    }
    if ( n <= 0 )
        return LS_OK;
    pCur = pBegin;
    flag = MultiplexerFactory::getMultiplexer()->getFLTag();
    while( pCur < pEnd )
    {
        fd = pCur->fd;
        if ( fd != -1 )
            m_pProcessor->addNewConn( pCur, flag );
        ++pCur;
    }
    return LS_OK;
}


int SocketListener::handleEvents(short event)
{
    if ( !connProcIsSet())
        return LS_FAIL;

    static struct conn_data conns[SLCONN_BATCH_SIZE];
    struct conn_data * pEnd = &conns[SLCONN_BATCH_SIZE];
    struct conn_data * pCur = conns;
    int iCount = 0;

    //int allowed = SLCONN_BATCH_SIZE;
    while( pCur < pEnd )
    {
        socklen_t len = 24;
        pCur->fd = ::accept( getfd(), (struct sockaddr *)(pCur->achPeerAddr), &len );
        if ( pCur->fd == -1 )
        {
            resetRevent( POLLIN );
            if (( errno != EAGAIN )&&( errno != ECONNABORTED )
                &&( errno != EINTR ))
            {
                LS_ERROR( "SocketListener::handleEvents(): [%s] can't accept:%s!",
                          getAddrStr(), strerror( errno ));
            }
            break;
        }
        GSockAddr::mappedV6ToV4((struct sockaddr *)(pCur->achPeerAddr));
        ++pCur;
    }

    if ( pCur > conns )
    {
        iCount = pCur - conns;
        addConns( conns, pCur, &iCount );
    }

    m_pProcessor->updateStats( iCount);
    LS_DBG_H( "[%s] %d connection(s) accepted!", getAddrStr(), iCount );
    return LS_OK;
}


int SocketListener::stop()
{
    if ( getfd() != -1 )
    {
        LS_INFO( "Stop listener %s.", getAddrStr());
        MultiplexerFactory::getMultiplexer()->remove( this );
        close( getfd() );
        setfd( -1 );
        return LS_OK;
    }
    return EBADF;
}



