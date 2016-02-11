
#include "replshmtidcontainer.h"
#include "lcreplconf.h"
#include "replshmhelper.h"
#include "lcreplgroup.h"
#include "lcreplsender.h"
#include <log4cxx/logger.h>

ReplShmTidContainer::ReplShmTidContainer (int id, 
                                          ReplicableAllocator * pAllocator )
    : ReplContainer ( id, pAllocator )
{}

ReplShmTidContainer::~ReplShmTidContainer()
{}

ReplShmTidContainer& ReplShmTidContainer::operator= ( const ReplShmTidContainer& other )
{
    return *this;
}

bool ReplShmTidContainer::operator== ( const ReplShmTidContainer& other ) const
{
///TODO: return ...;
}

int ReplShmTidContainer::addAndUpdateData ( uint8_t * pData, int dataLen, int iUpdate ) 
{
    int idx = getId() - 1;
    LcNodeInfo* pLocalStatus = LcReplGroup::getInstance().getLocalNodeInfo();
    uint32_t role = pLocalStatus->getRole(idx);
    assert (R_SLAVE == role);

    ReplShmHelper &replShmHelper = ReplShmHelper::getInstance();
    int ret = replShmHelper.tidSetItems(idx, pData, dataLen);  
    if ( ret >= 0)
    {
        //pLocalStatus->setCurrTid(idx, replShmHelper.getLastShmTid(idx) );
        return LS_OK;
    }
    else
    {
        LS_ERROR(  "ReplShmTidContainer failed to apply replication data, tidSetItems ret=%d, idx=%d, dataLen=%d"
            , ret, idx, dataLen );
        return LS_FAIL;
    }
}

int ReplShmTidContainer::purgeExpired()
{
    return LS_OK;
}

int ReplShmTidContainer::verifyObj( char * pBuf, int bufLen )
{
    uint32_t nPacket = 0 ;
    ReplPacker::unpack ( pBuf, nPacket);
    LS_DBG_M(   "verifyObj nPacket=%d\n", nPacket);    
    if( nPacket == 0 ) 
        return LS_FAIL;
    
    uint32_t ipackSize;
    int iTotSize  = sizeof(uint32_t);
    
    char *pTempBuf = pBuf + sizeof(uint32_t);
    for(int i=0 ; i < nPacket ; ++i)
    {
        ReplPacker::unpack ( pTempBuf, ipackSize);
        iTotSize += ipackSize;
        pTempBuf += ipackSize;    
    }
    return iTotSize == bufLen ? nPacket : LS_FAIL;    
    
}
