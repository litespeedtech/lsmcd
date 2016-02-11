#ifndef LCREPLSENDER_H
#define LCREPLSENDER_H
#include "repl/replicable.h"
#include "repl/replconn.h"
#include <repl/replsender.h>
#include <util/stringlist.h>

class LcReplSender : public ReplSender
{
public:
    LcReplSender();
    virtual ~LcReplSender();
    static LcReplSender &getInstance();
    
    int clientJoinListenSvrReq(ReplConn * pConn );
    int clientJoinListenSvrAck(ReplConn * pConn );
    
    virtual bool sendLocalReplStatus(ReplConn * pConn, uint32_t uiType);
    
    int mstr2ClientFlipReq(ReplConn * pConn, uint32_t iContID);
    int client2MstrFlipAck(ReplConn * pConn, uint32_t iContID);
    int mstr2ClientFlipDone(ReplConn * pConn, uint32_t iContID);
    int newMstrElectDone(ReplConn * pConn, uint32_t iContID);
    
    int sendTidHashData (ReplConn *pReplNode, int idx, uint64_t iPeerTid );
    uint64_t bcastReplicableData (int idx, uint64_t iMstrLstTid);
    int bcastNewMstrElectDone(uint32_t iContID);
};

#endif
