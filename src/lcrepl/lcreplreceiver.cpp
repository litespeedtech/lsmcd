
#include "lcreplreceiver.h"
#include "lcnodeinfo.h"
#include "lcreplpacket.h"
#include "replshmhelper.h"
#include "replicableshmtid.h"
#include "lcreplgroup.h"
#include "lcreplconf.h"
#include "lcreplsender.h"
#include "replshmtidcontainer.h"
#include "usockmcd.h"

#include <repl/replregistry.h>
#include <repl/replsender.h>
#include <repl/replconn.h>
#include <log4cxx/logger.h>

#include <stddef.h>
#include <stdio.h>


LcReplReceiver::LcReplReceiver()
{}
 
LcReplReceiver::~LcReplReceiver()
{}

LcReplReceiver &LcReplReceiver::getInstance()
{
    return *(static_cast<LcReplReceiver*>(getReplReceiver()));
}

int LcReplReceiver::processPacket ( ReplConn * pConn, ReplPacketHeader * header, char * pData )
{
    switch ( header->getType() )
    {
    case     RT_DATA_UPDATE:
        LS_DBG_M(  "LcReplReceiver::processPacket---Got a RT_DATA_UPDATE Packet SequenceNum=%d from %s, Length=%d\n",
                     header->getSequenceNum(), pConn->getPeerAddr(), header->getPackLen() );
        handleData ( pConn, header, pData, 0 );
        break;
    case     RT_ACK:
        handleDataAck( pConn, header, pData );
        break;
    case     RT_REJECT:
        handleAckReject ( pConn, header );
        break;
    case     RT_C2M_JOINREQ:
        LS_DBG_M(  "LcReplReceiver::processPacket---Got a RT_C2M_JOINREQ, ConID=%d, Key=%s, SequenceNum=%d from %s\n",
                     header->getContID(), pData, header->getSequenceNum(), pConn->getPeerAddr() ) ;
        svrHandleClntJoinReq( pConn, header, pData );
        break;
    case     RT_M2C_JOINACK:
        LS_DBG_M(  "LcReplReceiver::processPacket---Got a RT_M2C_JOINACK, ConID=%d, Key=%s, SequenceNum=%d from %s\n",
                     header->getContID(), pData, header->getSequenceNum(), pConn->getPeerAddr() ) ;
        clntHandleJoinSvrResp( pConn, header, pData );
        break;
    case     RT_REPL_PING:
        LS_DBG_M(  "LcReplReceiver::processPacket---Got a RT_REPL_PING, ConID=%d, Key=%s, SequenceNum=%d from %s\n",
                     header->getContID(), pData, header->getSequenceNum(), pConn->getPeerAddr() ) ;
        svrHandleClientPingStatus ( pConn, header, pData );
        break;

    case     RT_REPL_ECHO:
        LS_DBG_M(  "LcReplReceiver::processPacket---Got a RT_REPL_ECHO, ConID=%d, Key=%s, SequenceNum=%d from %s\n",
                     header->getContID(), pData, header->getSequenceNum(), pConn->getPeerAddr());
        clientHandleSvrEchoStatus ( pConn, header, pData );
        break;
        
    case     RT_M2C_MSTRFLIPING:
        LS_DBG_M(  "LcReplReceiver::processPacket---Got a RT_M2C_MSTRFLIPING, ConID=%d, SequenceNum=%d from %s\n",
                     header->getContID(), header->getSequenceNum(), pConn->getPeerAddr() );
        clientHandleMasterFliping( pConn, header, pData );
        break;
    case     RT_C2M_FLIPINGACK:
        LS_DBG_M(  "LcReplReceiver::processPacket---Got a RT_C2M_FLIPINGACK, ConID=%d, Key=%s, SequenceNum=%d from %s\n",
                     header->getContID(), pData, header->getSequenceNum(), pConn->getPeerAddr() ) ;
        masterHandleClientToFlip ( pConn, header, pData );
        break;
    case     RT_M2C_FLIPDONE:
        LS_DBG_M(  "LcReplReceiver::processPacket---Got a RT_M2C_FLIPDONE, ConID=%d, SequenceNum=%d from %s\n",
                     header->getContID(), header->getSequenceNum(), pConn->getPeerAddr() );
        clientHandleTakeMstr( pConn, header, pData );
        break;        
    case    RT_GROUP_NEWMASTER:
        LS_DBG_M(  "LcReplReceiver::processPacket---Got a RT_GROUP_NEWMASTER, ConID=%d, SequenceNum=%d from %s\n",
                     header->getContID(), header->getSequenceNum(), pConn->getPeerAddr() );
        handleNewElectedMaster( pConn, header, pData );        
        break;
        
    case     RT_C2M_FRPEL_BOTHDONE:
        LS_DBG_M(  "LbReplReceiver::processPacket---Got a RT_C2M_FRPEL_BOTHDONE, ConID=%d, SequenceNum=%d from %s",
                     header->getContID(), header->getSequenceNum(), pConn->getPeerAddr());
        handleFreplBothDone ( pConn, header, pData );
        break;        
        
    }
    return LS_OK;
}


