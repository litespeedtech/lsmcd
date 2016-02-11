
#include "nodeinfo.h"
#include "replconf.h"
#include "replregistry.h"
#include <log4cxx/logger.h>
#include <string.h>
#include <stdio.h>

#define STATUS_FILE     "/tmp/lslbd/.replstatus"
#define STATUS_FILE_TMP "/tmp/lslbd/.replstatus.tmp"

JsonStatus::JsonStatus()
    : m_ip(NULL)
    , m_isLocal(false)
    , m_isLeader(false)
    , m_isSyncing(false)
    , m_isSync(false)
{}


JsonStatus::~JsonStatus()
{}


void JsonStatus::init(const char *ip, bool isLocal, bool isLeader, bool isSyncing, bool isSync
    , const NodeInfo *pNodeInfo)
{
    m_ip        = ip;
    m_isLocal   = isLeader;
    m_isLeader  =isLeader;
    m_isSyncing = isSyncing;
    m_isSync    = isSync;
    m_pNodeInfo = pNodeInfo;
}
/*
 * "Active IP1 of LB list": {
 * ...
 * }
*/
void JsonStatus::mkJson(AutoBuf &rAutoBuf) const
{
    char pBuf[128];
    snprintf(pBuf, 128, "\"%s\": {\n", m_ip);
    rAutoBuf.append(pBuf); 
    mkJsonFirstUp(rAutoBuf);
    mkJsonStatus(rAutoBuf);
    //if (m_isLocal && !m_pNodeInfo->m_fReplStat.m_isLeader)
    //    mkJsonFRepl(rAutoBuf);    
    mkJsonContainers(rAutoBuf);
    rAutoBuf.append("\n}\n");
}

//"firstUp": "yes/no", 
void JsonStatus::mkJsonFirstUp(AutoBuf &rAutoBuf) const
{
    char pBuf[128];
    snprintf(pBuf, 128, "\"firstUp\":\"%s\",\n", m_isLeader ? "Yes" : "No");
    rAutoBuf.append(pBuf);        
}

//"status": "in/out Sync",
void JsonStatus::mkJsonStatus(AutoBuf &rAutoBuf) const
{
    char pBuf[128];
    if (m_isSyncing)
        snprintf(pBuf, 128, "\"status\":\"%s\",\n", "Syncing up");
    else
        snprintf(pBuf, 128, "\"status\":\"%s\",\n", m_isSync ? "in Sync" : "out Sync");
    rAutoBuf.append(pBuf);
}

/*
"fullRepl":{
    "destAddr": "192.168.0.149",
    "startTime": 1444056061,
    "endTime": 1444056090,
    "dataSize": 98k,
}

}
 */
void JsonStatus::mkJsonFRepl(AutoBuf &rAutoBuf) const
{
//    char pBuf[128], pAddr[64];
    rAutoBuf.append("\"fullRepl\":{\n");
    
//     strcpy(pAddr, m_pNodeInfo->m_fReplStat.m_srcAddr.c_str());     
//     char *ptr = strchr(pAddr, ':');
//     *ptr = '\0'; 
//     snprintf(pBuf, 128, "\"destAddr\": \"%s\",\n", pAddr);
//     rAutoBuf.append(pBuf);
//     
//     snprintf(pBuf, 128, "\"startTime\": %d,\n", m_pNodeInfo->m_fReplStat.m_startTm);
//     rAutoBuf.append(pBuf);    
//     
//     snprintf(pBuf, 128, "\"endTime\": %d,\n", m_pNodeInfo->m_fReplStat.m_endTm);
//     rAutoBuf.append(pBuf);  
//     
//     snprintf(pBuf, 128, "\"dataSize\": %d\n", m_pNodeInfo->m_fReplStat.m_dataSize);
//     rAutoBuf.append(pBuf);  
//     rAutoBuf.append("},\n");  
  
}

/*
"session": {
   xxxx
}
 */
void JsonStatus::mkJsonContainers(AutoBuf &rAutoBuf) const
{
}
   
int JsonStatus::writeStatusFile(AutoBuf &autoBuf) const
{
    FILE * fp;
    int size = 0;
    fp = fopen(STATUS_FILE_TMP, "w");
    if (fp != NULL)
    {
        chmod(STATUS_FILE_TMP, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH );
        size = fwrite(autoBuf.begin(),1 , autoBuf.size(), fp);
        fclose(fp);
        if (size == autoBuf.size()) 
        {
            rename( STATUS_FILE_TMP, STATUS_FILE);
            return LS_OK;
        }

    }
    LS_ERROR("Replication fails to refresh repl status file, error:%s", strerror(errno));
    return LS_FAIL;
}
  
void JsonStatus::removeStatusFile() const
{
    remove(STATUS_FILE);
}
 
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
    if (getUpTime() == pLeaderNode->getUpTime())
    {
        isLeader = true;
        isSync   = true;
        return true;
    }
    else
    {
        isLeader = false;
        isSync   = pLeaderNode->isSyncWith(*this);
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
    LS_INFO("nodeinfo debug: syncTime:%d, upTime:%d"
        , m_syncTime, m_upTime);        
    m_CntId2CntMap.DEBUG();
}


void NodeInfo::mkJson(const char * ip,  bool isLocal, bool isLeader
    , bool isSyncing, bool isSync, AutoBuf& rAutoBuf) const
{
    LS_DBG_M("NodeInfo::mkJson m_pStatusFormater:%p", m_pStatusFormater);
    if (m_pStatusFormater != NULL)
    {
        m_pStatusFormater->init(
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

NodeInfo *NodeInfoMgr::getLocalNodeInfo()
{
    NodeInfo * pLocal = getNodeInfo(ConfWrapper::getInstance()->getLocalLsntnrIp());
    if (!pLocal)
        return NULL;
    pLocal->refresh();
    return pLocal;
}

const NodeInfo * NodeInfoMgr::getLeaderNode() const
{
    time_t minTm, tm;
    minTm = 2 * time(NULL);
    NodeInfo *pNodeInfo = NULL;
    Ip2NodeInfoMap_t::iterator itr;
    for (itr = m_hNodeInfoMap.begin(); itr != m_hNodeInfoMap.end(); 
         itr = m_hNodeInfoMap.next(itr) )
    {
        NodeInfo *pTmpNode = itr.second();
        tm = pTmpNode->getUpTime();
        if (tm > 0 && tm < minTm)
        {
            minTm = tm;
            pNodeInfo = pTmpNode;
        }
    }
    assert (pNodeInfo != NULL);
    return pNodeInfo;
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
    assert(pNodeInfo != NULL);
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
        itr = m_hNodeInfoMap.remove(Ip);
        delete  itr.second();    
    }
}

//note that calling time affects the result
bool NodeInfoMgr::isSelfFirstUp()
{
    return getLocalNodeInfo()->getUpTime() == getLeaderNode()->getUpTime();
}


