#ifndef REPLCONTAINER_H
#define REPLCONTAINER_H

#include "replicable.h"
#include <util/autostr.h>
#include <util/ghash.h>
#include <util/stringlist.h>
#include <util/dlinkqueue.h>
#include <socket/gsockaddr.h>
#include "lsdef.h"

#define LS_UNPACK_ERR   -2

class ReplicableAllocator
{
public:
    ReplicableAllocator()           {}
    virtual ~ReplicableAllocator()  {}
    virtual Replicable * allocate() = 0;
    void release ( Replicable * pObj )
    {
        delete pObj;
    }
};

template <class _Obj >
class TReplicableAllocator : public ReplicableAllocator
{
public:
    Replicable * allocate()
    {
        return new _Obj;
    }

};
class ReplProgressTrker;
class LruHashReplBridge;
class ReplContainer
{

public:
    ReplContainer ( int id, ReplicableAllocator * pAllocator )
        : m_iId ( id )
        , m_pAllocator ( pAllocator )
    {}
    virtual ~ReplContainer();
    LruHashReplBridge * getHashBridge() const   {       return m_pHashBridge;   }
    virtual Replicable * getObj ( const char * pKey, int keyLen ) { return NULL;}
    int32_t getId() const       {       return m_iId;   }
    void setId ( int32_t id )   {       m_iId = id;     }

    virtual int size() {   return 0;   }
    virtual int addAndUpdateObj ( Replicable * pObj, int iUpdate = 1) {   return 0;   }
    virtual int addAndUpdateData ( uint8_t * pData, int dataLen, int iUpdate = 1) {   return 0;   }
    virtual int purgeExpired() = 0;
    virtual bool        isReady() const                 {       return true;    }
    virtual uint32_t    getHeadLruHashTm() const;
    virtual uint32_t    getTailLruHashTm() const;
    virtual uint32_t    getCurrLruNextTm(uint32_t tm) const;
    
    virtual int         getLruHashCnt() const           {       return 0;       }           
    virtual int         getLruHashKeyHash(uint32_t startTm, uint32_t endTm, AutoBuf &rAutoBuf) const;
    virtual int         readShmKeyVal(ReplProgressTrker &, int availSize, AutoBuf &rAutoBuf) const; 
    void                setName(const char *pName)      {       m_sName = pName;        }
    const char*         getName() const                 {       return m_sName.c_str();         }    
protected:
    int32_t  m_iId;
    AutoStr  m_sName;
    ReplicableAllocator * m_pAllocator;
    LruHashReplBridge *m_pHashBridge;    
private:
    LS_NO_COPY_ASSIGN (ReplContainer)    
};

#endif // REPLCONTAINER_H
