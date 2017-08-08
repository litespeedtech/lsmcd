
#include "lcreplgroup.h"
#include "replshmhelper.h"
#include "lcreplsender.h"
#include "lcreplconf.h"
#include "lcreplsender.h"
#include "lcreplreceiver.h"
#include "replshmtidcontainer.h"
#include "replicableshmtid.h"
#include "usocklistener.h"
#include "usockmcd.h"
#include <repl/sockconn.h>
#include <repl/replregistry.h>
#include <log4cxx/logger.h>
#include <string.h>

LcReplGroup::LcReplGroup()
    : m_pStNodeInfoMgr(new StNodeInfoMgr())
{
    m_pNodeInfoMgr = new LcNodeInfoMgr();
    setReplReceiver(new LcReplReceiver());
    setReplSender(new LcReplSender());
}

//sockAdd:ip:port
LcReplGroup::~LcReplGroup ()
{
}

LcReplGroup &LcReplGroup::getInstance()
{
    return *(static_cast<LcReplGroup*> (getReplGroup()));
}

//sockAdd:ip:port
void LcReplGroup::initSelfStatus ()
{
    LcNodeInfo *pNodeInfo = new LcNodeInfo( getReplConf()->getSliceCount(), getReplConf()->getLocalLsntnrIp());
    getNodeInfoMgr()->addNodeInfo(pNodeInfo);
    int count = 0;
    int *pPriorities = getReplConf()->getPriorities(&count);
    memcpy(pNodeInfo->getPriority(), pPriorities, sizeof(int) * count);
    DEBUG();
}

//repld notify all cacheds, but can't tell which slice change, 
int LcReplGroup::notifyRoleChange()
{
    m_pUsockLstnr->repldNotifyRole();
    return LS_OK;
}

int LcReplGroup::notifyConnChange()
{
    m_pUsockLstnr->repldNotifyConn(getSockConnMgr()->getActvLstnrConnCnt());
    return LS_OK;
}

void LcReplGroup::delClntInfoAll(const char *pClntAddr)
{
    LS_INFO (  "LcReplGroup::delClntInfoAll: deleting socket client[%s]", pClntAddr);

    getSockConnMgr()->delAcceptedNode(pClntAddr);
    getNodeInfoMgr()->delNodeInfo(pClntAddr);
    getStNodeInfoMgr()->delNodeInfo(pClntAddr);
    LS_DBG_M(  "delClntInfoAll client[%s],  ReplStatus size=%d,  StaticStatus size=%d, m_hClientReplPeer size=%d"
        , pClntAddr, getNodeInfoMgr()->size(), getStNodeInfoMgr()->size(), getSockConnMgr()->getAcptedConnCnt());
}

LcNodeInfo * LcReplGroup::getLocalNodeInfo(bool bRefresh)
{
    return static_cast<LcNodeInfo *>(getNodeInfoMgr()->getLocalNodeInfo(bRefresh));
}

void LcReplGroup::DEBUG ()
{
    uint16_t idx = 0;
    LS_DBG_M (  "============================All lsmcd STATUS ============================");
    const char* pSockAddr;
    Ip2NodeInfoMap_t::iterator itr;
    const StaticNodeInfo * pStaticStatus;
    Ip2NodeInfoMap_t &infoMap = getNodeInfoMgr()->getNodeInfoMap();
    for (itr = infoMap.begin(); itr != infoMap.end(); itr = infoMap.next(itr) )
    {
        pSockAddr = itr.first();
        LS_DBG_M (  "node[%d], Client[%s]", idx, pSockAddr);

        pStaticStatus = getStNodeInfoMgr()->getNodeInfo(pSockAddr);
        if (pStaticStatus != NULL)
            pStaticStatus->DEBUG();
        LcNodeInfo *pStatus = static_cast<LcNodeInfo*>(itr.second());
        pStatus->DEBUG();
        ++idx;
    }
    
    LS_DBG_M (  "============================ END (total=%d) ============================", idx);
}

