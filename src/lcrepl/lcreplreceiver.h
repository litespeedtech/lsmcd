#ifndef LCREPLRECEIVER_H
#define LCREPLRECEIVER_H
#include <repl/replreceiver.h>
#include <stdint.h>

class ReplContainer;
class Replicable;
class ReplConn;
class CountBuf;
class ReplPacketHeader;

class LcReplReceiver : public ReplReceiver
{
public:
    LcReplReceiver();
    virtual ~LcReplReceiver();
    static LcReplReceiver &getInstance();
    int processPacket ( ReplConn * pConn, ReplPacketHeader * header, char * pData );
    int handleData ( ReplConn *pReplNode, ReplPacketHeader * header,
                     char * pData, int iBulk );
    int handleDataAck (ReplConn *pReplConn, ReplPacketHeader * header, char * pData);
    int svrHandleClntJoinReq(ReplConn* pReplNode, ReplPacketHeader* header, char* pData);
    int clntHandleJoinSvrResp(ReplConn* pReplNode, ReplPacketHeader* header, char* pData);

private:    
    int svrHandleClientPingStatus ( ReplConn *pReplNode, ReplPacketHeader * header, char * pData );
    int clientHandleSvrEchoStatus ( ReplConn *pReplNode, ReplPacketHeader * header, char * pData );
    int handleAckReject ( ReplConn *pReplNode, ReplPacketHeader * header );
    int clientHandleMasterFliping ( ReplConn *pReplNode, ReplPacketHeader * header, char * pData );
    int masterHandleClientToFlip ( ReplConn *pReplNode, ReplPacketHeader * header, char * pData );
    int clientHandleTakeMstr ( ReplConn *pReplNode, ReplPacketHeader * header, char * pData );
    int handleNewElectedMaster(ReplConn *pReplNode, ReplPacketHeader * header, char * pData ); 
    int handleFreplBothDone(ReplConn *pReplNode, ReplPacketHeader * header, char * pData ); 
    int saveDataToContainer ( ReplContainer* pReplContainer, ReplConn *pReplNode,
                              ReplPacketHeader * header, char * pData, int iLen, int iBulk);
    
    int respondPacket(ReplConn *pReplConn, ReplPacketHeader * pHeader, int iType, int iBulk);
    LS_NO_COPY_ASSIGN(LcReplReceiver);    
};

#endif
