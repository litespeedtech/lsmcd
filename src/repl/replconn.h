

#ifndef REPLCONN_H
#define REPLCONN_H

#include "gzipstream.h"
#include "repldef.h"
#include <util/autostr.h>
#include <edio/eventreactor.h>
#include <socket/gsockaddr.h>
#include <util/loopbuf.h>
#include <util/hashstringmap.h>

#include <sys/stat.h>
#include <string>
#include <zlib.h>


class ReplPacketHeader;

struct sockaddr;
class Multiplexer;
class BulkReplCtx;
#define FULLREPLBUFSZ  65536
class WaitAckCache
{
public:
    WaitAckCache();
    int  size() const    {       return m_waitAckData.size();    }
    int  getPackedCache(AutoBuf & autoBuf) const;
    int  addCache(uint32_t seqNum, int contID, uint32_t lruTm);
    int  clearCache(uint32_t seqNum);
    void clearCache()    {       m_waitAckData.clear();          }
private:
    bool            m_overLimit;
    //LoopBuf         m_waitAckData;
    AutoBuf         m_waitAckData;
};

class ReplConn : public EventReactor
{
public:
    enum
    {
        CS_DISCONNECTED,
        CS_CONNECTING,
        CS_CONNECTED,
        CS_CLOSING,
        CS_SHUTDOWN,
        CS_CLOSED
    };

    ReplConn();
    virtual ~ReplConn();
    
    int closeConnection();
    int clearConnection();
    
    int SendEx(const char *msg, int len, int flags); 
    int sendBuff(const char *msg, int len, int flags);
    int flush();

    bool IsSendBufferFull() const;
    bool canSendDataObject(int tobesend);
    
    void SetMultiplexer( Multiplexer * pMplx )  {       m_pMultiplexer = pMplx;         }

    const char * getPeerAddr() const            {       return m_peerAddr.c_str();      }
    virtual const char * getLocalAddr() const   {       return NULL;                    }    
    virtual int handleEvents( short events );
    virtual void onTimer();

    int processIncoming();
    int sendPacket(uint32_t uContID, uint32_t uiType, uint32_t seqNum
        , const char* pPayLoad, uint32_t loadLen);
    
    void continueWrite();
    void suspendWrite();
    void resetZipStream();
    bool isActive() const;
    bool isConnected() const                    {   return m_iConnState ==  CS_CONNECTED;  } 

    uint32_t getOutgoingBufSize() const ;
    
    const  char * getHashKey() const;
    void   setHashKey(const char* sKey);
    
    int    cacheWaitAckData(uint32_t seqNum, int contID, uint32_t lruTm);
    int    clearWaitAckData(uint32_t seqNum);
    int    getWaitAckCacheSize() const;
    void   clearWaitAckData();
    int    getPackedCache(AutoBuf &autoBuf) const;
protected:
    virtual int handleSockCloseEvent()          {       return 0;                   }
    int onRead();
    int onWrite();
    int onHup();
    int onErr();

    int Read( char * pBuf, int len );
    int protocolErr();
    int forwardNoneMplx();
    
    int Flush();
    int onInitConnected();
    
    Multiplexer * getMultiplexer() const        {   return m_pMultiplexer;   }
    
    virtual int onReadSuccess()         {       return 0;       }
    virtual int onSendSuccess()         {       return 0;       }
    virtual int onWriteSuccess()        {       return 0;       }
    virtual int onFlushSuccess()        {       return 0;       }
    virtual int onClearConn()           {       return 0;       }
protected:
    int             m_iConnState;
    AutoStr         m_peerAddr;
    AutoStr         m_localAddr;//ip:port
    int             m_iSSPort;
    Multiplexer    *m_pMultiplexer;
        
    int             m_iMaxBufferSize;
    LoopBuf         m_bufOutgoing;
    LoopBuf         m_bufIncoming;
   
    GzipStream     *m_pDefZipStream;
    GzipStream     *m_pInfZipStream;
    
    AutoStr         m_hashKey;
    WaitAckCache    m_waitAckCache;
};

#endif
