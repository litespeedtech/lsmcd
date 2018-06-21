

#include "memcacheconn.h"
#include "memcache/lsmemcache.h"
#include <log4cxx/logger.h>
#include <util/autobuf.h>

#include <edio/multiplexer.h>
#include <socket/coresocket.h>
#include <socket/gsockaddr.h>

#include <string>

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <netinet/tcp.h>

time_t  MemcacheConn::s_curTime;
int     MemcacheConn::s_ConnTimeout = 30;

MemcacheConn::MemcacheConn()
    : m_iConnState(CS_DISCONNECTED)
    , m_iLastActiveTime(0)
    , m_iSSPort(0)
    , m_iMaxBufferSize(0)
    , m_bufIncoming(4096)
    , _Protocol(MC_UNKNOWN)
    , _ConnFlags(0)
    , m_pMultiplexer(NULL)
    , _pSasl(NULL)
    , _pLink(NULL)
    , m_pHash(NULL)
{
}


MemcacheConn::~MemcacheConn()
{
}


//void MemcacheConn::ResetReq()
//{
//
//}


void MemcacheConn::setHash(LsShmHash *pHash)
{   
    LS_DBG_M("Conn setHash %p\n", pHash); 
    m_pHash = pHash;  
}


LsShmHash  *MemcacheConn::getHash()
{   
    LS_DBG_M("Conn getHash %p\n", m_pHash); 
    return m_pHash;   
}


int MemcacheConn::protocolErr()
{

    m_iConnState = CS_CLOSING;
    return -1;
}

#include "log4cxx/logger.h"
int MemcacheConn::InitConn(int fd, struct sockaddr *pAddr)
{
    char achBuf[80];
    if (pAddr != NULL)
    {
        m_iSSPort = (unsigned short)ntohs(((struct sockaddr_in  *)pAddr)->sin_port);
        snprintf( achBuf, 80, "[%s:%d]", inet_ntoa( ((struct sockaddr_in *)pAddr)->sin_addr ),
                    m_iSSPort );
        m_peerAddr = achBuf;
        LS_DBG_M ("Memcache New connection from %s", m_peerAddr.c_str()) ;
        }
#ifdef USE_SASL
    _pSasl = new LsMcSasl();
    if (_pSasl == NULL)
    {
        LS_DBG_M("SASL initialization failed!\n");
        return -1;
    }
#endif
         int nodelay = 1;
        ::setsockopt( fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof( int ) );

    setfd( fd );

    //ResetReq();
    m_iConnState      = CS_CONNECTED;
    m_iLastActiveTime = s_curTime;

    m_bufOutgoing.clear();
    m_bufIncoming.clear();

    return getMultiplexer()->add( this, POLLIN| POLLOUT | POLLHUP|POLLERR );

}


int MemcacheConn::CloseConnection()
{
    if ( m_iConnState == CS_DISCONNECTED )
        return 0;
    if (LsMemcache::getInstance().getVerbose() > 1)
    {
        LS_INFO("<%d connection closed.\n", getfd());
    }
#ifdef USE_SASL
    if (_pSasl != NULL)
    {
        delete _pSasl;
        _pSasl = NULL;
    }
#endif
    getMultiplexer()->remove( this );
    ::close( getfd() );
    m_iConnState = CS_DISCONNECTED;
    _ConnFlags = 0;
    m_bufOutgoing.clear();
    m_bufIncoming.clear();
    setfd( -1 );
    
    return 0;
}


int MemcacheConn::connectTo(const GSockAddr *pServerAddr)
{
    int fd;
    int ret;
    Multiplexer * pMplex = getMultiplexer();
    //LOG_DBG( _pClientConn->GetPeerAddr() << "-C" << _ChannelId <<
    //" Connecting to Service Provider, socket type:" << (int)_Type );
    char szBuf[64];
    m_peerAddr = pServerAddr->toString(szBuf, 64);
    m_iSSPort   = pServerAddr->getPort();

    ret = CoreSocket::connect( *pServerAddr,
                               pMplex->getFLTag(), &fd, 1 );
    if (( fd == -1 )&&( errno == ECONNREFUSED ))
    {
        ret = CoreSocket::connect( *pServerAddr,
                                   pMplex->getFLTag(), &fd, 1 );
    }
    if ( fd != -1 )
    {
         int nodelay = 1;
        ::setsockopt( fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof( int ) );
        m_iLastActiveTime = time( NULL );
        ::fcntl( fd, F_SETFD, FD_CLOEXEC );
        setfd( fd );
        LS_DBG_M("MemcacheConn::connectTo fd:%d", fd);
        pMplex->add( this, POLLIN|POLLOUT|POLLHUP|POLLERR );
        if ( ret == 0 )
        {
            m_iConnState = CS_CONNECTED;
            onWrite();
        }
        else
            m_iConnState = CS_CONNECTING;
        return 0;
    }
    return -1;
}