int LcReplGroup::clientsElectMstr(const LcNodeInfo * pLocalStatus, int idx)
{
    if (isLegalClientBeMstr(idx, pLocalStatus->getCurrTid(idx),
        pLocalStatus->getPriority(idx))) //ouch! the client can take over mstr
    {
        LS_NOTICE(  "I'm trying to be new master");
        ((LcNodeInfo *)pLocalStatus)->setRole(idx, R_MASTER);  //notify peter of new master
        LcReplSender::getInstance().bcastNewMstrElectDone (pLocalStatus->getContID(idx));
        return LS_OK;
    }
    LS_NOTICE(  "I'm not legal client to become new master");
    return LS_FAIL;
}

int LcReplGroup::onTimer1s()
{
    static time_t lastTm = time(NULL);
    if (time(NULL) - lastTm < 1)
        return LS_OK;
    lastTm = time(NULL);

    if (!isLstnrProc())
        return LS_OK;

    static int s_timeOut = 4;
    s_timeOut --;
    if ( !s_timeOut )
    {
        s_timeOut = 4;
        connGroupLstnrs();
        monitorRoles();
    }

    return LS_OK;   
}

int  LcReplGroup::onTimer30s()
{
    if (!isLstnrProc())
        return LS_OK;

    DEBUG();
    return LS_OK;
}

void LcReplGroup:: monitorRoles()
{
    LS_DBG_M("monitorRoles");
    int inNum  = getSockConnMgr()->getAcptedConnCnt();
    int outNum = getSockConnMgr()->getActvLstnrConnCnt();
    if ( !inNum && !outNum)
    {
        LS_DBG_M("none client is connected, verifying as being master");
        flipSelfBeMstr();
    }
    else if ( (inNum == outNum) && (inNum == getNodeInfoMgr()->size() -1)
        && inNum > 0 ) // all of in, out connection and status are ready
    {
        /*3) if self[idx] is master and all member in sync.
               if find the largest member priority > self,
                   send new election master to it,
                   after get ACK, self change role
          */
        int changes=0;
        const LcNodeInfo *pLocalStatus = getLocalNodeInfo();
        int num = pLocalStatus->getContCount();
        for (int idx = 0 ; idx < num ; ++idx)
        {
            if ( pLocalStatus->getRole(idx) == R_MASTER )
            {
                if ( isAllClientSync(pLocalStatus, idx))
                {
                    const char *pSockAddr = hasHighPriorityClient (pLocalStatus->getPriority(idx), idx);
                    if (pSockAddr != NULL) //wow... flip over role
                    {
                        //ServerConn * pSConn = getSockConnMgr()->getServerConn(pSockAddr);//   getClientNode(pSockAddr)->getReplConn();
                        ClientConn * pConn = getSockConnMgr()->getActvLstnrByAddr(pSockAddr);
                        LcReplSender::getInstance().mstr2ClientFlipReq(pConn, pLocalStatus->getContID(idx));
                        ((LcNodeInfo *)pLocalStatus)->setRole(idx, R_FLIPING_MASTER);
                        changes++;
                    }
                }
                else
                    LS_WARN("master warns data at slice idx %d is out of sync ", idx);
            }
            else //it is client
            {
                if ( !roleExistInGroup(idx, R_MASTER)) //no master in group
                {
                    clientsElectMstr(pLocalStatus, idx);
                    changes++;
                }
            }
        }
    }
}

int LcReplGroup::flipSelfBeMstr()
{
    bool bUpdate = false;
    LcNodeInfo *pLocalStatus = (LcNodeInfo *)LcReplGroup::getInstance().getLocalNodeInfo();
    int contCount =  pLocalStatus->getContCount();
    for (int i = 0; i < contCount; ++i)
    {
        if (pLocalStatus->getRole(i) != R_MASTER)
        {
            pLocalStatus->setRole(i, R_MASTER);
            LcReplGroup::getInstance().setNewMstrClearOthers(ConfWrapper::getInstance()->getLocalLsntnrIp(), pLocalStatus->getContID(i));
            //notify cached of new master
            LS_WARN(  "LcReplGroup::flipSelfBeMstr notify I'm new master, idx=%d", i);
            
            UsockSvr::getRoleData()->setRoleAddr(i, NULL, 0);
            bUpdate = true;
        }
    }
    if (bUpdate)
        LcReplGroup::getInstance().notifyRoleChange();
    return LS_OK;
}

