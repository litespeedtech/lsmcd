#include "lcreplsender.h"
#include "lcreplpacket.h"
#include "lcreplconf.h"
#include "replshmhelper.h"
#include "lcreplgroup.h"
#include <repl/replconn.h>
#include <repl/replregistry.h>
#include <stdio.h>

LcReplSender::LcReplSender()
    : ReplSender()
{}

LcReplSender::~LcReplSender()
{}

LcReplSender &LcReplSender::getInstance()
{
    return *(static_cast<LcReplSender *>(getReplSender()));
}

int LcReplSender::clientJoinListenSvrReq (ReplConn * pConn )
{
    LS_DBG_M("LcReplSender::clientJoinListenSvrReq getPeerAddr:%s", pConn->getPeerAddr());
    assert(pConn != NULL);
    AutoBuf autoBuf;
    StaticNodeInfo localStatic(getReplConf()->getLisenSvrAddr());
    localStatic.serialize(autoBuf);
    char pBuf[64];
    StaticNodeInfo::getSvrIPStr(localStatic.getSvrIp(), pBuf, 64);
    LS_DBG_M("ip:%s, port:%d, shm port:%d, packLen:%d"
        , pBuf, localStatic.getSvrPort()
        , localStatic.getShmSvrPort(), autoBuf.size());

    pConn->sendPacket(CONT_TID, RT_C2M_JOINREQ, 0, autoBuf.begin(), autoBuf.size());
    return LS_OK;

}

int LcReplSender::clientJoinListenSvrAck(ReplConn * pConn )
{
    if ( pConn == NULL )
        return false;
    AutoBuf autoBuf;
    StaticNodeInfo staticStatus(getReplConf()->getLisenSvrAddr());
    staticStatus.serialize(autoBuf);
    pConn->sendPacket(CONT_TID, RT_M2C_JOINACK, getSeqNum(), autoBuf.begin(), autoBuf.size());
    return LS_OK;
}

bool LcReplSender::sendLocalReplStatus(ReplConn * pConn, uint32_t uiType)
{
    if ( pConn == NULL )
        return false;

    AutoBuf autoBuf;
    LcNodeInfo* pLocalStatus = LcReplGroup::getInstance().getLocalNodeInfo();
    pLocalStatus->serialize(autoBuf);
    pConn->sendPacket(CONT_ZERO, uiType, 0, autoBuf.begin(), autoBuf.size());
    return true;
}

int LcReplSender::mstr2ClientFlipReq(ReplConn * pConn, uint32_t iContID)
{
    pConn->sendPacket(iContID, RT_M2C_MSTRFLIPING, getSeqNum(), NULL, 0);
    return LS_OK;
}

int LcReplSender::client2MstrFlipAck(ReplConn * pConn, uint32_t iContID)
{
    pConn->sendPacket(iContID, RT_C2M_FLIPINGACK, getSeqNum(), NULL, 0);
    return LS_OK;
}

int LcReplSender::mstr2ClientFlipDone(ReplConn * pConn, uint32_t iContID)
{
    pConn->sendPacket(iContID, RT_M2C_FLIPDONE, getSeqNum(), NULL, 0);
    return LS_OK;
}

int LcReplSender::newMstrElectDone(ReplConn * pConn, uint32_t iContID)
{
    AutoBuf autoBuf;
    LcNodeInfo* pLocalStatus = LcReplGroup::getInstance().getLocalNodeInfo();
    pLocalStatus->serialize(autoBuf);
    pConn->sendPacket(iContID, RT_GROUP_NEWMASTER, 0, autoBuf.begin(), autoBuf.size());

    return LS_OK;
}

int LcReplSender::bcastNewMstrElectDone(uint32_t iContID)
{
    Addr2ClntConnMap_t::iterator itr;
    Addr2ClntConnMap_t &lstrMap = LcReplGroup::getInstance().getSockConnMgr()->getClntConnMap();
    for (itr = lstrMap.begin(); itr != lstrMap.end(); itr = lstrMap.next ( itr ))
    {
        LcReplSender::getInstance().newMstrElectDone(itr.second(), iContID);
    }
}

static int sendTidHashDataFn(void * cbData, void *pUData1, void *pUData2)
{
    ReplConn *pConn     = (ReplConn *)cbData;
    uint32_t contID     = *(uint32_t *)pUData1;
    AutoBuf *pBuf       = (AutoBuf *)pUData2;    
    pConn->sendPacket(contID, RT_DATA_UPDATE, getReplSender()->getSeqNum()
        , pBuf->begin(), pBuf->size());
    return LS_OK;
}

int LcReplSender::sendTidHashData (ReplConn *pConn, int idx, uint64_t iPeerTid)
{
    LS_DBG_M("LcReplSender::sendTidHashData on idx[%d]", idx);
    return ReplShmHelper::getInstance().bcastNewTidData(idx, iPeerTid
        , pConn, sendTidHashDataFn);
}

static int bcastReplicableDataFn(void * cbData, void *pUData1, void *pUData2)
{
    Addr2ClntConnMap_t *pconMap = (Addr2ClntConnMap_t *)cbData;
    uint32_t contID             = *(uint32_t *)pUData1;
    AutoBuf *pBuf               = (AutoBuf *)pUData2;
    
    Addr2ClntConnMap_t::iterator itr;
    for ( itr = pconMap->begin(); itr != pconMap->end(); itr = pconMap->next(itr))
    {
        itr.second()->sendPacket(contID, RT_DATA_UPDATE, getReplSender()->getSeqNum(), pBuf->begin(), pBuf->size());
    }
    
    return LS_OK;
}

uint64_t LcReplSender::bcastReplicableData (int idx, uint64_t iLstTid)
{
    Addr2ClntConnMap_t &conMap = LcReplGroup::getInstance().getSockConnMgr()->getClntConnMap();
    
    return ReplShmHelper::getInstance().bcastNewTidData(idx, iLstTid
        , &conMap, bcastReplicableDataFn);
}