int MemcacheConn::onInitConnected()
{
    int error;
    socklen_t len = sizeof( int );
    int ret = getsockopt( getfd(), SOL_SOCKET, SO_ERROR, &error, &len );

    if (( ret == -1 )||( error != 0 ))
    {
        if ( ret != -1 )
            errno = error;
        //LOG_ERR( _pClientConn->GetPeerAddr() << "-C" << _ChannelId <<
        //"Connection to Service Provider, socket type: " << (int)_Type << ", Failed, " <<
        //strerror( errno ) );
        m_iConnState = CS_CLOSING;
        return -1;
    }

    m_iConnState = CS_CONNECTED;
    //LOG_DBG( _pClientConn->GetPeerAddr() << "-C" << _ChannelId <<
    //" Connected to Service Privider, socket type: " << (int)_Type );

    onWrite();
    return 0;
}


int MemcacheConn::handleEvents(short events)
{
    if ( events & POLLIN )
    {
        onRead();
    }
    if( events & POLLOUT )
    {
        onWrite();
    }
    if ( events & POLLHUP )
    {
        onHup();
    }
    else if ( events & POLLERR )
    {
        onErr();
    }
    if ( m_iConnState == CS_CLOSING )
    {
        CloseConnection();
    }
    if (m_iConnState == CS_DISCONNECTED && getfd() == -1)
        delete this;
    return 0;
}


#include <test/testsendobj.h>

void MemcacheConn::onTimer()
{
    //TestSendObj::testSend( this );
}


int MemcacheConn::Read(char *pBuf, int len)
{
    int ret;
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
            m_iConnState = CS_CLOSING;
            ret = -1;
        }
        else
        {
            m_iLastActiveTime = s_curTime;
        }
        return ret;
    }
}


int MemcacheConn::sendACKPacket(int ptype, ReplPacketHeader *pHeader)
{
    pHeader->setType( ptype );
    pHeader->setPackLen( sizeof( *pHeader ) );
    m_bufOutgoing.append( (const char*)pHeader, sizeof( *pHeader ) );
    return 0;
}


int MemcacheConn::processIncoming()
{
    ReplPacketHeader header;
    int consumed;
    LS_DBG_M("MemcacheConn processIncoming pid:%d, addr:%p, m_bufIncoming size:%d", getpid(), this, m_bufIncoming.size());
    if (_Protocol == MC_UNKNOWN)
    {
        LS_DBG_L("MemcacheConn processIncoming 1");
        if ((unsigned char)*m_bufIncoming.begin() == (unsigned char)MC_INTERNAL_REQ)
        {
            LS_DBG_M("MemcacheConn received  MC_INTERNAL_REQ, ignore");
            _ConnFlags |= CS_INTERNAL;
            m_bufIncoming.pop_front(1);
            if (m_bufIncoming.size() <= 0)
                return 0;
            LS_DBG_M("MemcacheConn processIncoming 2");
        }
        LS_DBG_L("MemcacheConn processIncoming 2");
        _Protocol =
            (((unsigned char)*m_bufIncoming.begin() == (unsigned char)MC_BINARY_REQ) ?
            MC_BINARY : MC_ASCII);
        if (LsMemcache::getInstance().getVerbose() > 1)
        {
            LS_INFO("%d: Client using the %s protocol\n", getfd(),
                    (char *)((_Protocol == MC_BINARY) ? "binary" : "ascii"));
        }
    }
    do
    {
        LS_DBG_L("MemcacheConn processIncoming pid:%d, addr:%p, 3", getpid(), this );
        if ( m_bufIncoming.blockSize() != m_bufIncoming.size() )
        {
            int size = m_bufIncoming.size();
            AutoBuf buf( size );
            m_bufIncoming.moveTo( buf.end(), size );
            m_bufIncoming.append( buf.begin(), size );

        }
        LS_DBG_L("MemcacheConn processIncoming pid:%d, addr:%p, 4", getpid(), this);
        if (_ConnFlags & CS_RESPWAIT)   // waiting for remote response
        {
            LS_DBG_L("MemcacheConn processIncoming addr:%p 4.1 putWaitQ", this);
            LsMemcache::getInstance().putWaitQ(this);
            break;
            
        }
        LS_DBG_L("MemcacheConn processIncoming pid:%d, addr:%p,5", getpid(), this);
        if (_ConnFlags & CS_INTERNAL)  // client command can change
        {
           LS_DBG_L("MemcacheConn processIncoming pid:%d, addr:%p, 5.1 CS_INTERNAL", getpid(), this); 
            _Protocol =
                (((unsigned char)*m_bufIncoming.begin() == (unsigned char)MC_BINARY_REQ) ?
                MC_BINARY : MC_ASCII);
        }
        switch (_Protocol)
        {
            case MC_ASCII:
                LS_DBG_L("MemcacheConn processIncoming pid:%d, addr:%p,6", getpid(), this);
                consumed = LsMemcache::getInstance().processCmd(
                    m_bufIncoming.begin(), m_bufIncoming.size(), this);
                break;
            case MC_BINARY:
                LS_DBG_L("MemcacheConn processIncoming pid:%d, addr:%p,7", getpid(), this);
                consumed = LsMemcache::getInstance().processBinCmd(
                    (uint8_t *)m_bufIncoming.begin(), m_bufIncoming.size(), this);
                break;
            case MC_INTERNAL:
                LS_DBG_L("MemcacheConn processIncoming pid:%d, addr:%p,8", getpid(), this);
                consumed = LsMemcache::getInstance().processInternal(
                    (uint8_t *)m_bufIncoming.begin(), m_bufIncoming.size(), this);
                break;
            default:
                // should not get here
                LS_INFO("%d: Unknown _Protocol! [%d]\n",
                        getfd(), (int)_Protocol);
                consumed = 0;
                break;
        }
        if (consumed > 0)
        {
            m_bufIncoming.pop_front(consumed);
            LS_DBG_L("MemcacheConn processIncoming pop_front 9.1 addr:%p, m_bufIncoming size:%d", this, m_bufIncoming.size());
        }
        else if (consumed == 0)
        {
            LS_DBG_L("MemcacheConn processIncoming 10");
            CloseConnection();
        }
        // else need more data
    }
    while ((consumed > 0) && (m_bufIncoming.size() > 0));

    if (!m_bufOutgoing.empty())
    {
        Flush();
    }
    if (_Protocol == MC_INTERNAL)
        LsMemcache::getInstance().processWaitQ();

    return 0;
}


