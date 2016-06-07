
#include "replconn.h"
#include "replpacket.h"
#include "replreceiver.h"
#include "replconf.h"
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
#define ACKCACHESIZE 1200

ReplConn::ReplConn()
    : m_iConnState( CS_DISCONNECTED )
    , m_bufOutgoing(4096 )
    , m_bufIncoming(4096 )
    
{
    if (ConfWrapper::getInstance()->isGzipStream())
    {
        m_pDefZipStream = new GzipStream(ZIP_DEF, &m_bufOutgoing);
        m_pInfZipStream = new GzipStream(ZIP_INF, &m_bufIncoming);
    }
    else
    {
        m_pDefZipStream = NULL;
        m_pInfZipStream = NULL;
    }
    
    if (ConfWrapper::getInstance()->isSockCached())
    {
        m_pWaitAckCache = new WaitAckCache();
    }
    else 
        m_pWaitAckCache = NULL;
}


ReplConn::~ReplConn()
{
    if (m_pDefZipStream != NULL)
        delete m_pDefZipStream;
    if (m_pInfZipStream != NULL)
        delete m_pInfZipStream;
    if (m_pWaitAckCache != NULL)
        delete m_pWaitAckCache;
}


int ReplConn::protocolErr()
{
    m_iConnState = CS_CLOSING;
    return -1;
}


int ReplConn::clearConnection()
{
    getMultiplexer()->remove( this );
    ::close( getfd() );
    m_iConnState = CS_DISCONNECTED;
    setfd( -1 );
    
    m_bufOutgoing.clear();
    m_bufIncoming.clear();    
    resetZipStream();
    onClearConn();
    return LS_OK;
}


int ReplConn::closeConnection()
{
     LS_DBG_M(  "call closeConnection to close  connection[%s -> %s]", 
         getLocalAddr(), getPeerAddr());
     
    if ( m_iConnState == CS_DISCONNECTED )
        return 0;

    clearConnection();
    return 0;
}


bool ReplConn::isActive() const
{
    return m_iConnState ==  CS_CONNECTED
        || m_iConnState ==  CS_CONNECTING; 
}


int ReplConn::onInitConnected()
{
    int error;
    socklen_t len = sizeof( int );
    int ret = getsockopt( getfd(), SOL_SOCKET, SO_ERROR, &error, &len );
    
    if (( ret == -1 )||( error != 0 ))
    {
        if ( ret != -1 )
            errno = error;
        //LOG_ERR( _pClientConn->getPeerAddr() << "-C" << _ChannelId <<
        //"Connection to Service Provider, socket type: " << (int)_Type << ", Failed, " <<
        //strerror( errno ) );
        m_iConnState = CS_CLOSING;
        return -1;
    }
    
    m_iConnState = CS_CONNECTED;
    //LOG_DBG( _pClientConn->getPeerAddr() << "-C" << _ChannelId <<
    //" Connected to Service Privider, socket type: " << (int)_Type );
    
    onWrite();
    return 0;
}


int ReplConn::handleEvents( short events )
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
        handleSockCloseEvent();
    }
    return 0;
}


void ReplConn::onTimer()
{

}

bool ReplConn::IsSendBufferFull() const
{   
    return m_bufOutgoing.size() >= m_iMaxBufferSize; 
}


bool ReplConn::canSendDataObject(int tobesend)
{   
    return m_bufOutgoing.empty() || (m_bufOutgoing.size() + tobesend < 8192); 
}


int ReplConn::Read( char * pBuf, int len )
{
    int ret;
    if( m_iConnState == CS_CLOSED)
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
            m_iConnState = CS_CLOSING;
            ret = -1;
        }
        else
        {   onReadSuccess();  }
        return ret;
    }

}

int ReplConn::sendPacket(uint32_t uContID, uint32_t uiType, uint32_t seqNum
    , const char* pPayLoad, uint32_t loadLen)
{
    ReplPacketHeader header;
    header.initPack();
    header.setContID (uContID);
    header.setType ( uiType );
    header.setSequenceNum ( seqNum );
    header.setPackLen ( sizeof (header) +  loadLen );        
    sendBuff ( ( const char* ) (&header), sizeof ( ReplPacketHeader ), 0);
    if (loadLen > 0)
        sendBuff ( ( const char* ) pPayLoad, loadLen, 0);
    return sizeof (header) +  loadLen;
}


int ReplConn::processIncoming()
{
    ReplPacketHeader header;
    int consumed;
    do 
    {
        if ( m_bufIncoming.blockSize() != m_bufIncoming.size() )
        {
            int size = m_bufIncoming.size();

            AutoBuf buf( size );
            m_bufIncoming.moveTo( buf.end(), size );
            m_bufIncoming.append( buf.begin(), size );
            
        }
        consumed = header.unpack( m_bufIncoming.begin(), m_bufIncoming.size() );
        if ( consumed > 0 )
        {
            getReplReceiver()->processPacket( this, &header, m_bufIncoming.begin() + sizeof( header ) );
            m_bufIncoming.pop_front( consumed );
        }
        else if (consumed == 0 )
        {
            LS_INFO( "ReplConn::processIncoming received packet with invalid protocol, \
                and cloing client socket now\n");
            closeConnection();
        }
    }while( consumed > 0 );
    
    if ( !m_bufOutgoing.empty() )
    {
        Flush();
    }
    return 0;
}

