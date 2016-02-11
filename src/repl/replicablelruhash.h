#ifndef  REPLICABLESESSID_H
#define REPLICABLESESSID_H

#include "replicable.h"

/*
 * note this data pack only consider the total size 
 */
class AutoBuf;
class LruHashData
{
public:
    LruHashData()   {}
    LruHashData(uint32_t lruTm, unsigned char *pId, int idLen,
      unsigned char *pData, int iDataLen);
    LruHashData(const LruHashData & sd);
    int getSize ();
public:    
    uint32_t            m_lruTm;
    int                 m_idLen;
    int                 m_iDataLen;
    unsigned char      *m_pId;
    unsigned char      *m_pData;  
};

class ReplicableLruHash : public Replicable
{
public:
    ReplicableLruHash() {}
    int addReplicalbe(uint32_t tm, unsigned char *pId, int idLen,
      unsigned char *pData, int iDataLen);
    const LruHashData * getReplicable() const ;
    virtual ~ReplicableLruHash();
    virtual int getsize ();
    virtual int packObj ( int32_t id, char * pBuf, int bufLen );
    virtual int unpackObj ( char * pBuf, int bufLen );
    virtual int packObj ( int32_t id, AutoBuf* pBuf );
    virtual const char * getKey ( int & keyLen );
private:
    LruHashData m_lruHashData;
};


#endif 
