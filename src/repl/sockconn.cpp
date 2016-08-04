
#include "sockconn.h"
#include "nodeinfo.h"
#include "replsender.h"
#include "replgroup.h"
#include "replpacket.h"
#include "replreceiver.h"
#include "replconf.h"
#include "bulkreplctx.h"

#include <edio/multiplexer.h>
#include <socket/coresocket.h>
#include <socket/gsockaddr.h>
#include <log4cxx/logger.h>
#include <util/hashstringmap.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <netinet/tcp.h>

ClientConn::ClientConn()
    : ReplConn()
{
    refreshLstRecvTm();
    refreshLstSentTm();
    
    m_HBFreq   = ConfWrapper::getInstance()->getHbFreq();
    m_HBTimeout  = ConfWrapper::getInstance()->getHbTimeout();

}


ClientConn::~ClientConn()
{
}


int ClientConn::onClearConn()
{
    m_LastSentTm = 0;
    m_LastRecvTm = 0;
    return LS_OK;
}


void ClientConn::recycle()
{
    delete this;
}


int ClientConn::handleSockCloseEvent()
{
    LS_INFO("client socket fires close event: the connection [%s -> %s] is closed"
        , getLocalAddr(), getPeerAddr());
    clearConnection();
    return 0;
}


int ClientConn::onReadSuccess()
{
    refreshLstRecvTm(); 
    return LS_OK;    
}          


int ClientConn::onSendSuccess()
{
    refreshLstSentTm(); 
    return LS_OK;    
}


int ClientConn::connectTo( const GSockAddr * pServerAddr )
{
    int fd;
    int ret;
    Multiplexer * pMplex = getMultiplexer();
    char szBuf[64]; 
    m_peerAddr = pServerAddr->toString(szBuf, 64);
    m_iSSPort  = pServerAddr->getPort();
    
    refreshLstRecvTm();
    refreshLstSentTm();
    
    const char *pSvrIp = ConfWrapper::getInstance()->getLocalLsntnrIp();
    
    struct sockaddr_in bindAddr;
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_addr.s_addr = inet_addr(pSvrIp);
    bindAddr.sin_port = 0;  
    GSockAddr bindSock(bindAddr);
    
    ret = CoreSocket::connect( *pServerAddr,
                               pMplex->getFLTag() , &fd, 1, &bindSock );
    if (( fd == -1 )&&( errno == ECONNREFUSED ))
    {
        ret = CoreSocket::connect( *pServerAddr,
                                   pMplex->getFLTag(), &fd, 1, &bindSock );
    }
    
    LS_DBG_L ( "connectTo bind addr:%s,  server addr:%s, return:%d, fd:%d "
        , szBuf, m_peerAddr.c_str(), ret, fd );

    if ( fd != -1 )
    {
        //int nodelay = 1;
        //::setsockopt( fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof( int ) );
        ::fcntl( fd, F_SETFD, FD_CLOEXEC );
        setfd( fd );
        pMplex->add( this, POLLIN|POLLOUT|POLLHUP|POLLERR );
        if ( ret == 0 )
        {
            m_iConnState = CS_CONNECTED;
            onWrite();
        }
        else
            m_iConnState = CS_CONNECTING;

        LS_DBG_L("ClientConn::connectTo m_iConnState %d", m_iConnState);
        
        struct sockaddr_in sin;
        socklen_t len = sizeof(sin);
        if (getsockname(fd, (struct sockaddr *)&sin, &len) == -1)
        {
            LS_ERROR("Replication failed to call API getsockname, error:%s", strerror(errno));
            return LS_FAIL;
        }
        
        snprintf(szBuf, 64, "%s:%d", inet_ntoa( sin.sin_addr ), ntohs(sin.sin_port));
        m_localAddr = szBuf;

        return 0;
    }
    return -1;
}


const char * ClientConn::getLocalAddr() const 
{  
    return m_localAddr.c_str(); 
}