int LcReplReceiver::respondPacket(ReplConn *pReplConn, ReplPacketHeader * pHeader, int iType, int iBulk)
{
    AutoBuf autoBuf;
    uint64_t iTid = 0;
    int contId = pHeader->getContID();
    if ( contId > 0)
    {
        iTid   = ReplShmHelper::getInstance().getLastShmTid(contId - 1);
    }
    autoBuf.append((const char*)&iBulk, sizeof(int) );
    autoBuf.append((const char*)&iTid, sizeof(uint64_t) );
    getReplSender()->sendACKPacket(pReplConn, pHeader, iType
        , autoBuf.begin(), autoBuf.size());
    return LS_OK;
}

int LcReplReceiver::handleData ( ReplConn *pConn, ReplPacketHeader * header,
                               char * pData, int iBulk )
{
    uint32_t contId = header->getContID();
    ReplContainer* pReplContainer = ReplRegistry::getInstance().get ( contId );
    if ( pReplContainer == NULL )
    {
        LS_WARN ("LcReplReceiver::handleData(...)---container %d not found", contId );
        respondPacket(pConn, header, RT_REJECT, iBulk);
        return LS_FAIL;
    }
    uint64_t iPeerTid = *(uint64_t *)pData;
    LcNodeInfo *pPeerNode = (LcNodeInfo *) getReplGroup()->getNodeInfoMgr()->getNodeInfo(pConn->getPeerAddr());
    if (pPeerNode != NULL)
    {
        pPeerNode->setCurrTid(contId -1, iPeerTid);
    
        int dataLen = header->getPackLen() - sizeof ( *header ) - sizeof(uint64_t);
        LS_DBG_M("LcReplReceiver::handleData idx:%d, iPeerTid:%lld, dataLen:%d", 
                 contId -1, (long long)iPeerTid, dataLen);
        saveDataToContainer ( pReplContainer, pConn, header, pData + sizeof(uint64_t)
            , dataLen, iBulk); 
    }
    else
        LS_DBG_M("code bug...");
    return LS_OK;
}

int LcReplReceiver::saveDataToContainer ( ReplContainer* pReplContainer, ReplConn *pReplNode,
                                        ReplPacketHeader * header, char * pData, int dataLen, int iBulk)
{
    static uint32_t lastTm = time(NULL);
    if ( pReplContainer->addAndUpdateData ( ( uint8_t * )(pData), dataLen, 0) == 0 )
    {
        LS_DBG_M(  "add_And_Update_Obj to container %d successfully isBulk=%d, dataLen=%d",
                        header->getContID(), iBulk, dataLen ) ;
        if ( time(NULL) - lastTm > 1)         //void too much ACK at big traffic                  
        {
            respondPacket(pReplNode, header, RT_ACK, iBulk);
            lastTm = time(NULL);
        }
    }
    else
    {
        LS_NOTICE(  "add_And_Update_Obj to container %d failed, Object already exists isBulk=%d, dataLen=%d",
                        header->getContID(), iBulk, dataLen);  
        respondPacket(pReplNode, header, RT_REJECT, iBulk);
    }
    return 0;
}

