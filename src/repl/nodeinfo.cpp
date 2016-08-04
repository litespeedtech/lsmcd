
#include "nodeinfo.h"
#include "jsonstatus.h"

#include "replconf.h"
#include "replregistry.h"
#include "sockconn.h"
#include <util/autobuf.h>
#include <log4cxx/logger.h>
#include <string.h>
#include <stdio.h>

 
NodeInfo::NodeInfo(const char *pAddr)
    : m_syncTime(0)
    , m_upTime(0)
    , m_pStatusFormater(NULL)
{
    setIp(pAddr);
}

NodeInfo::~NodeInfo()
{
    if (m_pStatusFormater != NULL)
        delete m_pStatusFormater;
}

int NodeInfo::copyAll(NodeInfo* rhs)
{
    if (rhs == this)
        return LS_FAIL;
    m_syncTime = rhs->m_syncTime;
    m_upTime   = rhs->m_upTime;
    rhs->m_CntId2CntMap.assignTo(&m_CntId2CntMap);
    return LS_OK;
}

int NodeInfo::getContIdVal(int contId) const  
{
    return  m_CntId2CntMap.getVal(contId);
}

int refreshFn(void* cbData, void* pUData1, void* pUData2)
{
    ReplContainer *pContainer   = (ReplContainer *)cbData;
    Int2IntMap *pInt2IntMap     = (Int2IntMap *)pUData1;
    pInt2IntMap->addSetVal(pContainer->getId(), pContainer->getLruHashCnt());
    return LS_OK;
}

int NodeInfo::refresh()
{
    int count = 0;
    ReplRegistry& RegObj = ReplRegistry::getInstance();    
    RegObj.for_each(refreshFn, &m_CntId2CntMap, NULL);
    return count;
}

bool NodeInfo::isSyncWith(const NodeInfo &peerNode) const
{
    const Int2IntMap &peerMap = peerNode.geCntId2CntMap();
    return m_CntId2CntMap.isEqual(&peerMap);
}

bool NodeInfo::cmpLeaderNode(const NodeInfo *pLeaderNode
    , bool &isLeader, bool &isSync) const
{
    if (this == pLeaderNode)
    {
        isLeader = true;
        isSync   = true;
        return true;
    }
    else
    {
        isLeader = false;
        isSync   = isSyncWith(*pLeaderNode);
        return false;
    }    
}

void NodeInfo::setIp(const char *pAddr)
{
    AutoStr str; 
    m_Ip = getIpOfAddr(pAddr, str);    
}

//pack order: syncTime, upTime, containerId1, value1...
int NodeInfo::serialize (AutoBuf &rAutoBuf)
{
    rAutoBuf.append((const char*)&m_syncTime, sizeof(uint32_t));
    rAutoBuf.append((const char*)&m_upTime, sizeof(uint32_t));
    m_CntId2CntMap.serialze(rAutoBuf);    
    return rAutoBuf.size();
}

//pAddr can be ip or add
NodeInfo * NodeInfo::deserialize(const char *pBuf, int bufSz, const char* pAddr)
{
    
    NodeInfo *pNodeInfo = new NodeInfo(pAddr);

    char *ptemBuf = (char *)pBuf;
    int temBufLen = bufSz;

    int iCopiedlen = ReplPacker::unpack ( ptemBuf, pNodeInfo->m_syncTime );
    ptemBuf   += iCopiedlen;
    temBufLen -= iCopiedlen;

    iCopiedlen = ReplPacker::unpack ( ptemBuf, pNodeInfo->m_upTime );
    ptemBuf   += iCopiedlen;
    temBufLen -= iCopiedlen;

    pNodeInfo->m_CntId2CntMap.deserialze(ptemBuf, temBufLen);

    return pNodeInfo;
}

void NodeInfo::DEBUG() const
{
    LS_INFO("nodeinfo debug: syncTime:%u, upTime:%u"
        , m_syncTime, m_upTime);        
    m_CntId2CntMap.DEBUG();
}


void NodeInfo::mkJson(const char * ip,  bool isLocal, bool isLeader
    , bool isSyncing, bool isSync, AutoBuf& rAutoBuf) const
{
    LS_DBG_M("NodeInfo::mkJson m_pStatusFormater:%p", m_pStatusFormater);
    if (m_pStatusFormater != NULL)
    {
        m_pStatusFormater->setNode(
            ip, 
            isLocal,
            isLeader,
            isSyncing,
            isSync,
            this
        );
        m_pStatusFormater->mkJson(rAutoBuf);
    }
}

NodeInfo *NodeInfoMgr::getLocalNodeInfo(bool bRefresh)
{
    NodeInfo * pLocal = getNodeInfo(ConfWrapper::getInstance()->getLocalLsntnrIp());
    if (!pLocal)
        return NULL;
    if ( bRefresh )
        pLocal->refresh();
    return pLocal;
}

