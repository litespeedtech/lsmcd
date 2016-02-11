#ifndef _LCREPLGROUP_H
#define _LCREPLGROUP_H
#include "lsdef.h"
#include "repl/replconn.h"

#include "lcrepl/lcnodeinfo.h"
#include "lcshmevent.h"
#include <repl/replgroup.h>
#include <memcache/rolememcache.h>
#include <util/dlinkqueue.h>
#include <util/hashstringmap.h>
#include <util/stringlist.h>
#include <edio/multiplexer.h>

enum
{
    CONT_ZERO     = 0,
    CONT_TID
};

class LcReplGroup : public ReplGroup
{
public:
    LcReplGroup();
    virtual ~LcReplGroup();
    static LcReplGroup &getInstance();
    
    void initSelfStatus ();
    void delClntInfoAll(const char *sockAddr);
    
    virtual int  initReplConn();
    virtual int  connAllListenSvr(Multiplexer* pMultiplexer);
    virtual int  clearClntOnClose(const char *pAddr);
    virtual bool isFullReplNeeded(uint32_t contId, const NodeInfo *pLocalInfo, const NodeInfo *pPeerInfo);
    void setR2cEventRef( LsRepl2CacheEvent * pR2cEventArr)      {       m_pR2cEventArr = pR2cEventArr;  }
    int  notifyRoleChange(int idx, const char *pAddr, int len);
    
    LcNodeInfo *getLocalNodeInfo();   
    virtual int onTimer1s();
    virtual int onTimer30s();
    int setNewMstrClearOthers(const char* pSockAddr, uint32_t iContID);
    StNodeInfoMgr * getStNodeInfoMgr()  {       return m_pStNodeInfoMgr;  }
    bool initReplRegistry(Multiplexer* pMultiplexer);
    void DEBUG();

private:
    bool roleExistInGroup(int idx, uint16_t role);
    bool isAllClientSync(const LcNodeInfo *  pLocalStatus, int idx);
    const char *hasHighPriorityClient(uint32_t myPriority, int idx);
    bool isLegalClientBeMstr(int idx, uint64_t myTid, uint32_t myPriority);
    bool isSelfMinAddr(const StringList &inlist, const char* pSvrAddr);
    int  flipSelfBeMstr();
    int  clientsElectMstr(const LcNodeInfo * pLocalStatus, int idx);
    void reorderClientHBFreq(int connCount);
    uint64_t getClientsMaxTid(const StringList &inlist, int idx, uint64_t myTid, StringList &outlist);
    
    int  monitorRoles();    
private:
    StNodeInfoMgr       *m_pStNodeInfoMgr;
    LsRepl2CacheEvent   * m_pR2cEventArr;
};

#endif // REPLCONTAINER_H
