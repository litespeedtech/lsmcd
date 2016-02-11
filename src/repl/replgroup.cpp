
#include "replgroup.h"
#include "replsender.h"
#include "replconf.h"
#include "replicablelruhash.h"
#include "replregistry.h"
#include "sockconn.h"
#include "lruhashreplbridge.h"
#include "replreceiver.h"
#include "replpacket.h"
#include <sys/socket.h>
#include <edio/multiplexerfactory.h>
#include <log4cxx/logger.h>
#include <string.h>
#include <errno.h>

ReplGroup* g_pReplGroup = NULL;
ReplGroup *getReplGroup()
{
    return g_pReplGroup;
}

void setReplGroup(ReplGroup* pReplGroup)
{
    assert(g_pReplGroup == NULL);        
    g_pReplGroup = pReplGroup;
}

ReplGroup::ReplGroup()
    : m_isLstnrProc(false)
    , m_pNodeInfoMgr(NULL)
    , m_pSockConnMgr(new SockConnMgr())
    , m_pStatusFormater(NULL)
    , m_pMultiplexer(NULL)
{
}

//sockAdd:ip:port
ReplGroup::~ReplGroup ()
{
    if (isLstnrProc())
        m_pStatusFormater->removeStatusFile();
    delete m_pNodeInfoMgr;
    delete m_pSockConnMgr;
}

int ReplGroup::reinitMultiplexer()
{
    m_replListener.SetMultiplexer(MultiplexerFactory::getMultiplexer());
    return MultiplexerFactory::getMultiplexer()->add(&m_replListener
        , POLLIN|POLLHUP|POLLERR );    
}

int ReplGroup::closeListener()
{
    m_replListener.SetMultiplexer(MultiplexerFactory::getMultiplexer());
    return m_replListener.Stop();
}

bool ReplGroup::addFullReplTsk(ServerConn *pFReplConn, const AutoBuf &tmSlotBuf, uint32_t startTm, uint32_t endTm)
{
    return m_fullReplAuditor.start(pFReplConn, tmSlotBuf.begin(), tmSlotBuf.size(), startTm, endTm);
}

void ReplGroup::FReplDoneCallBack(ServerConn *pConn, uint32_t startTm, uint32_t endTm)
{
    time_t nextJumpTm = ReplRegistry::getInstance().getAllContNextTm(endTm);
    notifyFRelStepDone(pConn, nextJumpTm);
//     delete m_pBulkReplCtx;
//     m_pBulkReplCtx = NULL;
}

void ReplGroup::firstLegAuditDone(ServerConn *pConn, uint32_t startTm, uint32_t endTm)
{
    m_replSyncAuditor.doneOneAudit(endTm);
    syncBulkReplTime(pConn, startTm, endTm);
    if (!m_replSyncAuditor.isInitiator()) //void dead loop
        startSecLegAudit(pConn);    
}

void ReplGroup::startSecLegAudit(ServerConn *pConn)
{
    const char *pAddr = pConn->getPeerAddr();
   
    ClientConn *pClntConn = getSockConnMgr()->getActvLstnrByAddr(pAddr);//sAddr.c_str(), len);
    assert(pClntConn != NULL  && isLstnrProc());

    AutoBuf *pAutoBuf = m_replSyncAuditor.getSelfLstRTHash();
    assert (pAutoBuf->size() > 0);
    getReplSender()->clntSendSelfRTHash(pClntConn, pAutoBuf);
}

int ReplGroup::notifyFRelStepDone(ServerConn *pConn, uint32_t nextJumpTm)
{
//     NodeInfo *pLocalNodeInfo = getLocalNodeInfo();
    uint32_t syncTime = m_pNodeInfoMgr->getLocalNodeInfo()->getSyncTime();
    AutoBuf  sendBuf;
    sendBuf.append((const char*)&syncTime, sizeof(uint32_t));
    sendBuf.append((const char*)&nextJumpTm, sizeof(uint32_t));
    
    return pConn->sendPacket(0, RT_M2C_FREPL_STEPDONE, getReplSender()->getSeqNum()
        , sendBuf.begin(), sendBuf.size()); 
}

int ReplGroup::syncBulkReplTime(ServerConn *pConn, uint32_t startTm, uint32_t endTm)
{
    AutoBuf sendBuf;
    sendBuf.append((const char*)&startTm, sizeof(uint32_t));
    sendBuf.append((const char*)&endTm, sizeof(uint32_t));
    
    return pConn->sendPacket(0, RT_M2C_BREPL_PUSHTM, getReplSender()->getSeqNum()
        , sendBuf.begin(), sendBuf.size()); 
}

int ReplGroup::refreshStatusFile()
{
    if (!isLstnrProc())
        return LS_OK;
    AutoBuf autoBuf;
    getJsonStatus(autoBuf);
    return m_pStatusFormater->writeStatusFile(autoBuf);
}

