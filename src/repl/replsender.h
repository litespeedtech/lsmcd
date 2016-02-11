#ifndef REPLSENDER_H
#define REPLSENDER_H
#include "replicable.h"
#include "sockconn.h"
#include <util/dlinkqueue.h>
#include <util/stringlist.h>
#include <util/tsingleton.h>
#include <sys/socket.h>

class BulkReplCtx;

class ReplSender
{
public:
    ReplSender();
    virtual ~ReplSender();

    uint32_t getSeqNum();
    virtual bool sendLocalReplStatus(ReplConn * pConn, uint32_t uiType);
    bool sockClientPingSvrStatus (ReplConn * pConn );

    bool notifyClntNeedFRepl(ReplConn* pConn);
    
    bool sockClientHandShakeSvr (ReplConn * pConn );
    
    int  sendLruHashData (int contID, uint32_t lruTm, unsigned char *pId, int idLen,
      unsigned char *pData, int iDataLen);
    int  sendLruHashBlukData   (ReplConn *pConn, int contId, AutoBuf &rAutoBuf);

    bool clntSendSelfRTHash(ReplConn * pConn, AutoBuf *pAutoBuf);
    int  bcastReplicableData (Replicable* pRepl, int contID, uint32_t lruTm);
    int  sendACKPacket( ReplConn * pConn, ReplPacketHeader * header, int type, const char *pBuf, int len );
private:
    uint32_t m_uiSequenceNum;
    LS_NO_COPY_ASSIGN(ReplSender)    
};

ReplSender * getReplSender();
void setReplSender (ReplSender *pSender);
#endif // REPLSENDER_H