//each IP is the initial leader in that group (containerID), which is connected from other IPs.
//each group is represented as channnel to replicate one of shm file
int LcReplGroup::initReplConn()
{
    StringList &sl = ConfWrapper::getInstance()->getLBAddrs();
    StringList::iterator itr;
    for ( itr = sl.begin(); itr != sl.end(); itr++ )
    {
        if (strcmp( ( *itr )->c_str(),
            ConfWrapper::getInstance()->getLisenSvrAddr() ))
        {
            m_pSockConnMgr->addClntNode(( *itr )->c_str());
        }
    }
    
    initSelfStatus();
    LcNodeInfo *pLocalInfo = getLocalNodeInfo();
    int subNum = getReplConf()->getSliceCount();
    for (int idx = 0; idx < subNum; ++idx)
    {
        ReplShmTidContainer * pContainer1 = new ReplShmTidContainer (
            idx + 1, NULL );
        ReplRegistry::getInstance().add ( idx + 1, pContainer1 );
        ReplShmHelper::getInstance().readShmDBTid(idx, pLocalInfo);
    }
    
    return LS_OK;
}

void LcReplGroup::setRef(UsocklListener * pUsockLstnr)
{
    m_pUsockLstnr = pUsockLstnr;
}
int LcReplGroup::connGroupLstnrs()
{
    Addr2ClntConnMap_t::iterator itr;
    Addr2ClntConnMap_t &lstrMap = m_pSockConnMgr->getClntConnMap();
    const char* sockAddr;
    ClientConn * pConn;
    for (itr = lstrMap.begin(); itr != lstrMap.end(); itr = lstrMap.next ( itr ))
    {
        sockAddr = itr.first();
        pConn = itr.second();
        LS_DBG_M("LcReplGroup::connAllListenSvr addr:%s", sockAddr);
        if (!pConn->isActive() )
        {
            LS_DBG_M("LcReplGroup::connAllListenSvr is not active,  pMultiplexer:%p", getMultiplexer());
            pConn->closeConnection();
            pConn->SetMultiplexer(getMultiplexer() );

            GSockAddr serverAddr;
            serverAddr.set(sockAddr, AF_INET );
            if (pConn->connectTo (&serverAddr) != -1)
            {
                LS_INFO ("node[%s] addr=%p, is connecting to master[%s]",
                         pConn->getLocalAddr(), pConn, sockAddr);

                LcReplSender::getInstance().clientJoinListenSvrReq(pConn);
            }
            else
                LS_INFO ("failed to connect to master %d", pConn->isActive());
        }
    }
  return LS_OK;
}

int LcReplGroup::clearClntOnClose(const char *pAddr)
{
    LS_INFO (  "LcReplGroup::clearClntOnClose: deleting socket client[%s]", pAddr);
    getSockConnMgr()->delAcceptedNode(pAddr);
    getNodeInfoMgr()->delNodeInfo(pAddr);
    getStNodeInfoMgr()->delNodeInfo(pAddr);
    LS_DBG_M(  "clearClntOnClose client[%s],  ReplStatus size=%d,  StaticStatus size=%d, m_hClientReplPeer size=%d"
        , pAddr, getNodeInfoMgr()->size(), getStNodeInfoMgr()->size(), getSockConnMgr()->getAcptedConnCnt());
    
    LcReplGroup::getInstance().notifyConnChange();
    return LS_OK;
}