int ReplConn::onRead()
{
    if ( m_iConnState == CS_CONNECTING )
        return onInitConnected();
    
    /*if (m_bufIncoming.size() > 1524000)
    {
        LS_DBG_M(  "m_bufIncoming.size too big=%d\n", m_bufIncoming.size() );       
        processIncoming();
        return 0;
    }*/    
    char pBuf[4096];
    int ret = Read(pBuf, 4096);
    if ( ret > 0 )
    {
        if (m_pInfZipStream != NULL)
            ret = m_pInfZipStream->popIncomingData(pBuf, ret);
        else
            m_bufIncoming.append(pBuf, ret);
        
        if (ret > 0) 
            { processIncoming(); }    
        else
        {
            m_iConnState = CS_CLOSING;
            LS_ERROR ("Replication fails to read zip packet!!!! this socket conneciton is closing");
        }
    }
    return 0;
}


int ReplConn::onWrite()
{
    //LS_DBG_L("ReplConn::onWrite called, peer addr=%s, local addr=%s, isclient=%d, ctx=%p"
    //    , getPeerAddr(), getLocalAddr(), isClient(), m_pfullReplCtx);
    if ( m_iConnState == CS_CONNECTING )
        return onInitConnected();    
    if ( !m_bufOutgoing.empty() )
    {
        if ( Flush() != 1 )
            return 0;
    }
    onWriteSuccess();
    return 0;
}


int ReplConn::onHup()
{
    m_iConnState = CS_CLOSING;
    return 0;
}


int ReplConn::onErr()
{
    m_iConnState = CS_CLOSING;
    return 0;
}


int ReplConn::SendEx(const char *msg, int len, int flags)
{
    int ret, total;
    total = 0;
    if ( CS_CLOSING == m_iConnState )
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
            {
                //const char * pErrorStr = strerror( errno );
                m_iConnState = CS_CLOSING;
            }
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
    if (total > 0)
        onSendSuccess();
    return total;

}


int ReplConn::sendBuff(const char *msg, int len, int flags)
{
    int sent = 0;
    if (m_pDefZipStream != NULL)     
        m_pDefZipStream->pushOutGoingData(msg, len);
    else
        m_bufOutgoing.append(msg, len);
    
    if (!m_bufOutgoing.empty())
    {
        sent = SendEx ( m_bufOutgoing.begin(), m_bufOutgoing.blockSize(), 0 );
        if ( sent > 0 )
        {
            m_bufOutgoing.pop_front( sent );
        }       
    }
    if (!m_bufOutgoing.empty())
    {
        continueWrite();
    }    
    return LS_OK;    
}


int ReplConn::Flush()
{
    if ( getfd() == -1 || m_iConnState != CS_CONNECTED)
        return -1;
    while( !m_bufOutgoing.empty() )
    {
        int sent = SendEx( m_bufOutgoing.begin(), m_bufOutgoing.blockSize(), 0 );
        if ( sent > 0 )
        {
            m_bufOutgoing.pop_front( sent );
        }
        else
        {
            continueWrite();
            return sent;
        }
    }
    onFlushSuccess();    
    return 1;

}


void ReplConn::continueWrite()
{   
    getMultiplexer()->continueWrite( this );
}


void ReplConn::suspendWrite()
{   
    getMultiplexer()->suspendWrite( this );
}

void ReplConn::resetZipStream()
{
    if (m_pDefZipStream != NULL && m_pInfZipStream != NULL)
    {
        m_pDefZipStream->initDef();
        m_pInfZipStream->initInf();
    }
}

uint32_t ReplConn::getOutgoingBufSize() const
{
    return m_bufOutgoing.size();        
}

const char * ReplConn::getHashKey() const 
{
    return m_hashKey.c_str();
}

void  ReplConn::setHashKey(const char* sKey)
{    
    m_hashKey = sKey;
}

int ReplConn::cacheWaitAckData(uint32_t seqNum, int contID, uint32_t lruTm)
{
    if (m_pWaitAckCache)
        return m_pWaitAckCache->addCache(seqNum, contID, lruTm);
    else
        return -1;
}

int ReplConn::clearWaitAckData(uint32_t seqNum)
{
    if (m_pWaitAckCache)
        return m_pWaitAckCache->clearCache(seqNum);
    else
        return -1;
}


int  ReplConn::getWaitAckCacheSize() const
{   
    if (m_pWaitAckCache)
        return m_pWaitAckCache->size();            
    else
        return -1;
}  

void ReplConn::clearWaitAckData()
{   
    if (m_pWaitAckCache)
        m_pWaitAckCache->clearCache();     
}

int  ReplConn::getPackedCache(AutoBuf &autoBuf) const 
{     
    if (m_pWaitAckCache)
        return m_pWaitAckCache->getPackedCache(autoBuf);
    else
        return -1;
}  

