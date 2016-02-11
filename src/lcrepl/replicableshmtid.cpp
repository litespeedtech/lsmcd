
#include "replicableshmtid.h"
#include "replshmhelper.h"
#include <log4cxx/logger.h>

#include <stdio.h>
#include <assert.h>

ReplicableShmTid::ReplicableShmTid()
{}

ReplicableShmTid::~ReplicableShmTid()
{}
    
int ReplicableShmTid::getsize ()
{
    TPointerList<TidPacket>::iterator iter;

    int32_t iTotalSize = sizeof( uint32_t ) ;
    for (iter = m_tidPacketList.begin(); iter != m_tidPacketList.end();
         ++iter)
    {
        iTotalSize += (*iter)->m_iSize;
    }     
    
    LS_DBG_M(  "ReplicableShmTid getsize:%d\n", iTotalSize);    
    return iTotalSize; // total packet size    
}

const char * ReplicableShmTid::getKey ( int & keyLen ) 
{
    return NULL;
}

int ReplicableShmTid::addTidPacket(TidPacket* pTidPacket)
{
    uint32_t maxSize = ReplShmHelper::getInstance().getMaxPacketSize();
    TPointerList<TidPacket>::iterator iter;

    int32_t iTotalSize = sizeof( uint32_t ) + sizeof( ReplPacketHeader ) ;
    
    for (iter = m_tidPacketList.begin(); iter != m_tidPacketList.end();
         ++iter)
    {
        iTotalSize += (*iter)->m_iSize;
        if( maxSize <= (iTotalSize + pTidPacket->m_iSize) )
            return LS_FAIL;
    }        
    
    m_tidPacketList.push_back(pTidPacket);
    return maxSize - (iTotalSize + pTidPacket->m_iSize) ;
}

int ReplicableShmTid::getTidPacketNum()
{
    return m_tidPacketList.size();
}

int ReplicableShmTid::unpackObj ( char * pBuf, int bufLen )
{       return LS_OK;   }

int ReplicableShmTid::packObj ( int32_t id, char * pBuf, int bufLen )
{
    ReplPacker::detectEdian();


    TPointerList<TidPacket>::iterator iter;

    int32_t nPacket = m_tidPacketList.size();
    //pBuf->append(( const char* ) &size, sizeof ( uint32_t ));
    int32_t ret = sizeof(uint32_t ) ;
    
    char* ptemBuf = pBuf;
    char * pEndBuf = pBuf + bufLen;
    int iSavedlen = ReplPacker::pack ( ptemBuf, pEndBuf - ptemBuf, nPacket );
    ptemBuf += iSavedlen; //id
    
    for (iter = m_tidPacketList.begin(); iter != m_tidPacketList.end();
         ++iter)
    {
        iSavedlen = ReplPacker::pack ( ptemBuf, pEndBuf - ptemBuf, (*iter)->m_iSize);
        ptemBuf  += iSavedlen; //m_iSize

        iSavedlen = ReplPacker::packRawStr ( ptemBuf, pEndBuf - ptemBuf 
                    , ( const char* )( (const char*)(*iter) + sizeof(uint32_t) ) 
                    , sizeof(char) * ( (*iter)->m_iSize - sizeof(uint32_t) ) );
        ptemBuf  += iSavedlen;
        ret      += (*iter)->m_iSize;
    }      
    
    LS_DBG_M(  "packobj id=%d, npacket=%d, packet Size=%d\n",
		  id, nPacket, ret);
    return ret;
    
}

int ReplicableShmTid::packObj ( int32_t id, AutoBuf* pBuf )
{
    TPointerList<TidPacket>::iterator iter;

    int32_t nPacket = m_tidPacketList.size();
    pBuf->append(( const char* ) &nPacket, sizeof ( uint32_t ));
    
    int32_t ret = sizeof(uint32_t ) ;
    
    for (iter = m_tidPacketList.begin(); iter != m_tidPacketList.end();
         ++iter)
    {
        pBuf->append(( const char* ) &( (*iter)->m_iSize), sizeof ( uint32_t ));
        
        pBuf->append( (const char*)(( const char* ) (*iter) + sizeof(uint32_t)),   
                    sizeof ( uint8_t ) * ( (*iter)->m_iSize - sizeof(uint32_t) ) );
        ret += (*iter)->m_iSize;
    }    
    return ret;
}