bool LcReplGroup::isAllClientSync(const LcNodeInfo *  pLocalStatus, int idx)
{
    uint64_t myTid = pLocalStatus->getCurrTid(idx);
    LcNodeInfo *pPeerlStatus;
    Ip2NodeInfoMap_t::iterator itr;
    Ip2NodeInfoMap_t &infoMap = getNodeInfoMgr()->getNodeInfoMap();

    for (itr = infoMap.begin(); itr != infoMap.end(); itr = infoMap.next(itr) )
    {
        pPeerlStatus = static_cast<LcNodeInfo*>(itr.second());
        if (pPeerlStatus->getCurrTid(idx) !=  myTid)
        {
            LS_DBG_M("LcReplGroup::isAllClientSync return false idx:%d, peer tid:%lld, mytid:%lld"
                , idx, (long long)pPeerlStatus->getCurrTid(idx), (long long)myTid );
            return false;
        }
    }
    return true;
}

bool LcReplGroup::roleExistInGroup(int idx, uint16_t role)
{
    char pBuf[32];
    LcNodeInfo *pPeerlStatus;
    Ip2NodeInfoMap_t::iterator itr;
    Ip2NodeInfoMap_t &infoMap = getNodeInfoMgr()->getNodeInfoMap();
    for (itr = infoMap.begin(); itr != infoMap.end(); itr = infoMap.next(itr) )
    {
        pPeerlStatus = static_cast<LcNodeInfo*>(itr.second());
        if (pPeerlStatus->getRole(idx) ==  role)
        {
            return true;
        }
    }
    LS_WARN(  "no %s is found on the idx[%d]"
        , LcNodeInfo::printStrRole(role, pBuf, sizeof(pBuf)), idx);
    return false;
}

const char *LcReplGroup::hasHighPriorityClient(uint32_t myPriority, int idx)
{
    LcNodeInfo *pPeerlStatus = NULL;
    LcNodeInfo *pHighest = NULL;
    const char *sockAddr     = NULL;

    Ip2NodeInfoMap_t::iterator itr;
    Ip2NodeInfoMap_t &infoMap = getNodeInfoMgr()->getNodeInfoMap();
    for (itr = infoMap.begin(); itr != infoMap.end(); itr = infoMap.next(itr) )
    {
        pPeerlStatus = static_cast<LcNodeInfo *>(itr.second());
        if (pHighest == NULL)
        {
            pHighest = pPeerlStatus;
            sockAddr = itr.first();
        }
        else
        {
            if (pPeerlStatus->getPriority(idx) > pHighest->getPriority(idx))
            {
                pHighest = pPeerlStatus;
                sockAddr = itr.first();
            }
        }
    }
    if (pHighest != NULL &&
        pHighest->getPriority(idx) > myPriority )
    {
        return sockAddr;
    }
    return NULL;
}

int LcReplGroup::setNewMstrClearOthers(const char *pSockAddr, uint32_t iContID)
{
    AutoStr str;
    const char *Ip = getIpOfAddr(pSockAddr, str);
    Ip2NodeInfoMap_t::iterator itr;
    Ip2NodeInfoMap_t &infoMap = getNodeInfoMgr()->getNodeInfoMap();
    LcNodeInfo *pPeerlStatus;
    for (itr = infoMap.begin(); itr != infoMap.end(); itr = infoMap.next(itr) )
    {
        pPeerlStatus    = static_cast<LcNodeInfo *>(itr.second());
        LS_DBG_M("LcReplGroup::setNewMstrClearOthers addr:%s, Ip:%s", itr.first(), Ip);

        if ( !strcmp(itr.first(), Ip) )
        {
            pPeerlStatus->setRole(iContID - 1, R_MASTER);
        }
        else
            pPeerlStatus->setRole(iContID - 1, R_SLAVE);
    }
    return true;
}

