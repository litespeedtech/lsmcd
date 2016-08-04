#ifndef _LCREPLGROUP_H
#define _LCREPLGROUP_H
#include "lsdef.h"
#include "repl/replconn.h"

#include "lcrepl/lcnodeinfo.h"
#include <repl/replgroup.h>
#include <util/dlinkqueue.h>
#include <util/hashstringmap.h>
#include <util/stringlist.h>
#include <edio/multiplexer.h>
#include "usockconn.h"
enum
{
    CONT_ZERO     = 0,
    CONT_TID
};
class UsocklListener;

class LcReplGroup : public ReplGroup
{
public:
    LcReplGroup();
    virtual ~LcReplGroup();
    static LcReplGroup &getInstance();
    
    void initSelfStatus ();
    void delClntInfoAll(const char *sockAddr);
    void setRef(UsocklListener * pUsockLstnr);
    virtual int  initReplConn();
    virtual int  connGroupLstnrs();
    virtual int  clearClntOnClose(const char *pAddr);
    virtual bool isFullReplNeeded(uint32_t contId, const NodeInfo *pLocalInfo, const NodeInfo *pPeerInfo);
    int  notifyRoleChange();
    int  notifyConnChange();
    LcNodeInfo *getLocalNodeInfo(bool bRefresh = true);   
    virtual int onTimer1s();
    virtual int onTimer30s();
    int setNewMstrClearOthers(const char* pSockAddr, uint32_t iContID);
    StNodeInfoMgr * getStNodeInfoMgr()  {       return m_pStNodeInfoMgr;  }
    void DEBUG();

private:
    bool roleExistInGroup(int idx, uint16_t role);
    bool isAllClientSync(const LcNodeInfo *  pLocalStatus, int idx);
    const char *hasHighPriorityClient(uint32_t myPriority, int idx);
    bool isLegalClientBeMstr(int idx, uint64_t myTid, uint32_t myPriority);
    bool isSelfMinAddr(const StringList &inlist, const char* pSvrAddr);
    int  flipSelfBeMstr();
    int  clientsElectMstr(const LcNodeInfo * pLocalStatus, int idx);
    uint64_t getClientsMaxTid(const StringList &inlist, int idx, uint64_t myTid, StringList &outlist);
    
    void  monitorRoles();    
private:
    StNodeInfoMgr       *m_pStNodeInfoMgr;
    UsocklListener      *m_pUsockLstnr;
    
};

#endif