int LcReplReceiver::handleDataAck (ReplConn *pConn, ReplPacketHeader * header,
                               char * pData)
{  
    int contId = header->getContID();
    if (contId == 0)
        return LS_OK;
    int iBulk           = *(int *)pData;           
    uint64_t iTid       = *(uint64_t *)(pData + sizeof(int));
    LS_DBG_M("pid[%d]  LcReplReceiver::handleDataAck, contId:%d, iBulk:%d, peer Tid:%lld"
        , getpid(), contId, iBulk, (long long)iTid);

    LcNodeInfo *pPeerNode = (LcNodeInfo *) getReplGroup()->getNodeInfoMgr()->getNodeInfo(pConn->getPeerAddr());
    pPeerNode->setCurrTid(contId -1, iTid);    
    return LS_OK;    
}

int LcReplReceiver::svrHandleClntJoinReq(ReplConn* pConn, ReplPacketHeader* header, char* pData)
{
    if (!LcReplGroup::getInstance().isLstnrProc())
        return false;
    int dataLen = header->getPackLen() - sizeof(ReplPacketHeader); 
    StaticNodeInfo *pPeerStaticStatus = StaticNodeInfo::deserialize (
        pData, dataLen);
    pPeerStaticStatus->setHashKey(pConn->getPeerAddr());
    LcReplGroup::getInstance().getStNodeInfoMgr()->addNodeInfo(pPeerStaticStatus );
    
    LcReplGroup::getInstance().DEBUG();
    char pSockAddr[64];    
    StaticNodeInfo::getSvrAddr(pPeerStaticStatus->getSvrIp()
        , pPeerStaticStatus->getSvrPort(), pSockAddr, 64);
    //1)check allow IP

    if (ConfWrapper::getInstance()->isAllowedIP(pSockAddr))
    {
        LcReplSender::getInstance().clientJoinListenSvrAck(pConn);
    }
    else
    {
        LS_WARN(  "Received joining request from invalid addr[%s]", pSockAddr);
        respondPacket(pConn, header, RT_REJECT, 0);
        
        return LS_FAIL;
    }
    return LS_OK;

}

int LcReplReceiver::clntHandleJoinSvrResp(ReplConn* pReplNode, ReplPacketHeader* header, char* pData)
{
    if (!LcReplGroup::getInstance().isLstnrProc())
        return false;
    
    const LcNodeInfo *pLocalStatus = LcReplGroup::getInstance().getLocalNodeInfo();
    int dataLen = header->getPackLen() - sizeof(ReplPacketHeader); 
    StaticNodeInfo *pPeerStatus = StaticNodeInfo::deserialize (
        pData, dataLen);
    
    char pSockAddr[64];
    StaticNodeInfo::getSvrAddr(pPeerStatus->getSvrIp(), pPeerStatus->getSvrPort(), pSockAddr, 64);
    LS_DBG_M(  "client connection to server[%s] is ready", pSockAddr);
    
    LcReplSender::getInstance().sockClientPingSvrStatus(pReplNode);
    
    StaticNodeInfo::getSvrAddr(pPeerStatus->getShmSvrIp(), pPeerStatus->getShmSvrPort(), pSockAddr, 64);
    LS_DBG_M(  "set all cached slices to pointing to master addr[%s]", pSockAddr);
    bool bUpdated = false;  
    for (int idx = 0; idx < (int)(pLocalStatus->getContCount()); ++idx)
    {
        if (R_SLAVE == pLocalStatus->getRole(idx))
        {
            UsockSvr::getRoleData()->setRoleAddr(idx, pSockAddr, strlen(pSockAddr));
            bUpdated =  true;
        }
    }
    if(bUpdated)
        LcReplGroup::getInstance().notifyRoleChange();
    LcReplGroup::getInstance().notifyConnChange();
    delete pPeerStatus;
    return LS_OK;

}

