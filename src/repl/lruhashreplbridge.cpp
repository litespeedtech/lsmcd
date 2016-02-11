/*
 * 
 */

#include "lruhashreplbridge.h"

#include <log4cxx/logger.h>
#include <repl/replsender.h>
#include <repl/replgroup.h>
#include <repl/replprgrsstrkr.h>
#include <shm/lsshmhash.h>
#include <util/datetime.h>
#include <util/stringtool.h>

LruHashReplBridge::LruHashReplBridge(LsShmHash *pHash, int contId)
    : LsShmHashObserver(pHash)
    , m_contId(contId)
{
}

LruHashReplBridge::~LruHashReplBridge()
{
}

int LruHashReplBridge::onNewEntry(const void *pKey, int keyLen, const void *pVal, 
                           int valLen, uint32_t lruTm)
{
    getReplSender()->sendLruHashData(m_contId, lruTm, (unsigned char *)pKey, keyLen, 
                           (unsigned char *)pVal, valLen);
    return LS_OK;
}


int LruHashReplBridge::onDelEntry(const void *pKey, int keyLen)
{
    return LS_OK;    
}

uint32_t LruHashReplBridge::getLruHashTmHash(time_t startTm, time_t endTm, AutoBuf &autoBuf) const
{
    if (!getHash())
        return 0;
    uint32_t count = 0;
    startTm--;
    getHash()->lock();
    while ( startTm <= endTm ) 
    {
        startTm = hashTillNextTmSlot(startTm, endTm, autoBuf, count);
    }
    getHash()->unlock();
    LS_DBG_L("LruHashReplBridge::getLruHashTmHash, read count %d, endTm=%d, nextTm=%d "
        , count, endTm, startTm);    
    return count;
}


int LruHashReplBridge::readShmKeyVal(int contId, ReplProgressTrker &replTmSlotLst, uint32_t availSize, AutoBuf &rAutoBuf) const
{
    if (!getHash())
        return 0;
    uint32_t currTm, endTm;

    replTmSlotLst.getBReplTmRange(contId, currTm, endTm); //getBReplSslSessTmSlot(currTm, endTm);
    //currTm--;
    getHash()->lock();
    while ( !replTmSlotLst.isReplDone(contId) && (uint32_t)rAutoBuf.size() < availSize )
    {
        currTm = readTillNextTmSlot(currTm, endTm + 1, rAutoBuf);
        replTmSlotLst.advBReplTmSlot(contId, currTm);
    }
    getHash()->unlock();
    LS_DBG_L("Bulk Replication LruHashReplBridge::readShmKeyVal2 currTm:%d, endTm:%d, contId:%d, read bytes:%d"
        , currTm, endTm, contId, rAutoBuf.size());
    return rAutoBuf.size();
}

//input: 99,99,100,100,100,100,100,103,103,105  
//if passing 99, the function reads 5 of 100, and return tm 100
time_t LruHashReplBridge::readTillNextTmSlot(time_t tm, time_t endTm, AutoBuf & rAutoBuf) const
{
    int count=0;
    LsShmHash::iterator iter;
    LsShmHash::iteroffset iterOff;
    time_t nextTm;
    iterOff = getHash()->nextTmLruIterOff(tm);
    if (iterOff.m_iOffset == 0 )
    {
        LS_DBG_L("LruHashReplBridge::readTillNextTmSlot retun iterOff=0, tm=%d", tm);
        return time(NULL) + 1;
    }
    iter        = getHash()->offset2iterator(iterOff);
    nextTm      = iter->getLruLasttime();
    if (nextTm > endTm)
        return nextTm;
    
    appendKeyVal((uint32_t)nextTm
        , (uint32_t)iter->getKeyLen()
        , (uint32_t)iter->getValLen()
        , (const char*)iter->getKey()
        , (const char*)iter->getVal()
        , rAutoBuf);
    count++;
    while (1)
    {
        iterOff = getHash()->nextLruIterOff(iterOff);
        if (iterOff.m_iOffset == 0 )
            break;
        iter = getHash()->offset2iterator(iterOff);
        if (nextTm != iter->getLruLasttime())
        {
            break;
        }
        appendKeyVal((uint32_t)nextTm
            , (uint32_t)iter->getKeyLen()
            , (uint32_t)iter->getValLen()
            , (const char*)iter->getKey()
            , (const char*)iter->getVal()
            , rAutoBuf);
        count++;

    }
    LS_DBG_L("LruHashReplBridge::readTillNextTmSlot, input tm=%d, read current tm=%d, count:%d, size=%d"
        , tm, nextTm, count, rAutoBuf.size());
    return nextTm;
}

