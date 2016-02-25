
#include "lcnodeinfo.h"
#include "replshmhelper.h"
#include "lcreplconf.h"
#include <log4cxx/logger.h>
#include <socket/gsockaddr.h>
#include <string.h>

StaticNodeInfo::StaticNodeInfo(const char *pAddr)
{
    setHashKey(pAddr);
    setSvrAddr(getReplConf()->getLisenSvrAddr());
    setShmSvrAddr(getReplConf()->getMemCachedAddr());
}

StaticNodeInfo::StaticNodeInfo(const StaticNodeInfo& status)
{
    if ( this == &status )
        return;
    m_SvrIP         = status.m_SvrIP;
    m_SvrPort       = status.m_SvrPort;
    m_ShmSvrIP      = status.m_ShmSvrIP;
    m_ShmSvrPort    = status.m_ShmSvrPort;
}

void  StaticNodeInfo::setSvrAddr(const char *pSockAddr)
{
    GSockAddr gSock;
    gSock.set(pSockAddr, AF_INET);
    m_SvrIP     = gSock.getV4()->sin_addr.s_addr;
    m_SvrPort   = gSock.getPort();
}

void  StaticNodeInfo::setShmSvrAddr(const char *pSockAddr)
{
    GSockAddr gSock;
    gSock.set(pSockAddr, AF_INET);
    m_ShmSvrIP     = gSock.getV4()->sin_addr.s_addr;
    m_ShmSvrPort   = gSock.getPort();
}

const char *StaticNodeInfo::getSvrIPStr(uint32_t IP, char * pBuf, int len)
{
    struct in_addr ip_addr;
    ip_addr.s_addr = IP;
    snprintf(pBuf, len, "%s", inet_ntoa(ip_addr));
    return pBuf;    
}

const char *StaticNodeInfo::getSvrPortStr(uint16_t port, char * pBuf, int len)
{
    snprintf(pBuf, len, "%d", port);
    return pBuf;
}

const char * StaticNodeInfo::getSvrAddr(uint32_t IP, uint16_t port, char * pBuf, int len)
{
    struct in_addr ip_addr;
    ip_addr.s_addr = IP;
    snprintf(pBuf, len, "%s:%d", inet_ntoa(ip_addr), port);
    return pBuf;          
}

int StaticNodeInfo::totalSize() const
{
    return sizeof(m_SvrIP) + sizeof(m_SvrPort) 
        + sizeof(m_ShmSvrIP) + sizeof(m_ShmSvrPort);
}

int StaticNodeInfo::serialize (AutoBuf &rAutoBuf)
{
    rAutoBuf.append((const char*)&m_SvrIP,      sizeof(uint32_t));
    rAutoBuf.append((const char*)&m_SvrPort,    sizeof(uint16_t));
    rAutoBuf.append((const char*)&m_ShmSvrIP,   sizeof(uint32_t));
    rAutoBuf.append((const char*)&m_ShmSvrPort, sizeof(uint16_t));
    return LS_OK;
}

StaticNodeInfo * StaticNodeInfo::deserialize(char *pBuf, int dataLen)
{
    StaticNodeInfo *pStatus = new StaticNodeInfo("");
    char * ptr = pBuf;
    
    pStatus->m_SvrIP  = *(uint32_t*)ptr;
    ptr += sizeof(uint32_t);

    pStatus->m_SvrPort  = *(uint16_t*)ptr;
    ptr += sizeof(uint16_t);

    pStatus->m_ShmSvrIP  = *(uint32_t*)ptr;
    ptr += sizeof(uint32_t);

    pStatus->m_ShmSvrPort  = *(uint16_t*)ptr;

    if (pStatus->totalSize() != dataLen)
    {
        delete pStatus;
        return NULL;
    }
    return pStatus;
}

void StaticNodeInfo::DEBUG() const
{
    char pBuf1[32];
    char pBuf2[16];
    
    LS_INFO (  "Repl Server[%s:%s]"
        , getSvrIPStr(getSvrIp(), (char*)pBuf1, sizeof(pBuf1))
        , getSvrPortStr(getSvrPort(), pBuf2, sizeof(pBuf2)) );
    
    LS_INFO (  "Shm Server[%s:%s]"
        , getSvrIPStr(getShmSvrIp(), (char*)pBuf1, sizeof(pBuf1))
        , getSvrPortStr(getShmSvrPort(), pBuf2, sizeof(pBuf2)) );
}    

int StaticNodeInfo::setHashKey(const char *pAddr)
{
    AutoStr str; 
    m_hashKey = getIpOfAddr(pAddr, str);
    return LS_OK;
}

