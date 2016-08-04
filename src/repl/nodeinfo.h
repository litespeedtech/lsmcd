#ifndef NODEINFO_H
#define NODEINFO_H
#include "lsdef.h"
#include "repllistener.h"
#include <repl/repldef.h>
#include <util/dlinkqueue.h>

class SockConnMgr;
class JsonStatus;
class AutoBuf;
class Int2IntMap;
class NodeInfo
{
    friend class JsonStatus;
public:
    NodeInfo(const char *pAddr);
    virtual ~NodeInfo();
    virtual int copyAll(NodeInfo* rhs);
    virtual bool        isSyncWith(const NodeInfo &peerNode) const;
    virtual int         refresh();

    virtual int         serialize (AutoBuf &rAutoBuf);
    static NodeInfo * deserialize(const char *pBuf, int dataLen, const char* pAddr);
    
    int         getContIdVal(int contId) const;  

    void        setUpTime(uint32_t tm)          {      m_upTime = tm;           }
    uint32_t    getUpTime() const               {      return m_upTime;         }

    void        setSyncTime(uint32_t tm)        {      m_syncTime = tm;         }
    uint32_t    getSyncTime() const             {      return m_syncTime;       }

    bool        cmpLeaderNode(const NodeInfo *pLeaderNode
        , bool &isLeader, bool &isSync) const;    
    const char *getIp() const                   {    return m_Ip.c_str();       }

    void        DEBUG() const;
    void        mkJson(const char * ip, bool isLocal, bool isLeader, bool isSyncing
                , bool isSync, AutoBuf &rAutoBuf) const;

    const Int2IntMap &geCntId2CntMap() const    {    return m_CntId2CntMap;     }
    Int2IntMap *geCntId2CntMapPtr()             {    return &m_CntId2CntMap;     }
    
private:
    void        setIp(const char *pAddr);
    LS_NO_COPY_ASSIGN(NodeInfo);    
private:    
    uint32_t    m_syncTime;
    uint32_t    m_upTime;
    Int2IntMap  m_CntId2CntMap;
    
    AutoStr             m_Ip;
    JsonStatus         *m_pStatusFormater;
};

typedef HashStringMap<NodeInfo *>   Ip2NodeInfoMap_t;

class NodeInfoMgr
{
public:
    virtual ~NodeInfoMgr();
    virtual int  addNodeInfo(NodeInfo *pNodeInfo);
    int  updateNodeInfo (NodeInfo & nodeInfo);
    int  updateNodeInfo (const char *pClntAddr, int contId, int count);
    int  updateNodeInfo (const char *pClntAddr, const char *pData, int len);
    void delNodeInfo(const char *Ip);
    int  replaceNodeInfo(NodeInfo *pNewInfo);
    
    bool isSelfFirstUp();
    NodeInfo *getLocalNodeInfo(bool brefresh = true);    
    const NodeInfo   * getLeaderNode() const;        
    Ip2NodeInfoMap_t & getNodeInfoMap()        {       return m_hNodeInfoMap;      }
    int  size() const   {       return m_hNodeInfoMap.size();   }
    NodeInfo * getNodeInfo(const char * pAddr);
    void getGroupJsonStatus(SockConnMgr *pSockConnMgr, JsonStatus *pStatusFormater, AutoBuf &rAutoBuf);
protected:    
    Ip2NodeInfoMap_t    m_hNodeInfoMap;
    
};


#endif