int MemcacheConn::onRead()
{
    int ret;
    if ( m_iConnState == CS_CONNECTING )
        return onInitConnected();
    if ( m_bufIncoming.full() )
        m_bufIncoming.guarantee( m_bufIncoming.size() + 4096 );
    ret = Read( m_bufIncoming.end(), m_bufIncoming.contiguous() );
    
    if ( ret > 0 )
    {
        m_bufIncoming.used( ret );
        processIncoming();
    }
    return 0;
}


int MemcacheConn::onWrite()
{
    if ( m_iConnState == CS_CONNECTING )
        return onInitConnected();
    if ( m_bufOutgoing.empty() )
    {

        getMultiplexer()->suspendWrite( this );
    }
    else
        return Flush();
    return 0;
}


int MemcacheConn::onHup()
{
    m_iConnState = CS_CLOSING;
    return 0;
}


int MemcacheConn::onErr()
{
    m_iConnState = CS_CLOSING;
    return 0;
}


int MemcacheConn::SendEx(const char *msg, int len, int flags)
{
    int ret;

    while( len > 0 )
    {
        ret = write( getfd(), msg, len );
        //assert( len == ret );
        if ( ret == -1 )
        {
            if ( errno == EINTR )
                continue;
            else if ( errno != EAGAIN )
            {
                //const char * pErrorStr = strerror( errno );
                m_iConnState = CS_CLOSING;
            }
            return 0;
        }
        else 
            return ret;
    }
    return 0;

}


int MemcacheConn::SendBuff(const char *msg, int len, int flags)
{
    int total = 0;
    if( m_bufOutgoing.empty() )
       total = SendEx(msg, len, flags);
    if(total < len)
    {
       int totalbuffed = m_bufOutgoing.append(msg + total, len - total);
       if(totalbuffed  < 0 )
           return totalbuffed;
       else 
           total += totalbuffed;
       continueWrite();
    }
    if (total > 0)
        m_iConnState = CS_CONNECTED;
    return total;
}


int MemcacheConn::Flush()
{
    if ( getfd() == -1 )
        return -1;
    while( !m_bufOutgoing.empty() )
    {
        int sent = SendEx( m_bufOutgoing.begin(), m_bufOutgoing.blockSize(), 0 );
        if ( sent > 0 )
        {
//            LOG_DBG( GetPeerAddr() <<
//                " Sent to client " << sent << " Bytes." );
            m_bufOutgoing.pop_front( sent );
        }
        else
        {
            getMultiplexer()->continueWrite( this );
            return sent;
        }

    }

    getMultiplexer()->suspendWrite( this );
    return 0;

}


void MemcacheConn::continueWrite()
{
    getMultiplexer()->continueWrite( this );
}