LcNodeInfo::LcNodeInfo(int iContCount, const char *pAddr)
                : NodeInfo(pAddr) 
                , m_iContCount(iContCount)
{
    m_pContId   = new uint32_t[m_iContCount]();
    m_pCurrTid  = new uint64_t[m_iContCount]();
    m_pPriority = new uint32_t[m_iContCount]();
    m_pTidNum   = new uint32_t[m_iContCount]();
    m_pRole     = new uint16_t[m_iContCount]();
    m_pFsync    = new uint16_t[m_iContCount](); 
    
    for (int i=0; i < (int)m_iContCount ; ++i)
    {
        m_pContId[i] = i+1;
        m_pRole[i]   = R_SLAVE;
    }
}

LcNodeInfo::~LcNodeInfo()
{
    delete []m_pContId;
    delete []m_pCurrTid;
    delete []m_pPriority;
    delete []m_pTidNum;
    delete []m_pRole;
    delete []m_pFsync;
}

int LcNodeInfo::copyAll(NodeInfo* rhs)
{
    LcNodeInfo* pStatus = static_cast<LcNodeInfo*>(rhs);
    memcpy(m_pContId,   pStatus->m_pContId,     sizeof(uint32_t) * m_iContCount);
    memcpy(m_pCurrTid,  pStatus->m_pCurrTid,    sizeof(uint64_t) * m_iContCount);
    memcpy(m_pPriority, pStatus->m_pPriority,   sizeof(uint32_t) * m_iContCount);
    memcpy(m_pTidNum,   pStatus->m_pTidNum,     sizeof(uint32_t) * m_iContCount);
    memcpy(m_pRole,     pStatus->m_pRole,       sizeof(uint16_t) * m_iContCount);
    memcpy(m_pFsync,    pStatus->m_pFsync,      sizeof(uint16_t) * m_iContCount);
    return LS_OK;
}

int LcNodeInfo::refresh()
{
    int subNum = getReplConf()->getSubFileNum();
    for (int idx = 0; idx < subNum; ++idx)
    {
        setCurrTid(idx, ReplShmHelper::getInstance().getLastShmTid(idx));
    }
    return LS_OK;
}

bool LcNodeInfo::isSynced(int idx, uint32_t peerNum, uint64_t peerMinTid, uint64_t peerCurrTid) const    
{
    return m_pCurrTid[idx] == peerCurrTid;
}

bool LcNodeInfo::isSynced(int idx, const LcNodeInfo* pStatus) const
{
    return m_pCurrTid[idx] == pStatus->getCurrTid(idx);
}

//only slave calls this to wipe out everything
bool LcNodeInfo::isBadSynced(int idx, const LcNodeInfo* pMstrStatus) const
{
    return m_pCurrTid[idx] == pMstrStatus->getCurrTid(idx);
}

int LcNodeInfo::totalSize() const
{
    return sizeof(m_iContCount)
        + (sizeof(*m_pContId) + sizeof(*m_pCurrTid) + sizeof(*m_pPriority) 
        +   sizeof(*m_pTidNum) + sizeof(*m_pRole) + sizeof(*m_pFsync)) 
        * m_iContCount;   
}

int LcNodeInfo::serialize (AutoBuf &rAutoBuf)
{
    rAutoBuf.append((const char*)&m_iContCount, sizeof(uint32_t));
    rAutoBuf.append((const char*)m_pContId,    sizeof(uint32_t) * m_iContCount);
    rAutoBuf.append((const char*)m_pCurrTid,   sizeof(uint64_t) * m_iContCount);
    rAutoBuf.append((const char*)m_pPriority,  sizeof(uint32_t) * m_iContCount);
    rAutoBuf.append((const char*)m_pTidNum,    sizeof(uint32_t) * m_iContCount);
    rAutoBuf.append((const char*)m_pRole,      sizeof(uint16_t) * m_iContCount);
    rAutoBuf.append((const char*)m_pFsync,     sizeof(uint16_t) * m_iContCount);
    return rAutoBuf.size();
}

LcNodeInfo * LcNodeInfo::deserialize(char *pBuf, int dataLen, const char* pClntAddr)
{
    char * ptr = pBuf;

    uint32_t contCount = *(uint32_t*)ptr;
    ptr += sizeof(uint32_t);
    
    LcNodeInfo *pStatus = new LcNodeInfo(contCount, pClntAddr);
    
    memcpy(pStatus->m_pContId, ptr, sizeof(uint32_t) * contCount);  
    ptr += sizeof(uint32_t) * contCount;
    
    memcpy(pStatus->m_pCurrTid, ptr, sizeof(uint64_t) * contCount);  
    ptr += sizeof(uint64_t) * contCount;
    
    memcpy(pStatus->m_pPriority, ptr, sizeof(uint32_t) * contCount);
    ptr += sizeof(uint32_t) * contCount;

    memcpy(pStatus->m_pTidNum, ptr, sizeof(uint32_t) * contCount);
    ptr += sizeof(uint32_t) * contCount;

    memcpy(pStatus->m_pRole, ptr, sizeof(uint16_t) * contCount);
    ptr += sizeof(uint16_t) * contCount;

    memcpy(pStatus->m_pFsync, ptr, sizeof(uint16_t) * contCount);
    ptr += sizeof(uint16_t) * contCount;
    
    if (pStatus->totalSize() != dataLen)
    {
        delete pStatus;
        return NULL;
    }
    return pStatus;
}

