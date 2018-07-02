

#ifndef MEMCACHECONN_H
#define MEMCACHECONN_H

#include <edio/eventreactor.h>

#include <socket/gsockaddr.h>

#include <util/loopbuf.h>
#include <util/autostr.h>
#include <sys/stat.h>

#include <string>
#include <zlib.h>

#include "repl/replpacket.h"
#include "memcache/lsmchashmulti.h"

//#include "replicate/replpeer.h"

/* _ConnFlags */
#define CS_INTERNAL     0x01    // internal connection
#define CS_REMBUSY      0x02    // remote is busy
#define CS_RESPWAIT     0x04    // waiting for remote response
#define CS_CMDWAIT      0x08    // waiting to process new command

class ReplPacketHeader;

struct sockaddr;
class Multiplexer;
class LsMcSasl;
struct ReplPacketHeader;
class LsShmHash;

class MemcacheConn : public EventReactor
{

public:
    static time_t   s_curTime;
    enum
    {
        CS_DISCONNECTED,
        CS_CONNECTING,
        CS_CONNECTED,
        CS_CLOSING,
        CS_SHUTDOWN,
        CS_CLOSED
    };

    enum
    {
        MC_UNKNOWN,
        MC_ASCII,
        MC_BINARY,
        MC_INTERNAL
    };

    MemcacheConn();
    ~MemcacheConn();
    
    int InitConn( int fd, struct sockaddr * pAddr );
    int CloseConnection();
    int SendEx(const char *msg, int len, int flags);
    int SendBuff(const char *msg, int len, int flags);
    int Flush();
 
    int appendOutput(const char *msg, int len)
    {   
        if (m_bufOutgoing.size() > 4096)
        {
            Flush();
            if (m_bufOutgoing.empty() && len > 1024)
                return SendBuff(msg, len, 0);
        }
        return m_bufOutgoing.append(msg, len);  
    }
    
    bool IsSendBufferFull()
    {   return m_bufOutgoing.size() >= m_iMaxBufferSize; }
    
    bool canSendDataObject(int tobesend)
    {   return m_bufOutgoing.empty() || (m_bufOutgoing.size() + tobesend < 8192); }
    
    int connectTo( const GSockAddr * pServerAddr );
    
    void SetMultiplexer( Multiplexer * pMplx )  {   m_pMultiplexer = pMplx;  }
    
    const char * GetPeerAddr() const        {  return m_peerAddr.c_str();   }
    
    virtual int handleEvents( short events );

    virtual void onTimer();

    static void SetConnTimeout( int t )     {   s_ConnTimeout = t;   }

    int sendACKPacket( int type , ReplPacketHeader * header );
    int processIncoming();

    void continueWrite();
    
    int GetConnState( ){ return m_iConnState; }
    long GetLastActiveTime () { return m_iLastActiveTime; }

    LsMcSasl      *GetSasl() const             {   return _pSasl;   }
    uint8_t        GetProtocol() const         {   return _Protocol;   }
    uint8_t        GetConnFlags() const        {   return _ConnFlags;   }
    void           SetConnInternal()           {   _Protocol = MC_INTERNAL;   }
    void           SetRemoteBusy()             {   _ConnFlags |= CS_REMBUSY;   }
    void           ClrRemoteBusy()             {   _ConnFlags &= ~CS_REMBUSY;   }
    void           SetRespWait()               {   _ConnFlags |= CS_RESPWAIT;   }
    void           ClrRespWait()               {   _ConnFlags &= ~CS_RESPWAIT;   }
    void           SetCmdWait()                {   _ConnFlags |= CS_CMDWAIT;   }
    void           ClrCmdWait()                {   _ConnFlags &= ~CS_CMDWAIT;   }
    MemcacheConn  *GetLink() const           {   return _pLink;   }
    void           SetLink(MemcacheConn *pLink){   _pLink = pLink;   }
    void           setHash(LsShmHash *pHash);
    LsShmHash     *getHash();   
    char          *setUser(const char *user);
    char          *getUser();
    void           setSlice(LsMcHashSlice *pSlice) { m_pSlice = pSlice; };
    LsMcHashSlice *getSlice()               { return m_pSlice; }
    void           setHdrOff(LsShmOffset_t   hdrOff)   { m_iHdrOff = hdrOff; };
    LsShmOffset_t  getHdrOff()               { return m_iHdrOff; };
    void           setConnStats(MemcacheConn *pConn) { m_pConnStats = pConn; };
    MemcacheConn  *getConnStats()            { return m_pConnStats; };

    void           clearForNewConn();
    
private:    
    int onRead();
    int onWrite();
    int onHup();
    int onErr();

    int Read( char * pBuf, int len );
 
    int protocolErr();
    int forwardNoneMplx();
    
    int onInitConnected();
    
    Multiplexer * getMultiplexer() const    {   return m_pMultiplexer;   }
    
private:
    int             m_iConnState;
    long            m_iLastActiveTime;
    
    AutoStr         m_peerAddr;
    int             m_iSSPort;
       
    int             m_iMaxBufferSize;
    LoopBuf         m_bufOutgoing;
    LoopBuf         m_bufIncoming;
    uint8_t         _Protocol;      // binary or ascii
    uint8_t         _ConnFlags;
    
    Multiplexer *   m_pMultiplexer;
    LsMcSasl *      _pSasl;
    MemcacheConn *  _pLink;
    static int      s_ConnTimeout;
    LsShmHash      *m_pHash;
    char           *m_pUser;
    LsMcHashSlice  *m_pSlice;
    LsShmOffset_t   m_iHdrOff; // Used for stats!
    MemcacheConn   *m_pConnStats;
    
};

#endif
