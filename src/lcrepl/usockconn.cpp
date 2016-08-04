
#include "usockconn.h"

#include "lcreplconf.h"
#include "lcnodeinfo.h"
#include <edio/multiplexer.h>
#include <socket/coresocket.h>
#include <log4cxx/logger.h>
#include <util/hashstringmap.h>
#include <errno.h>

RoleTracker *UsockConn::m_pRoleData = NULL;
UsockConn::UsockConn()
    : m_iConnState( US_DISCONNECTED )
    , m_proto(UX_NONE)
{
    m_pRoleData = new RoleTracker(getReplConf()->getSliceCount());
}

UsockConn::~UsockConn()
{
    if (m_pRoleData != NULL)
    {
        delete m_pRoleData;
        m_pRoleData = NULL;
    }        
}

void UsockConn::setLocalAddr(const char *pAddr)
{
    m_localAddr.set(AF_UNIX, pAddr, 0);
}

int UsockConn::SendEx(const char* msg, int len, int flags)
{
    int ret, total;
    total = 0;
    if ( US_CLOSING == m_iConnState )
        return -1;    
    while( len > 0 )
    {
        ret = write( getfd(), msg, len );
        //assert( len == ret );
        if ( ret == -1 )
        {
            if ( errno == EINTR )
                continue;
            else if ( errno != EAGAIN )
                m_iConnState = US_CLOSING;
        }
        else if ( ret > 0 )
        {
            total += ret;
            if ( ret < len )
                break;
            len -= ret;
            msg += ret;
            continue;
        }
        break;
    }
    LS_DBG_L("usock sent bytes:%d", total);
    return total;

}

void UsockConn::closeConnection(bool bRecycle)
{
    LS_DBG_L("UsockConn::closeConnection called");
    getMultiplexer()->remove( this );
    ::close( getfd() );
    m_iConnState = US_DISCONNECTED;
    setfd( -1 );
    if ( bRecycle )
        recycle();
}

bool UsockConn::isActive() const
{
    return m_iConnState ==  US_CONNECTED
        || m_iConnState ==  US_CONNECTING; 
}

int UsockConn::handleEvents( short events )
{
    if ( events & POLLIN )
    {
        onRead();
    }
    if ( events & POLLHUP )
    {
        onHup();
    }
    else if ( events & POLLERR )
    {
        onErr();
    }
    if ( m_iConnState == US_CLOSING )
    {
        closeConnection(true);
    }
    return 0;
}

int UsockConn::onHup()
{
    LS_DBG_L("UsockConn::onHup called");
    m_iConnState = US_CLOSING;
    return 0;
}

int UsockConn::onErr()
{
    LS_DBG_L("UsockConn::onErr called");
    m_iConnState = US_CLOSING;
    return 0;
}

int UsockConn::Read( char * pBuf, int len )
{
    int ret;
    if( m_iConnState == US_CLOSED)
        return -1;
    
    while( 1 )
    {
        ret = read( getfd(), pBuf, len );
        if ( ret == -1 )
        {
            if ( errno == EINTR )
                continue;
            if ( errno == EAGAIN )
                ret = 0;
        }
        else if ( ret == 0 )
        {
            m_iConnState = US_CLOSING;
            ret = -1;
        }
        else
        {
        }
        return ret;
    }
}