void ClientConn::onTimer()
{
    if (isHBTimeout())
    {
        LS_WARN( "heartbeat of process[pid=%d] to server [%s -> %s] is timeout", 
            getpid(), getLocalAddr(), getPeerAddr());
        clearConnection();
    }
    else if(isHBSendTime())
    {
        getReplSender()->sockClientPingSvrStatus (this);
    }
}


int ClientConn::onWriteSuccess()
{
    getMultiplexer()->suspendWrite( this );
    return 0;
}


bool ClientConn::isHBTimeout() const
{
    return (time(NULL) - m_LastRecvTm) > m_HBTimeout ;
}


bool ClientConn::isHBSendTime() const
{
    return ( time(NULL) - m_LastSentTm ) > m_HBFreq ;
}


int ClientConn::statClntSock(char *pBuf, int size) const
{
    int len = snprintf(pBuf, size
, "client socket to listener server : localAddr:%s, serverAddr:%s \
, connectionState:%d, currTime:%ld \
, isTimeout:%s(lastRecvTime:%ld, HBTimeout:%d) \
, isHBTime:%s(lastSentTime:%ld, HBFreq:%d)"
        , getLocalAddr(), getPeerAddr()
        , m_iConnState, time(NULL)
        , isHBTimeout() ? "Yes" : "No", m_LastRecvTm, m_HBTimeout
        , isHBSendTime()? "Yes" : "No", m_LastSentTm, m_HBFreq
    );
    assert (len < size - 1 );
    return len;
}


ServerConn::ServerConn()
{}
    

ServerConn::~ServerConn()
{}    
    
    
int ServerConn::handleSockCloseEvent()
{
     LS_INFO("server socket fires close event: the connection [%s -> %s] is closed"
        , getLocalAddr(), getPeerAddr());

    getReplGroup()->clearClntOnClose(getPeerAddr());
    clearConnection();
    
    return 0;
}

const char * ServerConn::getLocalAddr() const 
{  
    return ConfWrapper::getInstance()->getLisenSvrAddr();
}


int ServerConn::svrInitConn( int fd, const char *pPeerIp, int port )
{
    char achBuf[64];
    snprintf( achBuf, 64, "%s:%d", pPeerIp, port);
    m_peerAddr = achBuf;
    LS_DBG_L("ServerConn::svrInitConn receive addr=%s", m_peerAddr.c_str());
    setfd( fd );
    m_iConnState      = CS_CONNECTED;
    
    m_bufOutgoing.clear();
    m_bufIncoming.clear();
    resetZipStream();
    
    return getMultiplexer()->add( this, POLLIN|POLLHUP|POLLERR );
}


int ServerConn::onFlushSuccess()
{
    if (m_pfullReplCtx == NULL)
    {
        getMultiplexer()->suspendWrite( this );
    }
    return LS_OK;    
}


int ServerConn::onClearConn()
{
    delete this;
    return LS_OK;
}


int ServerConn::onWriteSuccess()
{
    if ( m_pfullReplCtx != NULL )
    {
        m_pfullReplCtx->resumeBulkRepl(this); 
    }
    return 0;
}


SockConnMgr::~SockConnMgr()
{
    m_hClntConnMap.release_objects();
}

ClientConn *SockConnMgr::addClntNode(const char *pSockAddr)
{
    ClientConn *pConn = new ClientConn() ;
    pConn->setHashKey(pSockAddr);
    m_hClntConnMap.insert(pConn->getHashKey(), pConn);
    return pConn;
}

int SockConnMgr::addAcceptedNode(const char *pClntAddr, ServerConn *pConn)
{
    LS_DBG_M("addAcceptedNode sockAddr=%s", pClntAddr);
    
    Addr2SvrConnMap_t::iterator itr = m_hAcptedConnMap.find(pClntAddr);
    assert (itr == m_hAcptedConnMap.end());
    
    pConn->setHashKey(pClntAddr);
    m_hAcptedConnMap.insert(pConn->getHashKey(), pConn);

    return LS_OK;
}

