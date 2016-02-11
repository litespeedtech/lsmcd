#ifndef LSNODEINFO_H
#define LSNODEINFO_H
#include "lsdef.h"
#include <repl/nodeinfo.h>
#include <util/dlinkqueue.h>
#include <util/hashstringmap.h>
#include <util/stringlist.h>
#include <edio/multiplexer.h>

#define ANYCONTID   -1

enum
{
    R_UNKNOWN=1,
    R_MASTER,
    R_SLAVE,
    R_FLIPING_MASTER, //ready to hander over, still be master
    R_FlIPING_SLAVE   //unack master, still be slave
};

class StaticNodeInfo
{
public:
    StaticNodeInfo(const char *pAddr);
    virtual ~StaticNodeInfo(){};
    StaticNodeInfo(const StaticNodeInfo& status);
        
    int totalSize() const;
    uint32_t     getSvrIp() const                   {   return m_SvrIP;        }
    uint16_t     getSvrPort() const                 {   return m_SvrPort;      }
    void         setSvrAddr(const char *pSockAddr);

    uint32_t     getShmSvrIp() const                   {   return m_ShmSvrIP;        }
    uint16_t     getShmSvrPort() const                 {   return m_ShmSvrPort;      }
    void         setShmSvrAddr(const char *pSockAddr);

    static const char  *getSvrIPStr(uint32_t IP, char * pBuf, int len);
    static const char  *getSvrPortStr(uint16_t port, char * pBuf, int len);
    static const char  *getSvrAddr(uint32_t IP, uint16_t port, char * pBuf, int len);

    int         serialize (AutoBuf &rAutoBuf);
    static StaticNodeInfo * deserialize(char *pBuf, int dataLen);
    void        DEBUG() const;
    int setHashKey(const char *pAddr);
    const char* getHashKey()         {       return m_hashKey.c_str();    }
private:    
    uint32_t m_SvrIP;
    uint16_t m_SvrPort;
    uint32_t m_ShmSvrIP;
    uint16_t m_ShmSvrPort;
    AutoStr  m_hashKey;
};


typedef HashStringMap<StaticNodeInfo *>   Ip2StNodeInfoMap_t;

class StNodeInfoMgr
{
public:
    int  addNodeInfo(StaticNodeInfo *pNodeInfo);
    void delNodeInfo(const char *pAddr);
    
    Ip2StNodeInfoMap_t & getNodeInfoMap()        {       return m_hNodeInfoMap;      }
    int size()  {       return m_hNodeInfoMap.size();   }
    StaticNodeInfo * getNodeInfo(const char * Ip);
    
private:    
    Ip2StNodeInfoMap_t m_hNodeInfoMap;
    
};

//channel status. 1 channel : 1 sub file

class LcNodeInfo : public NodeInfo
{
public:
    explicit  LcNodeInfo(int iContCount, const char *pAddr);
    virtual ~LcNodeInfo();
    LcNodeInfo(const LcNodeInfo& status);
	
    bool copyOne(int idx, const LcNodeInfo* status);
    bool copyOne(int idx, uint64_t currTid, uint32_t priority, uint32_t tidNum, uint16_t role);        
    virtual int copyAll(NodeInfo* rhs);
    virtual int         refresh();

    void        setCurrTid(int idx, uint64_t currTid)     {   m_pCurrTid[idx] = currTid;  }
    uint64_t    getCurrTid(int idx) const                 {   return m_pCurrTid[idx];     }
    
    void        setTidNum(int idx, uint32_t num)          {   m_pTidNum[idx] = num;       }
    uint32_t    getTidNum(int idx) const                  {   return m_pTidNum[idx];      }
    
    bool        incTidNum(int idx)                        {   ++m_pTidNum[idx];           }
    bool        decTidNum(int idx)                        {   --m_pTidNum[idx];           }    

    void        setRole(int idx, uint16_t role);
    uint16_t    getRole(int idx) const                    {   return m_pRole[idx];        }
    
    void        setDidFsync(int idx)                         {   m_pFsync[idx] = 1;         }        
    bool        isDidFsync(int idx) const                    {   return m_pFsync[idx] > 0;    }
    
    static const char *printStrRole(uint16_t role, char *pBuf, int len);
    void        setAllRole(uint16_t role);
    
    bool        isSynced(int idx, uint32_t peerNum, uint64_t peerMinTid, uint64_t peerCurrTid) const;
    bool        isSynced(int idx, const LcNodeInfo* pStatus) const;
    bool        isBadSynced(int idx, const LcNodeInfo* pStatus) const;

    uint32_t    getPriority(int idx) const                {   return m_pPriority[idx];      }
    uint32_t  * getPriority() const                       {   return m_pPriority;      }
    int         totalSize() const;
    uint32_t    getContCount() const                           {   return m_iContCount;    }
    virtual int         serialize (AutoBuf &rAutoBuf);
    static LcNodeInfo * deserialize(char *pBuf, int dataLen, const char* pClntAddr);
    
    uint32_t     getContID(int idx) const                  {   return m_pContId[idx];       }

    void        DEBUG() const;
    
private:    
    uint32_t  m_iContCount;
    uint32_t *m_pContId;
    uint64_t *m_pCurrTid;
    uint32_t *m_pPriority;
    uint32_t *m_pTidNum;
    uint16_t *m_pRole;
    uint16_t *m_pFsync;
    
};

class LcNodeInfoMgr : public NodeInfoMgr
{
public:
    virtual int addNodeInfo(NodeInfo *pNodeInfo);
    virtual ~LcNodeInfoMgr() {}
    const NodeInfo   * getLeaderNode() const;        
protected:
    NodeInfo * getNodeInfo(const char * Ip);
    
};
#endif // REPLCONTAINER_H
