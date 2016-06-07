#ifndef LRUHASHREPLBRIDGE_H
#define LRUHASHREPLBRIDGE_H

#include <lsdef.h>
#include <shm/lsshmhashobserver.h>
#include <shm/lsshmtypes.h>
#include <openssl/md5.h>
#include <time.h>
#include "repldef.h"
class AutoBuf;
class ReplProgressTrker;

class LruHashReplBridge : public LsShmHashObserver
{
public:
    explicit LruHashReplBridge(LsShmHash * pHash, int contId);
    ~LruHashReplBridge();
    
    virtual int onNewEntry(const void *pKey, int keyLen, const void *pVal, 
                           int valLen, uint32_t lruTm);
    virtual int onDelEntry(const void *pKey, int keyLen);    

    uint32_t getLruHashTmHash(time_t startTm, time_t endTm, 
                            AutoBuf &autoBuf) const;

    int readShmKeyVal(int contId, ReplProgressTrker &replTmSlotLst, uint32_t availSize, 
                      AutoBuf &rAutoBuf) const;

    time_t readTillNextTmSlot(time_t tm, time_t endTm, 
                              AutoBuf & rAutoBuf) const;

    time_t hashTillNextTmSlot(time_t tm, time_t endTm, 
                              AutoBuf &rAutoBuf) const;

    time_t getCurrHeadShmTm() const;
    time_t getCurrTailShmTm() const;
    time_t getCurrLruNextTm(time_t tm) const;
    time_t getElmLruTm(LsShmHIterOff offCurr) const;
    int getContId() const       {       return m_contId;        }
protected:
    time_t for_each_tmslot(time_t tm, time_t endTm, for_each_fn2 fn, void* pUData1) const;    
private:
    int m_contId;
    LS_NO_COPY_ASSIGN(LruHashReplBridge);
};

#endif // LRUHASHREPLBRIDGE_H