int getPackedCacheFn(void *pKey, void *pVal, void *pUData)
{
    uint32_t *contId    = (uint32_t *)pKey;
    IntDArr *pArr       = (IntDArr*)pVal;
    AutoBuf *pBuf       = (AutoBuf *)pUData;
    int size = pArr->size();
    pBuf->append((const char*)&size,  sizeof(int));
    pBuf->append((const char*)contId, sizeof(uint32_t));
    
    AutoBuf *pArrBuf = pArr->getBuf();
    pBuf->append((const char *)pArrBuf->begin(), pArrBuf->size());
    return LS_OK;
}

int findContMinMaxTm(void *pKey, void *pVal, void *pUData)
{
    IntDArr *pArr       = (IntDArr*)pVal;
    uint32_t *pTm       = (uint32_t *)pUData;
    int lastIdx         = pArr->size() - 1;
    
    assert (pArr != NULL);
    if( (uint32_t)(*pArr)[0] < pTm[0])
        pTm[0] = (*pArr)[0];
    
    if( (uint32_t)(*pArr)[lastIdx] > pTm[1])
        pTm[1] = (*pArr)[lastIdx];
        
    return LS_OK;
}

int WaitAckCache::getPackedCache(AutoBuf& autoBuf) const 
{
    if (m_overLimit || m_waitAckData.size() == 0)
        return LS_FAIL;
    
    const char *pBuf;
    IntDArr *pArr;
    uint32_t contId, lruTm;
    Int2IntArrMap contIdMap; //contId->lruTm array

    int num = m_waitAckData.size() / (sizeof(int) * 3);
    for (int i = 0 ; i < num ; ++i)
    {
        pBuf    = m_waitAckData.begin() + i * sizeof(int) * 3;
        contId  = *(uint32_t *)(pBuf + sizeof(int));
        lruTm   = *(uint32_t *)(pBuf + sizeof(int) * 2);
        if (contId == 0)
            continue;
        pArr = contIdMap.getVal(contId);
        if ( pArr == NULL )
        {
            LS_DBG_M("WaitAckCache::getPackedCache contIdMap.getVal return null, contId:%d, lruTm:%d", contId,lruTm );
            pArr = new IntDArr();
            pArr->add(lruTm);
            contIdMap.addKey(contId, pArr);
        }
        else
        {
            LS_DBG_M("WaitAckCache::getPackedCache contIdMap.getVal found contId:%d, lruTm:%d", contId,lruTm );
            pArr->add(lruTm);
        }
    }
//     tm[0] = time(NULL) + 1;
//     tm[1] = 0;
//     contIdMap.for_each(findContMinMaxTm, tm);
//     LS_DBG_M("WaitAckCache::getPackedCache,  findContMinMaxTm, min:%d, max:%d", tm[0], tm[2]);
//     autoBuf.append((const char*)tm, sizeof(uint32_t) * 2 );

    contIdMap.for_each(getPackedCacheFn, &autoBuf);
    return LS_OK;
}

WaitAckCache::WaitAckCache()
    : m_overLimit(false)
{
}

int WaitAckCache::clearCache(uint32_t seqNum)
{
    char *pBuf;
    int CHUNK, count, idx;
    CHUNK = sizeof(int) * 3;
    count = m_waitAckData.size() / CHUNK;
    LS_DBG_M("WaitAckCache::clearCache seqNum:%d", seqNum);

    if( *(uint32_t *)m_waitAckData.begin() == seqNum)
    {
        m_waitAckData.pop_front(CHUNK);
        return LS_OK;
    }
    for (idx = 1 ; idx < count ; ++idx)
    {
        pBuf = m_waitAckData.begin() + idx * CHUNK;
        LS_DBG_M("WaitAckCache::clearCache loop, seqNum:%d", *(uint32_t *)pBuf);
        if (*(uint32_t *)pBuf == seqNum)
            break;
    }
    if ( idx  < count )
    {
        memmove(m_waitAckData.begin() + idx * CHUNK, m_waitAckData.begin() + (idx+1) * CHUNK,
                (count - idx -1) * CHUNK);
        m_waitAckData.used(-CHUNK);
    }
    else 
        LS_ERROR("seqNum:%d is not found, total element:%d", seqNum, count );
    return LS_FAIL;
}

int WaitAckCache::addCache(uint32_t seqNum, int contID, uint32_t lruTm)
{
    LS_DBG_M("WaitAckCache::addCache buffer size:%d, seqNum:%d", m_waitAckData.size(), seqNum);
    if (m_waitAckData.size() >= ACKCACHESIZE)
    {
        LS_ERROR("Too much data is being lost, data can't be cached");
        m_overLimit = true;
        m_waitAckData.clear();
        return LS_FAIL;
    }
    m_waitAckData.append((const char*)&seqNum, sizeof(uint32_t));
    m_waitAckData.append((const char*)&contID, sizeof(uint32_t));
    m_waitAckData.append((const char*)&lruTm , sizeof(uint32_t));
    return LS_OK;
}


