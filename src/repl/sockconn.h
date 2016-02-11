

#ifndef SOCKCONN_H
#define SOCKCONN_H

#include "replconn.h"
#include <util/autostr.h>
#include <util/hashstringmap.h>


class BulkReplCtx;

#define FULLREPLBUFSZ  65536

class ClientConn : public ReplConn
{
public:
    ClientConn();
    virtual ~ClientConn();
    void recycle();
    int connectTo( const GSockAddr * pServerAddr );
    
    time_t getLastSentTime ( )          {   return m_LastSentTm;       }
    bool   isHBTimeout() const;
    void   refreshLstRecvTm()           {   m_LastRecvTm = time(NULL); } 
    
    int    statClntSock(char *pBuf, int size) const;
    virtual const char * getLocalAddr() const;
    virtual void onTimer();
protected:
    void   refreshLstSentTm ( )            {   m_LastSentTm = time(NULL); } 
    bool   isHBSendTime() const;
    virtual int handleSockCloseEvent();
    virtual int onReadSuccess();
    virtual int onWriteSuccess();
    virtual int onSendSuccess();
    virtual int onClearConn();
private:
    time_t          m_LastSentTm;
    time_t          m_LastRecvTm;
    int             m_HBFreq;
    int             m_HBTimeout;
    
    AutoStr         m_localAddr;//ip:port
    int             m_iSSPort;
};

typedef HashStringMap<ClientConn *> Addr2ClntConnMap_t;


class ServerConn : public ReplConn
{
public:
    ServerConn();
    virtual ~ServerConn();
    int svrInitConn( int fd,  const char *pPeerIp, int port);
    //const char * getPeerName() const                    {       return m_peerName.c_str();      }
    virtual const char * getLocalAddr() const;
    void   setBulkReplCtx(BulkReplCtx *pfullReplCtx)    {       m_pfullReplCtx = pfullReplCtx;  } 
protected:
    virtual int handleSockCloseEvent();
    virtual int onFlushSuccess();
    virtual int onWriteSuccess();
    virtual int onClearConn();
private:
    //AutoStr         m_peerName; //real source ip of server accept socket
    BulkReplCtx    *m_pfullReplCtx;
};

typedef HashStringMap<ServerConn *> Addr2SvrConnMap_t;


class SockConnMgr
{
public:
    ~SockConnMgr();
    Addr2ClntConnMap_t &getClntConnMap()   {       return m_hClntConnMap;  }
    Addr2SvrConnMap_t  &getAcptConnMap()   {       return m_hAcptedConnMap;}
    bool isClntActive(const char *pSockAddr);
    ClientConn * addClntNode(const char *pSockAddr);
    int  delClntNode(const char *pSockAddr);
    int  delClntNode(const StringList &rmList);
    
    int  addAcceptedNode(const char *pClntAddr, ServerConn *pConn);
    int  delAcceptedNode(const char *pClntAddr);
    int  getAllActvLstnr (Addr2ClntConnMap_t & clntMap) const;
    ClientConn * getActvLstnrByAddr(const char * pAddr) const;
    int  getAcptedConnCnt()     {       return m_hAcptedConnMap.size();} 
    int  getActvLstnrConnCnt();
    void clearAckCache();
private:
    Addr2ClntConnMap_t  m_hClntConnMap;       //out connection
    Addr2SvrConnMap_t   m_hAcptedConnMap;     //in  connection    
};

#endif