const NodeInfo * NodeInfoMgr::getLeaderNode() const
{
    NodeInfo *pNodeInfo = NULL;
    Ip2NodeInfoMap_t::iterator itr;
    for (itr = m_hNodeInfoMap.begin(); itr != m_hNodeInfoMap.end(); 
         itr = m_hNodeInfoMap.next(itr) )
    {
        NodeInfo *pTmpNode = itr.second();
        if (pNodeInfo == NULL)
        {
            pNodeInfo = pTmpNode;            
        }
        else
        {
            time_t tm = pTmpNode->getUpTime();
            if (tm > 0 && tm < pNodeInfo->getUpTime())
            {
                pNodeInfo = pTmpNode;
            }
        }
    }
    assert (pNodeInfo != NULL);
    return pNodeInfo;
}

NodeInfoMgr::~NodeInfoMgr()
{
   m_hNodeInfoMap.release_objects(); 
}

int NodeInfoMgr::addNodeInfo(NodeInfo *pNodeInfo)
{
    m_hNodeInfoMap.insert(pNodeInfo->getIp(), pNodeInfo);
    return LS_OK;
}
//add and update 
int NodeInfoMgr::replaceNodeInfo(NodeInfo *pNewInfo)
{
    NodeInfo *pNodeInfo = getNodeInfo(pNewInfo->getIp());
    if(pNodeInfo == NULL)
    {
        addNodeInfo(pNewInfo);
        return 0;
    }
    else
    {
        pNodeInfo->copyAll(pNewInfo);
        return 1;
    }
}

NodeInfo * NodeInfoMgr::getNodeInfo(const char * pAddr)
{
    AutoStr str;
    const char *Ip = getIpOfAddr(pAddr, str);
    Ip2NodeInfoMap_t::iterator itr = m_hNodeInfoMap.find(Ip);
    if(itr == m_hNodeInfoMap.end())
        return NULL;
    return itr.second();
}

int NodeInfoMgr::updateNodeInfo (NodeInfo & nodeInfoVal)
{
    NodeInfo *pNodeInfo = getNodeInfo(nodeInfoVal.getIp());
    assert(pNodeInfo != NULL);
    pNodeInfo->copyAll(&nodeInfoVal);
    return LS_OK;
}

int NodeInfoMgr::updateNodeInfo (const char *pAddrVal, int contId, int count)
{
    NodeInfo *pNodeInfo = getNodeInfo(pAddrVal);
    if(pNodeInfo == NULL)
    {
        LS_ERROR("NodeInfoMgr::updateNodeInfo pAddrVal:%s can not be found, socket connection is lost?"
            , pAddrVal);
        return LS_FAIL;
    }
    pNodeInfo->geCntId2CntMapPtr()->addSetVal(contId, count);
    return LS_OK;    
}

int NodeInfoMgr::updateNodeInfo(const char* pClntAddr, const char* pData, int len)
{
    AutoStr str;
    const char *Ip = getIpOfAddr(pClntAddr, str);
    NodeInfo * pPeerInfo        = NodeInfo::deserialize(pData, len, Ip);
    updateNodeInfo(*pPeerInfo);

    delete pPeerInfo;
    return LS_OK;
}

void NodeInfoMgr::delNodeInfo(const char *pAddr)
{
    AutoStr str;
    const char *Ip = getIpOfAddr(pAddr, str);    
    HashStringMap< NodeInfo *>::iterator itr = m_hNodeInfoMap.find(Ip);
    if (itr != m_hNodeInfoMap.end())
    {
        NodeInfo *pNode = itr.second();    
        m_hNodeInfoMap.erase(itr);
        delete pNode;        
    }
}

//note that calling time affects the result
bool NodeInfoMgr::isSelfFirstUp()
{
    return getLocalNodeInfo()->getUpTime() == getLeaderNode()->getUpTime();
}

void NodeInfoMgr::getGroupJsonStatus(SockConnMgr *pSockConnMgr, JsonStatus *pStatusFormater, AutoBuf &rAutoBuf)
{
    bool isLeader, isSync;
    const NodeInfo *pPeerNode;
    const char *Ip;

    const NodeInfo *pLeaderNode = getLeaderNode();
    const char * localSvrIp     = ConfWrapper::getInstance()->getLocalLsntnrIp();
    //1)local Json
    rAutoBuf.append("{\n");
    NodeInfo *pLocalInfo = getLocalNodeInfo();
    pLocalInfo->cmpLeaderNode(pLeaderNode, isLeader, isSync); 
    
    pStatusFormater->setNode(localSvrIp, true, isLeader, false, isSync, pLocalInfo);
    pStatusFormater->mkJson(rAutoBuf);
    
    //2)acccepted node Json
    Ip2NodeInfoMap_t::iterator itr;
    Ip2NodeInfoMap_t &pNodeInfoMap = getNodeInfoMap();
    for (itr = pNodeInfoMap.begin(); itr != pNodeInfoMap.end(); 
         itr = pNodeInfoMap.next(itr) )
    {
        if ( itr.second() == pLocalInfo )
            continue;
        Ip = itr.first();
        if( !pSockConnMgr->isAceptedIpActv(Ip))
            continue;

        pPeerNode = getNodeInfo(Ip);
        if (pPeerNode == NULL)
            continue;
        pPeerNode->cmpLeaderNode(pLeaderNode, isLeader, isSync);
                rAutoBuf.append(",\n");
        pStatusFormater->setNode(Ip, false, isLeader, false, isSync, pPeerNode);
        pStatusFormater->mkJson(rAutoBuf);
    }
    rAutoBuf.append("}\n");
}
