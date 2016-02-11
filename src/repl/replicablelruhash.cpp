
#include "replicablelruhash.h"
#include "replpacket.h"
#include <log4cxx/logger.h>
#include <util/autobuf.h>

#include <stdio.h>
#include <assert.h>

LruHashData::LruHashData(uint32_t lruTm, unsigned char *pId, int idLen,
      unsigned char *pData, int iDataLen)
    : m_lruTm(lruTm)
    , m_idLen(idLen)
    , m_iDataLen(iDataLen)
    , m_pId(pId)
    , m_pData(pData)  
{
}

LruHashData::LruHashData(const LruHashData & sd)
{
    if ( this == &sd)
      return;
    m_lruTm     = sd.m_lruTm;
    m_idLen	= sd.m_idLen ;
    m_iDataLen	= sd.m_iDataLen;
    m_pId 	= sd.m_pId;
    m_pData	= sd.m_pData;      
}

int LruHashData::getSize ()
{
    return sizeof(m_lruTm) + sizeof(m_idLen) + sizeof(m_iDataLen) 
        + m_idLen + m_iDataLen;
}


ReplicableLruHash::~ReplicableLruHash()
{}


int ReplicableLruHash::addReplicalbe(uint32_t lruTm, unsigned char *pId, int idLen,
  unsigned char *pData, int iDataLen)
{
   m_lruHashData = LruHashData(lruTm, pId, idLen, pData, iDataLen);
   return 0;
}    
    
    
const LruHashData * ReplicableLruHash::getReplicable() const 
{
    if(m_lruHashData.m_idLen == 0)
        return NULL;
    else
        return &m_lruHashData;
}
    
    
int ReplicableLruHash::getsize ()
{
    return m_lruHashData.getSize();
}


const char * ReplicableLruHash::getKey ( int & keyLen )
{
    keyLen = m_lruHashData.m_idLen; 
    return  (const char*)m_lruHashData.m_pId;
}


int ReplicableLruHash::packObj ( int32_t id, char * pBuf, int bufLen )
{
    char* ptemBuf = pBuf;
    char * pEndBuf = pBuf + bufLen;
    int iSavedlen = ReplPacker::pack ( ptemBuf, pEndBuf - ptemBuf,  (uint32_t)m_lruHashData.m_lruTm );
    ptemBuf += iSavedlen; 
    
    iSavedlen = ReplPacker::pack ( ptemBuf, pEndBuf - ptemBuf,  (uint32_t)m_lruHashData.m_idLen  );
    ptemBuf += iSavedlen; 

    iSavedlen = ReplPacker::pack ( ptemBuf, pEndBuf - ptemBuf,  (uint32_t)m_lruHashData.m_iDataLen  );
    ptemBuf += iSavedlen; 
    
    iSavedlen = ReplPacker::packRawStr ( ptemBuf, pEndBuf - ptemBuf
                , (const char*)(m_lruHashData.m_pId), m_lruHashData.m_idLen); 
    ptemBuf  += iSavedlen;
    
    iSavedlen = ReplPacker::packRawStr ( ptemBuf, pEndBuf - ptemBuf
                , (const char*)(m_lruHashData.m_pData), m_lruHashData.m_iDataLen); 
    ptemBuf  += iSavedlen;
   
    assert( ptemBuf == pEndBuf );
    return getsize();
}


int ReplicableLruHash::packObj ( int32_t id, AutoBuf* pBuf )
{
    pBuf->append(( const char* )&m_lruHashData.m_lruTm      , sizeof (uint32_t));
    pBuf->append(( const char* )&m_lruHashData.m_idLen      , sizeof ( int));
    pBuf->append(( const char* )&m_lruHashData.m_iDataLen   , sizeof ( int ));

    pBuf->append(( const char* )m_lruHashData.m_pId         , m_lruHashData.m_idLen);
    pBuf->append(( const char* )m_lruHashData.m_pData       , m_lruHashData.m_iDataLen);    
    return getsize();
}

#include <replsender.h>
int ReplicableLruHash::unpackObj ( char * pBuf, int bufLen )
{
    char* ptemBuf = pBuf;
    int temBufLen = bufLen;

    int iCopiedlen = ReplPacker::unpack ( ptemBuf, m_lruHashData.m_lruTm );
    ptemBuf += iCopiedlen;
    temBufLen -= iCopiedlen;

    iCopiedlen = ReplPacker::unpack ( ptemBuf, m_lruHashData.m_idLen );
    ptemBuf += iCopiedlen;
    temBufLen -= iCopiedlen;
    
    iCopiedlen = ReplPacker::unpack ( ptemBuf, m_lruHashData.m_iDataLen);
    ptemBuf += iCopiedlen;
    temBufLen -= iCopiedlen;
   
    m_lruHashData.m_pId         = (unsigned char*)(pBuf + 3 * sizeof (int));
    m_lruHashData.m_pData       = m_lruHashData.m_pId + m_lruHashData.m_idLen;
    
    return m_lruHashData.getSize();
}


