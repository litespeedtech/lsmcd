
#include "replsender.h"
#include "nodeinfo.h"
#include "replpacket.h"
#include "replconn.h"
#include "replregistry.h"
#include "replicablelruhash.h"
#include "replgroup.h"
#include "lruhashreplbridge.h"
#include <socket/gsockaddr.h>
#include <shm/lsshmhash.h>
#include <log4cxx/logger.h>
#include <util/hashstringmap.h>

ReplSender * g_pReplSender = NULL;
ReplSender * getReplSender()
{
    return g_pReplSender;
}

void setReplSender (ReplSender *pSender)
{
    assert(g_pReplSender == NULL);
    g_pReplSender = pSender;
}

ReplSender::ReplSender()
    : m_uiSequenceNum ( 0 )
{}

ReplSender::~ReplSender()
{}

uint32_t ReplSender::getSeqNum()
{
    m_uiSequenceNum++;

    if ( m_uiSequenceNum == 0 )
        m_uiSequenceNum++;

    return m_uiSequenceNum;
}

bool ReplSender::sendLocalReplStatus(ReplConn * pConn, uint32_t uiType)
{
    if ( pConn == NULL )
        return false;
    
    NodeInfo *pLocalNodeInfo = getReplGroup()->getNodeInfoMgr()->getLocalNodeInfo();
    AutoBuf autoBuf;
    pLocalNodeInfo->serialize(autoBuf);
    pConn->sendPacket(0, uiType, 0, autoBuf.begin(), autoBuf.size());   
    
    return true;
}

bool ReplSender::clntSendSelfRTHash(ReplConn * pConn, AutoBuf *pAutoBuf)
{
    assert(pConn != NULL);
    pConn->sendPacket(0, RT_C2M_SELF_RTHASH, 0, pAutoBuf->begin(), pAutoBuf->size());   
    return true;
}

bool ReplSender::sockClientPingSvrStatus (ReplConn * pConn )
{
    return sendLocalReplStatus(pConn, RT_REPL_PING);
}

int ReplSender::sendACKPacket(ReplConn * pConn, ReplPacketHeader * pHeader, int ptype, const char *pBuf, int len)
{
    pConn->sendPacket(pHeader->getContID(), ptype, pHeader->getSequenceNum()
        , pBuf, len);   
    return 0;
}


/*pack format
 * container minTm, maxTm, config serialize len(int)
 * config chunk
 */
bool ReplSender::notifyClntNeedFRepl(ReplConn* pConn)
{
    uint32_t minTm, maxTm; 
    int clen = 0;
    AutoBuf autoBuf;
    ReplRegistry::getInstance().getContsLruMinMaxTm(minTm, maxTm);
    autoBuf.append((const char*)&minTm, sizeof(uint32_t));
    autoBuf.append((const char*)&maxTm, sizeof(uint32_t));    
    autoBuf.append((const char*)&clen  , sizeof(int));
    //config pack
    ReplRegistry::getInstance().getContNameMD5Lst(autoBuf);
    clen = autoBuf.size() - 3 * sizeof(int);
    *(int *)(autoBuf.begin() + 2 * sizeof(int)) = clen;
    return pConn->sendPacket(0, RT_M2C_FREPL_NEED, 0, autoBuf.begin(), autoBuf.size()); 
}
 
int ReplSender::sendLruHashData (int contID, uint32_t lruTm, unsigned char *pId, int idLen,
      unsigned char *pData, int iDataLen)
{
    LS_DBG_M("ReplSender::sendLruHashData contID:%d, lruTm:%d, idLen:%d, dataLen:%d", 
             contID, lruTm, idLen, iDataLen );
    ReplicableLruHash sd;
    sd.addReplicalbe(lruTm, pId, idLen, pData, iDataLen);
    return bcastReplicableData(&sd, contID, lruTm);
}

int ReplSender::sendLruHashBlukData (ReplConn *pConn, int contId, AutoBuf &rAutoBuf)
{
    pConn->sendPacket(contId, RT_BULK_UPDATE, getSeqNum()
        , rAutoBuf.begin(), rAutoBuf.size());

    getReplGroup()->getFullReplAuditor().addDataSize(rAutoBuf.size());
    return LS_OK;
}

/*
 *pack format: local container counter, serialize obj 
 */   
int ReplSender::bcastReplicableData (Replicable* pRepl, int contID, uint32_t lruTm)
{
    int count;
    AutoBuf autoBuf;
    uint32_t seqNum;
    ReplConn * pConn;    
    ReplPacketHeader header;

    int bufSize = pRepl->getsize();
    bufSize += sizeof(int);
    autoBuf.reserve(bufSize);
    
    NodeInfo *pLocalInfo = getReplGroup()->getNodeInfoMgr()->getLocalNodeInfo();
    count = pLocalInfo->geCntId2CntMap().getVal(contID);    
    autoBuf.append((const char*)&count, sizeof(int));
    
    pRepl->packObj(0, autoBuf.end(), bufSize - sizeof(int));

    header.initPack();
    header.setContID ( contID );
    header.setType ( RT_DATA_UPDATE );
    Addr2ClntConnMap_t &addr2ClntConnMap = getReplGroup()->getSockConnMgr()->getClntConnMap();
    
    HashStringMap< ReplConn *>::const_iterator itr ;
    for ( itr = addr2ClntConnMap.begin(); itr != addr2ClntConnMap.end() 
        ; itr = addr2ClntConnMap.next ( itr ) )
    {
        pConn   = itr.second();
        seqNum  = getSeqNum();
        if (pConn->isActive() )
        {
            ReplConn * pConn = itr.second();
            header.setSequenceNum ( seqNum );
            header.setPackLen ( sizeof (ReplPacketHeader) + bufSize  );    
            
            pConn->sendBuff ( ( const char* )(&header), sizeof(ReplPacketHeader), 0 );
            pConn->sendBuff ( ( const char* )autoBuf.begin(), bufSize, 0 );
            LS_DBG_L ( "bcastReplicableData sent to client addr[%s] %zd bytes"
                , itr.first(), sizeof(ReplPacketHeader) + bufSize);
        }
        pConn->cacheWaitAckData(seqNum, header.getContID(), lruTm);
    }
    return LS_OK;
}

  