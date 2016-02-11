
#include "replcontainer.h"
#include "lruhashreplbridge.h"
#include <log4cxx/logger.h>
#include "replprgrsstrkr.h"
ReplContainer::ReplContainer ( const ReplContainer& other )
{
}

ReplContainer::~ReplContainer()
{
    if ( m_pAllocator )
        delete m_pAllocator;
}

uint32_t ReplContainer::getHeadLruHashTm() const
{
    if (m_pHashBridge == NULL)
        return 0;
    if (isReady())
        return m_pHashBridge->getCurrHeadShmTm();
    return 0;
}

uint32_t ReplContainer::getTailLruHashTm() const
{
    if (isReady())
        return m_pHashBridge->getCurrTailShmTm();
    return 0;
}

uint32_t ReplContainer::getCurrLruNextTm(uint32_t tm) const
{
    if (m_pHashBridge == NULL)
        return 0;    
    if (isReady())
        return m_pHashBridge->getCurrLruNextTm(tm);
    return 0;
}

int ReplContainer::getLruHashKeyHash(uint32_t startTm, uint32_t endTm, AutoBuf &rAutoBuf) const
{
    if (isReady())
        return m_pHashBridge->getLruHashTmHash(startTm, endTm, rAutoBuf);
    return 0;
}

int  ReplContainer::readShmKeyVal(ReplProgressTrker &replTmSlotLst, int availSize, AutoBuf &rAutoBuf) const
{
    return m_pHashBridge->readShmKeyVal(getId(), replTmSlotLst, availSize, rAutoBuf);
}