int LcReplReceiver::svrHandleClientPingStatus ( ReplConn *pConn, ReplPacketHeader * header, char * pData )
{
    int dataLen = header->getPackLen() - sizeof(ReplPacketHeader); 
    LcNodeInfo *pPeerStatus = LcNodeInfo::deserialize (
        pData, dataLen, pConn->getPeerAddr() );
    int ret = LcReplGroup::getInstance().getNodeInfoMgr()->replaceNodeInfo(pPeerStatus);
    LcReplGroup::getInstance().DEBUG();
    
    LcNodeInfo *pLocalStatus = LcReplGroup::getInstance().getLocalNodeInfo();
    
    //LcReplSender &sender = LcReplSender::getInstance();
    int contCount = pPeerStatus->getContCount();
    int needSync = 0;

    for (int idx = 0; idx < contCount; ++idx)
    {
        if (!pPeerStatus->isDidFsync(idx))
        {
            if (LcReplGroup::getInstance().isFullReplNeeded(idx +1, pLocalStatus, pPeerStatus))
            {
                pLocalStatus->setDidFsync(idx);
                pPeerStatus->setDidFsync(idx);
                LcReplSender::getInstance().sendTidHashData( pConn,idx, pPeerStatus->getCurrTid(idx));
                pConn->sendPacket(idx + 1, RT_C2M_FRPEL_BOTHDONE
                    , header->getSequenceNum(), NULL, 0);            
                needSync++;
            }
        }
    }
    
    if (needSync == 0) 
        LcReplSender::getInstance().sendLocalReplStatus(pConn, RT_REPL_ECHO);
    
    if (ret > 0)
        delete pPeerStatus;
    return LS_OK;
}

int LcReplReceiver::clientHandleSvrEchoStatus ( ReplConn *pConn, ReplPacketHeader * header, char * pData )
{
    int dataLen = header->getPackLen() - sizeof(ReplPacketHeader); 
    uint32_t iContID = header->getContID();
    int idx = iContID - 1;
    
    ReplRegistry& RegObj = ReplRegistry::getInstance();
    ReplShmTidContainer* pReplShmTidContainer = ( ReplShmTidContainer* ) RegObj.get ( iContID );
    if( pReplShmTidContainer == NULL )
        return -1;

    LcNodeInfo *pPeerStatus = LcNodeInfo::deserialize (
        pData, dataLen, pConn->getPeerAddr());
    const LcNodeInfo *pLocalStatus = LcReplGroup::getInstance().getLocalNodeInfo();
    uint64_t iMstrTid = pPeerStatus->getCurrTid(idx);
    uint64_t iSlaveTid = pLocalStatus->getCurrTid(idx);
   
    if (iSlaveTid > iMstrTid)
    {
        LS_ERROR( "Repl slave[%s] out of sync with master[%s], bailouts! master currTid=%lld;slave currTid=%lld" 
                  , pConn->getLocalAddr(), pConn->getPeerAddr(), 
                  (long long)iMstrTid, (long long)iSlaveTid);
    }
    else if (pLocalStatus->getCurrTid(idx) < pPeerStatus->getCurrTid(idx))
    {
        LS_ERROR(  "Repl slave[%s] missing new changes on master[%s], weird! master currTid=%lld;slave currTid=%lld" 
            , pConn->getLocalAddr(), pConn->getPeerAddr(), (long long)iMstrTid, 
                   (long long)iSlaveTid);
    }
    else
    {
        LS_DBG_M(  "Repl slave[%s] sync with master[%s]"
            , pConn->getLocalAddr(), pConn->getPeerAddr());
    }
    
    delete pPeerStatus;
    return 0;
}

int LcReplReceiver::handleAckReject ( ReplConn *pReplNode, ReplPacketHeader * header )
{
    return 0;
}

int LcReplReceiver::clientHandleMasterFliping ( ReplConn *pConn, ReplPacketHeader * header, char * pData)
{
    uint32_t iContID = header->getContID();
    
    LS_DBG_M(  "LcReplReceiver::clientHandleMasterFliping,contID=%d, SequenceNum=%d from %s\n",
          iContID, header->getSequenceNum(), pConn->getPeerAddr() );

    LcReplSender::getInstance().client2MstrFlipAck(pConn, header->getContID()); 
    return 0;
}

