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
    if (getReplSender())
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
    uint32_t tmCnt = 0;
    startTm--;
    getHash()->lock();
    while ( startTm <= endTm ) 
    {
        startTm = hashTillNextTmSlot(startTm, endTm, autoBuf);
        if (startTm <= endTm)
            tmCnt++;
    }
    getHash()->unlock();
    //LS_DBG_L("LruHashReplBridge::getLruHashTmHash, read tmCnt=%d, endTm=%d, nextTm=%d "
    //    , tmCnt, endTm, startTm);    
    return tmCnt;
}


int LruHashReplBridge::readShmKeyVal(int contId, ReplProgressTrker &replTmSlotLst, uint32_t availSize, AutoBuf &rAutoBuf) const
{
    if (!getHash())
        return 0;
    uint32_t currTm, endTm;

    replTmSlotLst.getBReplTmRange(contId, currTm, endTm);
    //currTm--;
    getHash()->lock();
    while ( !replTmSlotLst.isReplDone(contId) && (uint32_t)rAutoBuf.size() < availSize )
    {
        currTm = readTillNextTmSlot(currTm, endTm + 1, rAutoBuf);
        replTmSlotLst.advBReplTmSlot(contId, currTm);
    }
    getHash()->unlock();
    LS_DBG_L("Bulk Replication LruHashReplBridge::readShmKeyVal currTm:%d, endTm:%d, contId:%d, read bytes:%d"
        , currTm, endTm, contId, rAutoBuf.size());
    return rAutoBuf.size();
}

//input: 99,99,100,100,100,100,100,103,103,105  
//if passing 99, the function reads 5 of 100, and return tm 100
time_t LruHashReplBridge::for_each_tmslot(time_t tm, time_t endTm, for_each_fn2 fn, void* pUData1) const
{
    int cnt = 0;
    LsShmHash::iterator iter;
    LsShmHash::iteroffset iterOff;
    time_t nextTm;
    iterOff = getHash()->nextTmLruIterOff(tm);
    if (iterOff.m_iOffset == 0 )
    {
        nextTm = time(NULL) > endTm ? (time(NULL) + 1) : (endTm + 1);
        LS_DBG_L("LruHashReplBridge::for_each_tmslot retun 0 byte at tm=%d, return time=%d", tm, nextTm);
        return nextTm;
    }
    iter        = getHash()->offset2iterator(iterOff);
    nextTm      = iter->getLruLasttime();
    if (nextTm > endTm)
        return nextTm;

    fn(iter, &nextTm, pUData1);
    cnt++;
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
        fn(iter, &nextTm, pUData1);
        cnt++;
    }
    //LS_DBG_L("LruHashReplBridge::for_each_tmslot, read entry Count:%d, input time from %d to %d"
    //    , cnt, tm, nextTm);
    return nextTm;
}

int readTillNextTmSlotFn(void * cbData, void* pUData1, void* pUData2)
{
    LsShmHash::iterator iter    = (LsShmHash::iterator)cbData;
    time_t *pLruTm              = (time_t *)pUData1;
    AutoBuf *pAutoBuf           = (AutoBuf*)pUData2;
    
    uint32_t lruTm              = *pLruTm;
    uint32_t uKeyLen            = iter->getKeyLen();
    uint32_t uValLen            = iter->getValLen();
    
    pAutoBuf->append((const char*)&lruTm, sizeof(uint32_t));
    pAutoBuf->append((const char*)&uKeyLen, sizeof(uint32_t));
    pAutoBuf->append((const char*)&uValLen, sizeof(uint32_t));
    pAutoBuf->append((const char*)iter->getKey(), uKeyLen * sizeof(uint8_t));
    pAutoBuf->append((const char*)iter->getVal(), uValLen * sizeof(uint8_t));
    return LS_OK;
}

inline time_t LruHashReplBridge::readTillNextTmSlot(time_t tm, time_t endTm, AutoBuf & rAutoBuf) const
{
    return for_each_tmslot(tm, endTm, readTillNextTmSlotFn, &rAutoBuf);
}

//starting from the next of tm
int hashTillNextTmSlotFn(void * cbData, void *pUData1, void *pUData2)
{
    LsShmHash::iterator iter    = (LsShmHash::iterator)cbData;
    MD5_CTX *pCtx               = (MD5_CTX*)pUData2;

    MD5_Update(pCtx, (const char*)iter->getKey(), (uint32_t)iter->getKeyLen());
    return LS_OK;
}

time_t LruHashReplBridge::hashTillNextTmSlot(time_t tm, time_t endTm, AutoBuf & rAutoBuf) const
{
    MD5_CTX     md5_ctx;
    MD5_Init(&md5_ctx);
    time_t nextTm = for_each_tmslot(tm, endTm, hashTillNextTmSlotFn,&md5_ctx);
    
    if (nextTm <= endTm)
    {
        unsigned char buf[16];
        char achAsc[33];

        MD5_Final(buf, &md5_ctx);
        StringTool::hexEncode((const char *)buf, 16, achAsc);
        rAutoBuf.append((const char*)&nextTm, sizeof(uint32_t));
        rAutoBuf.append(achAsc, 32);
    }
    return nextTm;
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


