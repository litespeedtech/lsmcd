#ifndef REPLGROUP_H
#define REPLGROUP_H
#include "lsdef.h"
#include "nodeinfo.h"
#include "sockconn.h"
#include "repllistener.h"
#include "bulkreplctx.h"
#include "replsyncauditor.h"

#include <util/dlinkqueue.h>
#include <util/hashstringmap.h>
#include <util/stringlist.h>
#include <util/autobuf.h>
#include <edio/multiplexer.h>
#include <time.h>

enum
{
    PS_REPLFULL,
    PS_REPLUNACK,
    PS_REPLUP
};

class ReplContainer;

class ReplGroup
{
public:
    ReplGroup();
    virtual ~ReplGroup();
    
    virtual int  initReplConn()=0;
    virtual int  connGroupLstnrs() = 0;
    virtual int  clearClntOnClose(const char *pAddr)=0;
    virtual int  onTimer1s() = 0;
    virtual int  onTimer30s() = 0;
    virtual bool isFullReplNeeded(uint32_t contId, const NodeInfo *pLocalInfo, const NodeInfo *pPeerInfo)=0;
    bool isLstnrProc()                  {       return m_isLstnrProc;   }
    void setLstnrProc(bool f)           {       m_isLstnrProc = f;      }

    int  reinitMultiplexer();
    int  closeListener();
    ReplListener *getListener()         {   return &m_replListener;     }

    bool addFullReplTsk(ServerConn *pFReplConn, const AutoBuf &tmSlotBuf, uint32_t startTm, uint32_t endTm);
    bool isFullReplNeeded(const char *pClntAddr, const NodeInfo &localInfo, const NodeInfo &peerInfo);
    
    void FReplDoneCallBack(ServerConn *pFReplConn, uint32_t startTm, uint32_t endTm);
    void firstLegAuditDone(ServerConn *pConn, uint32_t startTm, uint32_t endTm);
    
    int  incSyncRepl();
    int  refreshStatusFile() ;

    int  addReplTaskByRtHash(time_t startTm, time_t endTm
        , const char *localRtHash, int localLen
        , const char *peerRtHash, int peerLen, ServerConn * pConn); 
    void setStatusFormater(JsonStatus *pFormater);

    void setProcState(int s)                    {       m_ProcState = s;        }
    FullReplAuditor &getFullReplAuditor()       {       return m_fullReplAuditor;     }
    NodeInfoMgr *getNodeInfoMgr()               {       return m_pNodeInfoMgr;  }
    SockConnMgr *getSockConnMgr()               {       return m_pSockConnMgr;  }
    IncReplAuditor & getIncReplAuditor()        {       return m_replSyncAuditor;     }
    
    void setMultiplexer(Multiplexer * pMlr)     {       m_pMultiplexer = pMlr;  }
    Multiplexer * getMultiplexer()              {       return m_pMultiplexer;  }

private:
    int  notifyFRelStepDone(ServerConn *pConn, uint32_t now);
    int  syncBulkReplTime(ServerConn *pConn, uint32_t startTm, uint32_t endTm);
    void getJsonStatus(AutoBuf &rAutoBuf);

    void startSecLegAudit(ServerConn *pConn);
protected:
    bool                m_isLstnrProc;

    NodeInfoMgr        *m_pNodeInfoMgr;
    SockConnMgr        *m_pSockConnMgr; 
    
    ReplListener        m_replListener;

    JsonStatus         *m_pStatusFormater;
    
    IncReplAuditor      m_replSyncAuditor;
    FullReplAuditor     m_fullReplAuditor;
    int                 m_ProcState;
    Multiplexer         *m_pMultiplexer;
    
};

ReplGroup *getReplGroup();
void setReplGroup(ReplGroup* pReplGroup);
#endif 