//search max tid from IP of inlist, and IP list with max tid
uint64_t LcReplGroup::getClientsMaxTid(const StringList &inlist, int idx, uint64_t myTid, StringList &outlist)
{
    HashStringMap< LcNodeInfo *>::iterator hitr;
    StringList::const_iterator itr;
    const char* pSockAddr;
    for ( itr = inlist.begin(); itr != inlist.end(); itr++ )
    {
        pSockAddr = ( *itr )->c_str();
        LS_DBG_M("LcReplGroup::getClientsMaxTid, addr:%s", pSockAddr);
        hitr = getNodeInfoMgr()->getNodeInfoMap().find(pSockAddr);
        assert(hitr != NULL);
        LcNodeInfo *pStatus = hitr.second();
        uint64_t currTid =  pStatus->getCurrTid(idx);
        if (currTid >  myTid)
        {
            LS_DBG_M( "node[%s] priority %lld > myTid %lld", 
                      pSockAddr, (long long)currTid, (long long)myTid);
            return currTid;
        }
        else if (currTid == myTid)
        {
            outlist.add(pSockAddr);
        }
    }
    return myTid;
}

//the min IP is the one
bool LcReplGroup::isSelfMinAddr(const StringList &clntlist, const char* pMyAddr)
{
    char pSvrAddr[64];
    StringList::const_iterator itr;
    for ( itr = clntlist.begin(); itr != clntlist.end(); itr++ )
    {
        if (isEqualIpOfAddr(pSvrAddr, pMyAddr) < 0)
        {
            LS_INFO(  "Myself[%s] is not min Addr, at leaset node[%s] exists",
                 pMyAddr, pSvrAddr);
            return false;
        }
    }
    LS_INFO(  "MySelf[%s] is the min Addr", pMyAddr);
    return true;
}

//FIXME
bool LcReplGroup::isLegalClientBeMstr(int idx, uint64_t myTid, uint32_t myPriority)
{
    char pReplSvrAddr[64];
    LcNodeInfo *pPeerStatus = NULL;
    const char *pClntAddr     = NULL;
    StringList slist;
    Ip2NodeInfoMap_t::iterator itr;
    Ip2NodeInfoMap_t &infoMap = getNodeInfoMgr()->getNodeInfoMap();
    //1)compare priority
    for (itr = infoMap.begin(); itr != infoMap.end(); itr = infoMap.next(itr) )
    {
        pClntAddr   = itr.first();
        pPeerStatus = static_cast<LcNodeInfo *>(itr.second());
        if (pPeerStatus->getPriority(idx) > myPriority)
        {
            const StaticNodeInfo *pStatic = getStNodeInfoMgr()->getNodeInfo(pClntAddr);
            StaticNodeInfo::getSvrAddr(pStatic->getSvrIp(), pStatic->getSvrPort()
                , pReplSvrAddr, 64);
            LS_DBG_M(  "node[%s] has higher priority %d > %d, addr:%s",
                pReplSvrAddr, pPeerStatus->getPriority(idx), myPriority, pClntAddr);
            return false;
        }
        else if (pPeerStatus->getPriority(idx) == myPriority)
        {
            LS_DBG_M(  "add %s for tid compare", itr.first());
            slist.add(itr.first());
        }
    }
    if (slist.size() == 0)
    {
        LS_DBG_M(  "isLegalClientBeMstr: I have highest priority %d", myPriority);
        return true;
    }
    //2)compare Tid value among highest priority node(s) including myself
    StringList clntList;
    uint64_t maxTid = getClientsMaxTid(slist,idx, myTid, clntList);
    if (maxTid > myTid) //someone is bigger
        return false;

    //3) compare IP:port
    if (clntList.size() == 0)//I'm biggest
        return true;
    else
        return isSelfMinAddr(clntList, ConfWrapper::getInstance()->getLisenSvrAddr());

}

bool LcReplGroup::isFullReplNeeded(uint32_t contId, const NodeInfo *pLocalInfo, const NodeInfo *pPeerInfo)
{
    const LcNodeInfo * pLocal = static_cast<const LcNodeInfo*>(pLocalInfo);
    const LcNodeInfo * pPeer = static_cast<const LcNodeInfo*>(pPeerInfo);
    
    int idx = contId -1;
    if(pLocal->getCurrTid(idx) == pPeer->getCurrTid(idx))
    {
        if (R_MASTER == pLocal->getRole(idx))
            return true;
    }
    else if (pLocal->getCurrTid(idx) > pPeer->getCurrTid(idx))
    {
        return true;
    }
    return false;   
}