bool SockConnMgr::isClntActive(const char* pClntAddr)
{
    Addr2ClntConnMap_t::iterator itr = m_hClntConnMap.find(pClntAddr);
    if (itr != m_hClntConnMap.end())
    {
        if (itr.second()->isActive())
            return true;
    }
    return false;
}

int SockConnMgr::delAcceptedNode(const char *pClntAddr)
{
    LS_DBG_L("delAcceptedNode sockAddr=%s", pClntAddr);
    
    Addr2SvrConnMap_t::iterator itr = m_hAcptedConnMap.find(pClntAddr);
    if (itr != m_hAcptedConnMap.end())
    {
        //ServerConn *pConn = itr.second();
        m_hAcptedConnMap.erase(itr);
        //delete pConn;
    }
    return LS_OK;
}

int SockConnMgr::delClntNode(const char* pSockAddr)
{
    LS_DBG_L("delClntNode sockAddr=%s", pSockAddr);
    
    Addr2ClntConnMap_t::iterator itr = m_hClntConnMap.find(pSockAddr);
    if (itr != m_hClntConnMap.end())
    {
        //ClientConn *pConn = itr.second();
        m_hClntConnMap.erase(itr);
        //delete pConn;
    }
    return LS_OK;
}

int SockConnMgr::delClntNode(const StringList &rmList)
{
    StringList::const_iterator itr;
    for ( itr = rmList.begin(); itr != rmList.end(); itr++ )
    {
        delClntNode((*itr )->c_str());
    }
    return rmList.size();
}

int SockConnMgr::getAllActvLstnr (Addr2ClntConnMap_t & clntMap) const
{
    Addr2ClntConnMap_t::iterator itr;
    ClientConn * pConn;
    for (itr = m_hClntConnMap.begin(); itr != m_hClntConnMap.end(); 
         itr = m_hClntConnMap.next ( itr ))
    {
        pConn = itr.second();
        if (pConn->isConnected())
            clntMap.insert(itr.first(), pConn);
    }
    return clntMap.size();
}

void SockConnMgr::clearAckCache()
{
    Addr2ClntConnMap_t::iterator itr;
    for (itr = m_hClntConnMap.begin(); itr != m_hClntConnMap.end(); 
         itr = m_hClntConnMap.next ( itr ))
    {
        itr.second()->clearWaitAckData();
    }
}

ClientConn * SockConnMgr::getActvLstnrByAddr(const char * pAddr) const
{
    const char *pLstnrAddr;
    HashStringMap< ReplConn *>::const_iterator itr ;
    Addr2ClntConnMap_t activeLstnrs;
    
    getAllActvLstnr(activeLstnrs);    
    for ( itr = activeLstnrs.begin(); itr != activeLstnrs.end() 
        ; itr = activeLstnrs.next ( itr ) )
    {
        pLstnrAddr = itr.second()->getPeerAddr();
        LS_DBG_M("ReplGroup::getActvLstnrByAddr pLstnrAddr:%s", pLstnrAddr);
        if (isEqualIpOfAddr(pLstnrAddr, pAddr))
            return (ClientConn*)itr.second();
    }
    return NULL;
}

int SockConnMgr::getActvLstnrConnCnt()
{
    int cnt = 0;
    Addr2ClntConnMap_t::const_iterator itr;
    for (itr = getClntConnMap().begin(); itr != getClntConnMap().end(); 
            itr = getClntConnMap().next ( itr ))
    {    
        if (itr.second()->isConnected())
            cnt++;
    }
    return cnt;
}

bool SockConnMgr::isAceptedIpActv(const char *pAcptedIp)
{
    ServerConn *pConn;
    Addr2SvrConnMap_t::const_iterator mitr;
    for (mitr = getAcptConnMap().begin();
            mitr != getAcptConnMap().end();
            mitr = getAcptConnMap().next(mitr) )
    {
        pConn = mitr.second();
        assert(pConn != NULL);
        if (pConn->isConnected()
            && isEqualIpOfAddr(pAcptedIp, pConn->getPeerAddr()))
        {
            return true;
        }
    }
    return false;
}