//starting from the next of tm
time_t LruHashReplBridge::hashTillNextTmSlot(time_t tm, time_t endTm, AutoBuf & rAutoBuf, uint32_t &count) const
{
    //LS_DBG_L("LruHashReplBridge::hashTillNextTmSlot read data at time [%d, %d]", tm, endTm);
    time_t      nextTm;
    unsigned char buf[16];
    MD5_CTX     md5_ctx;
    LsShmHash::iterator         iter;
    LsShmHash::iteroffset       iterOff;
    iterOff = getHash()->nextTmLruIterOff(tm);
    if (iterOff.m_iOffset == 0 )
    {
        LS_DBG_L("LruHashReplBridge::hashTillNextTmSlot retun 0 byte at tm=%d, return time=%d", tm, time(NULL) + 1);
        return time(NULL) +1;
    }
    iter = getHash()->offset2iterator(iterOff);
    nextTm  = iter->getLruLasttime();
    if (nextTm > endTm)
        return nextTm;
    count++;
    MD5_Init(&md5_ctx);
    MD5_Update(&md5_ctx, (const char*)iter->getKey(), (uint32_t)iter->getKeyLen());
    while (1)
    {
        iterOff = getHash()->nextLruIterOff(iterOff);
        if( iterOff.m_iOffset == 0 )
            break;
        iter = getHash()->offset2iterator(iterOff);
        if (nextTm != iter->getLruLasttime())
        {
            break;
        }
        MD5_Update(&md5_ctx, (const char*)iter->getKey(), (uint32_t)iter->getKeyLen());
    }
    MD5_Final(buf, &md5_ctx);
    
    char achAsc[33];
    StringTool::hexEncode((const char *)buf, 16, achAsc);
    rAutoBuf.append((const char*)&nextTm, sizeof(uint32_t));
    rAutoBuf.append(achAsc, 32);
    //LS_DBG_L("LruHashReplBridge::hashTillNextTmSlot read achAsc=%s, at tm:%d, count:%d"
    //    , achAsc, nextTm, count );
    return nextTm;
}


void LruHashReplBridge::appendKeyVal(uint32_t lruTm, uint32_t uKeyLen, uint32_t uValLen, const char* pKey
    , const char* pVal, AutoBuf &rAutoBuf) const
{
    rAutoBuf.append((const char*)&lruTm, sizeof(uint32_t));
    rAutoBuf.append((const char*)&uKeyLen, sizeof(uint32_t));
    rAutoBuf.append((const char*)&uValLen, sizeof(uint32_t));
    rAutoBuf.append(pKey, uKeyLen * sizeof(uint8_t));
    rAutoBuf.append(pVal, uValLen * sizeof(uint8_t));
}

time_t LruHashReplBridge::getCurrHeadShmTm() const
{
    if (!getHash())
        return 0;    
    getHash()->lock();
    time_t tm = getElmLruTm(getHash()->getLruBottom());
    getHash()->unlock();
    return tm;
}

time_t LruHashReplBridge::getCurrTailShmTm() const
{
    if (!getHash())
        return 0;    
    getHash()->lock();
    time_t tm = getElmLruTm(getHash()->getLruTop());
    getHash()->unlock();
    return tm;
}

time_t LruHashReplBridge::getCurrLruNextTm(time_t tm) const
{
    LsShmHash::iteroffset iterOff;
    getHash()->lock();
    iterOff = getHash()->nextTmLruIterOff(tm);
    time_t nTm = getElmLruTm(iterOff);    
    getHash()->unlock();
    return nTm;
}

time_t LruHashReplBridge::getElmLruTm(LsShmHash::iteroffset offCurr) const
{
    if (offCurr.m_iOffset == 0)
        return 0;
    LsShmHash::iterator iter;
    iter = getHash()->offset2iterator(offCurr);
    return iter->getLruLasttime();
}