int LcReplReceiver::masterHandleClientToFlip ( ReplConn *pConn, ReplPacketHeader * header, char * pData)
{
    uint32_t iContID = header->getContID();
    
    LS_DBG_M(  "LcReplReceiver::masterHandleClientToFlip,contID=%d, SequenceNum=%d from %s\n",
          iContID, header->getSequenceNum(), pConn->getPeerAddr() );

    LcReplSender::getInstance().mstr2ClientFlipDone(pConn, header->getContID());
    
    LcNodeInfo *pLocalStatus = (LcNodeInfo *)LcReplGroup::getInstance().getLocalNodeInfo();
    pLocalStatus->setRole(header->getContID() - 1, R_SLAVE);    
    return 0;
}

int LcReplReceiver::clientHandleTakeMstr ( ReplConn *pConn, ReplPacketHeader * header, char * pData)
{
    uint32_t iContID = header->getContID();
    
    LS_DBG_M(  "LcReplReceiver::clientHandleTakeMstr,contID=%d, SequenceNum=%d from %s\n",
          iContID, header->getSequenceNum(), pConn->getPeerAddr() );

    LcNodeInfo *pLocalStatus = (LcNodeInfo *)LcReplGroup::getInstance().getLocalNodeInfo();
    pLocalStatus->setRole(header->getContID() - 1, R_MASTER);  
    LcReplGroup::getInstance().setNewMstrClearOthers(ConfWrapper::getInstance()->getLocalLsntnrIp(), iContID);

    LcReplSender::getInstance().bcastNewMstrElectDone (header->getContID());
    
    //notify cached of new master
    LS_DBG_M(  "LcReplReceiver::clientHandleTakeMstr notify I'm new master, idx=%d"
        , header->getContID() - 1); 
    UsockSvr::getRoleData()->setRoleAddr(header->getContID() - 1, NULL, 0);
    LcReplGroup::getInstance().notifyRoleChange();
    
    return 0;
}

int LcReplReceiver::handleNewElectedMaster(ReplConn *pConn, ReplPacketHeader * header, char * pData )
{
    uint32_t iContID = header->getContID();
    
    LS_DBG_M(  "LcReplReceiver::handleNewElectedMaster,contID=%d, SequenceNum=%d from %s\n",
          iContID, header->getSequenceNum(), pConn->getPeerAddr() );

    ReplRegistry& RegObj = ReplRegistry::getInstance();
    ReplShmTidContainer* pReplShmTidContainer = ( ReplShmTidContainer* ) RegObj.get ( iContID );
    if ( pReplShmTidContainer == NULL )
    {
        LS_ERROR("handleNewElectedMaster can't find the contID %d, new master status can't reset", iContID);
        return LS_FAIL;
    }
    LcReplGroup & group = LcReplGroup::getInstance();
    group.setNewMstrClearOthers(pConn->getPeerAddr(), iContID);
    
    //notify cached of new master
    char pSockAddr[64];
    const StaticNodeInfo *pStaticStaus = group.getStNodeInfoMgr()->getNodeInfo(pConn->getPeerAddr());
    StaticNodeInfo::getSvrAddr(pStaticStaus->getShmSvrIp(), pStaticStaus->getShmSvrPort()
        , pSockAddr, 64);
    LS_DBG_M(  "LcReplReceiver::handleNewElectedMaster notify master role idx=%d,addr=%s"
    , iContID -1, pSockAddr); 

    UsockSvr::getRoleData()->setRoleAddr(iContID -1, pSockAddr, strlen(pSockAddr));
    group.notifyRoleChange();
    return 0;
}

int LcReplReceiver::handleFreplBothDone(ReplConn *pConn, ReplPacketHeader * header, char * pData )
{
    int idx = header->getContID() - 1;
    LS_DBG_M("LcReplReceiver::handleFreplBothDone idx:%d", idx);
    LcNodeInfo *pLocalStatus = LcReplGroup::getInstance().getLocalNodeInfo();
    pLocalStatus->setDidFsync(idx);
    return LS_OK;    
}