int ReplGroup::incSyncRepl()
{
    FullReplAuditor &fRAuditor = getReplGroup()->getFullReplAuditor();
    if ( !fRAuditor.isFReplDone())
        return LS_FAIL;

    Addr2ClntConnMap_t activeLstnrs;
    m_pSockConnMgr->getAllActvLstnr(activeLstnrs);
    if (!isLstnrProc() || activeLstnrs.size() == 0 || !getIncReplAuditor().isInitiator())
        return LS_FAIL;    
    if (activeLstnrs.size() != 1)
    {
        LS_ERROR("incremental sync does not support more than 2 load balancers");
        return LS_FAIL;
    }
    const char *pLstnrAddr;
    ServerConn *pSvrConn;
    HashStringMap< ReplConn *>::const_iterator itr ;
    Addr2SvrConnMap_t::const_iterator mitr;
    for ( itr = activeLstnrs.begin(); itr != activeLstnrs.end() 
        ; itr = activeLstnrs.next ( itr ) )
    {
        pLstnrAddr = itr.second()->getPeerAddr();
        // which client is connecting to this listener svr
        for (mitr = m_pSockConnMgr->getAcptConnMap().begin(); 
             mitr != m_pSockConnMgr->getAcptConnMap().end(); 
             mitr = m_pSockConnMgr->getAcptConnMap().next(mitr) )
        {
            pSvrConn = mitr.second();
            assert(pSvrConn != NULL);
            if (pSvrConn->isConnected() 
                && isEqualIpOfAddr (pLstnrAddr, mitr.first()))
            {
                LS_DBG_M("incSyncRepl listener svr %s match the accepted client addr:%s", pLstnrAddr, mitr.first());
                m_replSyncAuditor.continueAudit((const ClientConn*)itr.second());
                break;
            }
        }    
    }
    activeLstnrs.clear();
    return LS_OK;
}

void ReplGroup::getJsonStatus(AutoBuf &rAutoBuf)
{
    bool isLeader, isSync;
    Addr2SvrConnMap_t::const_iterator mitr;
    //Addr2SvrConnMap_t &clntConnMap = m_pSockConnMgr->getAcptConnMap();
    const NodeInfo *pPeerNode;
    //ServerConn *pConn;
    const char *Ip;

    ConfWrapper &replConf       = ConfWrapper::getInstance();
    const NodeInfo *pLeaderNode = m_pNodeInfoMgr->getLeaderNode();
    const char * localSvrIp     = replConf->getLocalLsntnrIp();
    //1)local Json
    rAutoBuf.append("{\n");
    NodeInfo *pLocalInfo = m_pNodeInfoMgr->getLocalNodeInfo();
    pLocalInfo->cmpLeaderNode(pLeaderNode, isLeader, isSync); 
    
    m_pStatusFormater->init(localSvrIp, true, isLeader, false, isSync, pLocalInfo);
    m_pStatusFormater->mkJson(rAutoBuf);
    
    //2)acccepted node Json
    Ip2NodeInfoMap_t::iterator itr;
    Ip2NodeInfoMap_t &pNodeInfoMap = m_pNodeInfoMgr->getNodeInfoMap();
    for (itr = pNodeInfoMap.begin(); itr != pNodeInfoMap.end(); 
         itr = pNodeInfoMap.next(itr) )
    {
        if (itr.second() == m_pNodeInfoMgr->getLocalNodeInfo())
            continue;
        Ip = itr.first();
        pPeerNode = getNodeInfoMgr()->getNodeInfo(Ip);
        pPeerNode->cmpLeaderNode(pLeaderNode, isLeader, isSync);
                rAutoBuf.append(",\n");
                m_pStatusFormater->init(Ip, false, isLeader, false, isSync, pPeerNode);
                m_pStatusFormater->mkJson(rAutoBuf);

        /*for (mitr = clntConnMap.begin(); mitr != clntConnMap.end();
             mitr = clntConnMap.next(mitr) )
        {
            pConn = mitr.second();
            assert(pConn != NULL);
            if (pConn->isConnected() // only one active connection per LB 
                && isEqualIpOfAddr(Ip, pConn->getPeerAddr())) 
            {
                pPeerNode = itr.second();
                pPeerNode->cmpLeaderNode(pLeaderNode, isLeader, isSync);
                rAutoBuf.append(",\n");
                m_pStatusFormater->init(Ip, false, isLeader, false, isSync, pPeerNode);
                m_pStatusFormater->mkJson(rAutoBuf);
                break;
            }
        }*/    
    }
    rAutoBuf.append("}\n");
}

int ReplGroup::addReplTaskByRtHash(time_t startTm, time_t endTm
    , const char *localRtHash, int localLen
    , const char *peerRtHash, int peerLen, ServerConn *pConn)
{
    AutoBuf diffRtHash;
    diffRtHash.append((const char*)&startTm, sizeof(uint32_t));
    diffRtHash.append((const char*)&endTm, sizeof(uint32_t));
    
    int ret = cmpRTReplHash(localRtHash, localLen, peerRtHash, peerLen, diffRtHash);
    if (ret > 0)
    {
        m_replSyncAuditor.addBReplTask(pConn, diffRtHash);
    }
    else
    {
        LS_INFO("There is no data to replicate to peer at [%d:%d] time slot.", startTm, endTm);
        endTm++;
        firstLegAuditDone(pConn, startTm, endTm);
    }
    return LS_OK;
}

void ReplGroup::setStatusFormater(JsonStatus *pFormater)
{   m_pStatusFormater = pFormater;   }

bool ReplGroup::isFullReplNeeded(const char *pClntAddr, const NodeInfo &localInfo, const NodeInfo &peerInfo)
{
    Addr2ClntConnMap_t activeLstnrs;
    m_pSockConnMgr->getAllActvLstnr(activeLstnrs);
    if (!isLstnrProc() || activeLstnrs.size() == 0 || ! getNodeInfoMgr()->isSelfFirstUp())
    {
        return false;    
    }
    if (activeLstnrs.size() > 1)
    {
        LS_ERROR("incremental sync does not support more than 2 load balancers");
        return false;
    }
    if (getFullReplAuditor().isFReplDone() || getFullReplAuditor().isStarting())
    {
        return false;
    }
    if (localInfo.getSyncTime() < peerInfo.getSyncTime())
    {
        return true;
    }
    return false;
}