void LcNodeInfo::DEBUG() const
{
    char pBuf[32];
    for (int i = 0; i < (int)m_iContCount; ++i)
    {
        LS_INFO (  "\tcontID[%d]=%d, currTid[%d]=%lld, priority[%d]=%d,role[%d]=%s, didFrepl[%d]:%s"
            , i, getContID(i), i, getCurrTid(i), i, getPriority(i)
            , i, printStrRole(getRole(i), pBuf, sizeof(pBuf)) 
            , i, isDidFsync(i)?"Yes":"No");
    }
}

const char * LcNodeInfo::printStrRole(uint16_t role, char *pBuf, int len)
{
    switch(role)
    {
    case R_MASTER:
        snprintf(pBuf, len, "%s", "MASTER");
        break;
    case R_SLAVE:
        snprintf(pBuf, len, "%s", "SLAVE");
        break;
    case R_FLIPING_MASTER:  //ready to hander over, still be master
        snprintf(pBuf, len, "%s", "FLIPING_MASTER");
        break;
    case R_FlIPING_SLAVE:   //unack master, still be slave
        snprintf(pBuf, len, "%s", "FlIPING_SLAVE");
        break;        
    default:
        snprintf(pBuf, len, "%s", "UNKNOWN");
        break;        
    }
    return pBuf;
}

void  LcNodeInfo::setRole(int idx, uint16_t role)
{
    char pBuf[32], pBuf2[32];
    if (m_pRole[idx] != role)
    {
        LS_NOTICE ("role idx=%d, changing=[%s -> %s]", idx
            , printStrRole(m_pRole[idx], pBuf, sizeof(pBuf))
            , printStrRole(role, pBuf2, sizeof(pBuf2)));
        m_pRole[idx] = role;
    }    
}
    
void  LcNodeInfo::setAllRole(uint16_t role)
{
    for (int i = 0; i < (int)m_iContCount ; ++i)
    {
        setRole(i, role);
    }            
}

int LcNodeInfoMgr::addNodeInfo(NodeInfo *pNodeInfo)
{
    HashStringMap< LcNodeInfo *>::iterator iter = m_hNodeInfoMap.find(pNodeInfo->getIp());
    if (iter == m_hNodeInfoMap.end())
    {   
        m_hNodeInfoMap.insert( pNodeInfo->getIp(), pNodeInfo);
    }
    else
    {
         LcNodeInfo *pCurr = iter.second();
         pCurr->copyAll(static_cast<LcNodeInfo*>(pNodeInfo)); 
    }    
    
    return LS_OK;
}

NodeInfo * LcNodeInfoMgr::getNodeInfo(const char *pAddr)
{
    AutoStr str;
    const char *Ip = getIpOfAddr(pAddr, str);
    HashStringMap< LcNodeInfo *>::iterator iter = m_hNodeInfoMap.find(Ip);
    if (iter != m_hNodeInfoMap.end())
        return iter.second();  
    return NULL;
}

StaticNodeInfo * StNodeInfoMgr::getNodeInfo(const char *pAddr)
{
    AutoStr str;
    const char *Ip = getIpOfAddr(pAddr, str);
    LS_DBG_M("StNodeInfoMgr::getNodeInfo, ip:%s", Ip);
    HashStringMap< StaticNodeInfo *>::iterator iter = m_hNodeInfoMap.find(Ip);
    if (iter != m_hNodeInfoMap.end())
        return iter.second();  
    return NULL;
}

int StNodeInfoMgr::addNodeInfo(StaticNodeInfo* pNodeInfo)
{
    LS_DBG_M(  "StNodeInfoMgr::addNodeInfo Addr=%s", pNodeInfo->getHashKey());
    HashStringMap< StaticNodeInfo *>::iterator iter = m_hNodeInfoMap.find(pNodeInfo->getHashKey());
    if (iter != m_hNodeInfoMap.end())
    {
        LS_DBG_M(  "StNodeInfoMgr::addNodeInfo found Addr=%s",iter.first());
        return LS_FAIL;
    }

    m_hNodeInfoMap.insert( pNodeInfo->getHashKey(), pNodeInfo);
    return LS_OK;
}

void StNodeInfoMgr::delNodeInfo(const char* pAddr)
{
    AutoStr str;
    const char *Ip = getIpOfAddr(pAddr, str);    
    HashStringMap< StaticNodeInfo *>::iterator itr = m_hNodeInfoMap.find(Ip);
    if (itr != m_hNodeInfoMap.end())
    {
        delete  itr.second();    
        m_hNodeInfoMap.remove(Ip);
    }
}
