/*****************************************************************************
*    Open LiteSpeed is an open source HTTP server.                           *
*    Copyright (C) 2013 - 2015  LiteSpeed Technologies, Inc.                 *
*                                                                            *
*    This program is free software: you can redistribute it and/or modify    *
*    it under the terms of the GNU General Public License as published by    *
*    the Free Software Foundation, either version 3 of the License, or       *
*    (at your option) any later version.                                     *
*                                                                            *
*    This program is distributed in the hope that it will be useful,         *
*    but WITHOUT ANY WARRANTY; without even the implied warranty of          *
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the            *
*    GNU General Public License for more details.                            *
*                                                                            *
*    You should have received a copy of the GNU General Public License       *
*    along with this program. If not, see http://www.gnu.org/licenses/.      *
*****************************************************************************/
#ifndef LSSHMHASH_H
#define LSSHMHASH_H

#ifdef LSSHM_DEBUG_ENABLE
class debugBase;
#endif

#include <lsdef.h>
#include <lsr/ls_str.h>
#include <shm/lsshm.h>
#include <shm/lsshmpool.h>

#include <log4cxx/logger.h>

#define LSSHM_FLAG_NONE         0
#define LSSHM_FLAG_LRU          (1<<0)
#define LSSHM_FLAG_TID          (1<<1)    // `transaction' id
#define LSSHM_FLAG_TID_SLAVE    (1<<2)    // do *not* generate new tid, nor notify

/**
 * @file
 *  HASH element
 *
 *  +--------------------
 *  | Element info
 *  | hkey
 *  +--------------------  <--- start of variable data
 *  | key len
 *  | key
 *  +--------------------
 *  | LRU info (optional)
 *  +--------------------  <--- value offset
 *  | value len
 *  | value
 *  +--------------------
 *
 *  It is reasonable to support max 32k key string.
 *  Otherwise we should change to uint32_t
 */


typedef uint32_t     LsShmHKey;
typedef int32_t      LsShmHElemLen_t;
typedef uint32_t     LsShmHElemOffs_t;
typedef struct lsshmobsiter_s LsShmObsIter_t;
class LsShmObserver;
typedef struct lsShm_hElem_s LsShmHElem;



extern int s_Reported_Corruption;

typedef struct
{
    LsShmHIterOff        x_iLinkNext;     // offset to next in linked list
    LsShmHIterOff        x_iLinkPrev;     // offset to prev in linked list
    time_t               x_lasttime;      // last update time (sec)
} LsShmHElemLink;

typedef struct ls_vardata_s
{
    int32_t         x_size;
    uint8_t         x_data[0];
} ls_vardata_t;

typedef struct
{
    LsShmXSize_t    m_iHashInUse;   // shm allocated to hash (bytes)
} LsShmHTableStat;

typedef struct ls_shmhtable_s LsShmHTable;


class LsShmHashLruAddon
{

public:
    LsShmHashLruAddon() {}
    virtual ~LsShmHashLruAddon() {}

    virtual int setLruData(LsShmOffset_t offVal, const uint8_t *pVal, int valLen);

    virtual int getLruData(LsShmOffset_t offVal, LsShmOffset_t *pData, int cnt);

    virtual int getLruDataPtrs(LsShmOffset_t offVal, int (*func)(void *pData));

    virtual int clrdata(uint8_t *pValue);
    virtual int chkdata(uint8_t *pValue);

};


class AutoBuf;
class LsShmTidMgr;
class LsShmHash : public ls_shmhash_s
{
public:
    typedef LsShmHElem *iterator;
    typedef const LsShmHElem *const_iterator;
    typedef LsShmHIterOff iteroffset;

    typedef int (*for_each_fn)(iteroffset iterOff);
    typedef int (*for_each_fn2)(iteroffset iterOff, void *pUData);

    typedef int (*TrimCb)(iterator iter, void *arg);
    
    static LsShmHKey hashString(const void *__s, size_t len);
    static int compString(const void *pVal1, const void *pVal2, size_t len);

    static LsShmHKey hash32id(const void *__s, size_t len);
    static LsShmHKey hashBuf(const void *__s, size_t len);
    static LsShmHKey hashXXH32(const void *__s, size_t len);

    static LsShmHKey iHashString(const void *__s, size_t len);
    static int iCompString(const void *pVal1, const void *pVal2, size_t len);

    static int cmpIpv6(const void *pVal1, const void *pVal2, size_t len);
    static LsShmHKey hfIpv6(const void *pKey, size_t len);

    LsShmStatus_t status()
    { return m_status; }

public:
    LsShmHash(LsShmPool *pool,
              const char *name, LsShmHasher_fn hf, LsShmValComp_fn vc, int flags);
    virtual ~LsShmHash();

    int isOffsetValid(LsShmOffset_t offset)
    {   return m_pPool->getShm()->isOffsetValid(offset); }
    
    static LsShmHash *open(
        const char *pShmName, const char *pHashName, int init_size, int flags);

    static LsShmOffset_t allocHTable(LsShmPool * pPool, int init_size,
                                     int iMode, int iFlags,
                                     LsShmOffset_t lockOffset);

    //int init(size_t init_size);
    int init(LsShmOffset_t offset);

    static LsShmSize_t roundUp(LsShmSize_t sz);

    static int round4(int x)
    {   return (x + 0x3) & ~0x3; }

    static int sz2TableSz(LsShmSize_t sz)
    {   return (sz * sizeof(LsShmHIterOff)); }


    static LsShmHash *checkHTable(GHash::iterator itor, LsShmPool *pool,
                                  const char *name, LsShmHasher_fn hf, LsShmValComp_fn vc);

    static int chkHashTable(LsShm *pShm, LsShmReg *pReg, int *pMode, int *pFlags);


    ls_attr_inline LsShmPool *getPool() const
    {   return m_pPool;     }

    ls_attr_inline LsShmOffset_t ptr2offset(const void *ptr) const
    {   
        if (m_pPool)
            return m_pPool->ptr2offset(ptr); 
        return 0;
    }

    ls_attr_inline void *offset2ptr(LsShmOffset_t offset) const
    {   
        if (m_pPool)
            return m_pPool->offset2ptr(offset); 
        return NULL;
    }

    void        rebuild() const;
    
    iterator offset2iterator(iteroffset offset) const;

    void *offset2iteratorData(iteroffset offset) const;

    LsShmOffset_t iter2DataOffset(iteroffset offset) const;
    
    // For now, assume only one observer.
//     void *getObsData(LsShmHElem *pElem, LsShmObserver *pObserver) const;
    void *getObsData(LsShmHElem *pElem) const;

    LsShmOffset_t alloc2(LsShmSize_t size, int &remapped);

    void release2(LsShmOffset_t offset, LsShmSize_t size);

    LsShmOffset_t getHTableStatOffset() const;

    LsShmOffset_t getHTableReservedOffset() const;

    LsShmXSize_t getHashDataSize() const
    {
        if ((LsShmHTableStat *)offset2ptr(getHTableStatOffset()))
            return ((LsShmHTableStat *)offset2ptr(getHTableStatOffset()))->m_iHashInUse;
        return 0;
    }

    void *getIterDataPtr(iterator iter) const;

    const char *name() const
    {   return obj.m_pName; }

    uint16_t getExtraTotal()
    {   return m_dataExtraSpace + m_iterExtraSpace; }

    ls_attr_inline LsShmTidMgr *getTidMgr() const
    {   return m_pTidMgr;  }

    ls_attr_inline void setTidSlave()
    {   m_iFlags |= LSSHM_FLAG_TID_SLAVE;    }

    ls_attr_inline void setTidMaster()
    {   m_iFlags &= ~LSSHM_FLAG_TID_SLAVE;   }

    ls_attr_inline bool isTidMaster() const
    {   return ((m_iFlags & LSSHM_FLAG_TID_SLAVE) == 0); }

    void close();

    void clear();

    static ls_strpair_t *setParms(ls_strpair_t *pParms,
                            const void *pKey, int keyLen, const void *pValue, int valueLen)
    {
        ls_str_set(&pParms->key, (char *)pKey, keyLen);
        ls_str_set(&pParms->val, (char *)pValue, valueLen);
        return pParms;
    }

    int remove(const void *pKey, int keyLen)
    {
        iteroffset iterOff;
        ls_strpair_t parms;
        ls_str_set(&parms.key, (char *)pKey, keyLen);
        
        autoLockChkRehash();
        iterOff = (*m_find)(this, &parms);
        if (iterOff.m_iOffset != 0)
        {
            eraseIteratorHelper(iterOff);
        }
        autoUnlock();
        return iterOff.m_iOffset != 0;
    }

    LsShmOffset_t find(const void *pKey, int keyLen, int *valLen);

    LsShmOffset_t get(const void *pKey, int keyLen, int *valLen, int *pFlag);

    LsShmOffset_t insert(
        const void *pKey, int keyLen, const void *pValue, int valueLen)
    {
        ls_strpair_t parms;
        iteroffset iterOff = insertIterator(
                                 setParms(&parms, pKey, keyLen, pValue, valueLen));
        if (!iterOff.m_iOffset || offset2iteratorData(iterOff))
            return (iterOff.m_iOffset == 0) ?
                    0 : ptr2offset(offset2iteratorData(iterOff));
        return 0;
    }
    
    LsShmOffset_t set(
        const void *pKey, int keyLen, const void *pValue, int valueLen)
    {
        ls_strpair_t parms;
        iteroffset iterOff = setIterator(
                                 setParms(&parms, pKey, keyLen, pValue, valueLen));
        if (!iterOff.m_iOffset || offset2iteratorData(iterOff))
            return (iterOff.m_iOffset == 0) ?
                    0 : ptr2offset(offset2iteratorData(iterOff));
        return 0;
    }

    LsShmOffset_t update(
        const void *pKey, int keyLen, const void *pValue, int valueLen)
    {
        ls_strpair_t parms;
        iteroffset iterOff = updateIterator(
                                 setParms(&parms, pKey, keyLen, pValue, valueLen));
        if (!iterOff.m_iOffset || offset2iteratorData(iterOff))
            return (iterOff.m_iOffset == 0) ?
                    0 : ptr2offset(offset2iteratorData(iterOff));
        return 0;
    }

    int touchLru(iteroffset iterOff);

    int linkSetTopTime(iteroffset offset, time_t lasttime);

    int linkMvTopTime(iteroffset offset, time_t lasttime);

    void eraseIterator(iteroffset iterOff)
    {
        autoLockChkRehash();
        eraseIteratorHelper(iterOff);
        autoUnlock();
    }

    iteroffset  insertCopy(LsShmHKey key, ls_strpair_t *pParms)
    {
        iteroffset off;
        autoLockChkRehash();
        off = insertCopy2(key, pParms);
        autoUnlock();
        return off;
    }

    //
    //  Note - iterators should not be saved.
    //         use ptr2offset(iterator) to save the offset
    //
    iteroffset findIterator(ls_strpair_t *pParms)
    {
        autoLockChkRehash();
        iteroffset iterOff = (*m_find)(this, pParms);
        autoUnlock();
        return iterOff;
    }

    iteroffset getIterator(ls_strpair_t *pParms, int *pFlag)
    {
        autoLockChkRehash();
        iteroffset iterOff = (*m_get)(this, pParms, pFlag);
        autoUnlock();
        return iterOff;
    }

    iteroffset insertIterator(ls_strpair_t *pParms)
    {
        autoLockChkRehash();
        iteroffset iterOff = (*m_insert)(this, pParms);
        autoUnlock();
        return iterOff;
    }

    iteroffset setIterator(ls_strpair_t *pParms)
    {
        autoLockChkRehash();
        iteroffset iterOff = (*m_set)(this, pParms);
        autoUnlock();
        return iterOff;
    }

    iteroffset updateIterator(ls_strpair_t *pParms)
    {
        autoLockChkRehash();
        iteroffset iterOff = (*m_update)(this, pParms);
        autoUnlock();
        return iterOff;
    }

    iteroffset findIteratorWithKey(LsShmHKey key, ls_strpair_t *pParms)
    {
        autoLockChkRehash();
        iteroffset iterOff = find2(key, pParms);
        autoUnlock();
        return iterOff;
    }

    iteroffset getIteratorWithKey(LsShmHKey key, ls_strpair_t *pParms,
                                  int *pFlag)
    {
        autoLockChkRehash();
        iteroffset iterOff = find2(key, pParms);
        iterOff = doGet(iterOff, key, pParms, pFlag);
        autoUnlock();
        return iterOff;
    }

    iteroffset insertIteratorWithKey(LsShmHKey key, ls_strpair_t *pParms)
    {
        autoLockChkRehash();
        iteroffset iterOff = find2(key, pParms);
        iterOff = doInsert(iterOff, key, pParms);
        autoUnlock();
        return iterOff;
    }

    iteroffset setIteratorWithKey(LsShmHKey key, ls_strpair_t *pParms)
    {
        autoLockChkRehash();
        iteroffset iterOff = find2(key, pParms);
        iterOff = doSet(iterOff, key, pParms);
        autoUnlock();
        return iterOff;
    }

    iteroffset updateIteratorWithKey(LsShmHKey key, ls_strpair_t *pParms)
    {
        autoLockChkRehash();
        iteroffset iterOff = find2(key, pParms);
        iterOff = doUpdate(iterOff, key, pParms);
        autoUnlock();
        return iterOff;
    }

    // NOTICE: the following do* methods assume that the lock is locked.
    iteroffset doGet(iteroffset iterOff, LsShmHKey key, ls_strpair_t *pParms,
                     int *pFlag);
    iteroffset doInsert(iteroffset iterOff, LsShmHKey key, ls_strpair_t *pParms);
    iteroffset doSet(iteroffset iterOff, LsShmHKey key, ls_strpair_t *pParms);

    iteroffset doUpdate(iteroffset iterOff, LsShmHKey key, ls_strpair_t *pParms);

    iteroffset nextLruIterOff(iteroffset iterOff);

    iteroffset prevLruIterOff(iteroffset iterOff);

    iteroffset nextTmLruIterOff(time_t tmCutoff);
    iteroffset prevTmLruIterOff(time_t tmCutoff);

    iterator nextLruIterator(iterator iter);

    iterator prevLruIterator(iterator iter);

    LsShmHasher_fn getHashFn() const   {   return m_hf;    }
    LsShmValComp_fn getValComp() const {   return m_vc;    }

    void setFullFactor(int f);
    void setGrowFactor(int f);

    bool empty() const;
    LsShmSize_t size() const;
    LsShmSize_t capacity() const;

    void incrTableSize();
    void decrTableSize();


    iteroffset begin();
    iteroffset end()                   {   return m_end;   }
    iteroffset next(iteroffset iterOff);
    int for_each(iteroffset beg, iteroffset end, for_each_fn fun);
    int for_each2(iteroffset beg, iteroffset end, for_each_fn2 fun,
                  void *pUData);

    void destroy();

    //  @brief stat
    //  @brief gether statistic on HashTable
    int stat(LsHashStat *pHashStat, for_each_fn2 fun, void *pData);

    // LRU stuff
    LsHashLruInfo *getLru();

    iteroffset getLruTop();    
    iteroffset getLruBottom();
    int32_t getLruTotal();
    int32_t getLruTrimmed(); 

    int trim(time_t tmCutoff, LsShmHash::TrimCb cb, void *arg);
    int trimsize(int need, LsShmHash::TrimCb cb, void *arg);
    int trimByCb(int maxCnt, LsShmHash::TrimCb func, void *arg);
    
    int check();

    void enableAutoLock()
    {   m_iAutoLock = 1; };

    void disableAutoLock()
    {   m_iAutoLock = 0; };

    int isAutoLock()
    {   return m_iAutoLock;   }

    int lock()
    {
        if (m_iAutoLock != 0)
            return 0;
        return getPool()->getShm()->lockRemap(m_pShmLock);
    }

    int unlock()
    {   
        if (m_iAutoLock)
            return 0;
        return LsShm::unlock(m_pShmLock); 
    }

    void lockChkRehash();

    int         rehash(bool force);

    int getRef()     { return m_iRef; }
    int upRef()      { return ++m_iRef; }
    int downRef()    { return --m_iRef; }
    LsShmHash::iteroffset iterGrowValue(iteroffset iterOff,
                                        int size_to_grow, int front);

protected:
    typedef iteroffset(*hash_find)(LsShmHash *pThis, ls_strpair_t *pParms);
    typedef iteroffset(*hash_get)(LsShmHash *pThis, ls_strpair_t *pParms,
                                  int *pFlag);
    typedef iteroffset(*hash_insert)(LsShmHash *pThis, ls_strpair_t *pParms);
    typedef iteroffset(*hash_set)(LsShmHash *pThis, ls_strpair_t *pParms);
    typedef iteroffset(*hash_update)(LsShmHash *pThis, ls_strpair_t *pParms);

    static inline uint32_t getIndex(uint32_t k, uint32_t n)
    {   return k % n ; }

    ls_attr_inline LsShmHTable *getHTable() const
    {   
        if (m_pPool && m_pPool->offset2ptr(m_iOffset))
            if ( x_pTable == (LsShmHTable *)m_pPool->offset2ptr(m_iOffset))
                return x_pTable;   
        return NULL;
    }

    LsShmSize_t fullFactor() const;
    LsShmSize_t growFactor() const;

    ls_attr_inline LsShmHIterOff *getHIdx() const;

    ls_attr_inline uint8_t *getBitMap() const;
    int getBitMapEnt(uint32_t indx);
    void setBitMapEnt(uint32_t indx);
    void clrBitMapEnt(uint32_t indx);

    iteroffset  find2(LsShmHKey key, ls_strpair_t *pParms);
    iteroffset  insert2(LsShmHKey key, ls_strpair_t *pParms);
    iteroffset  insertCopy2(LsShmHKey key, ls_strpair_t *pParms);

    static iteroffset findNum(LsShmHash *pThis, ls_strpair_t *pParms);
    static iteroffset getNum(LsShmHash *pThis, ls_strpair_t *pParms,
                             int *pFlag);
    static iteroffset insertNum(LsShmHash *pThis, ls_strpair_t *pParms);
    static iteroffset setNum(LsShmHash *pThis, ls_strpair_t *pParms);
    static iteroffset updateNum(LsShmHash *pThis, ls_strpair_t *pParms);

    static iteroffset findPtr(LsShmHash *pThis, ls_strpair_t *pParms);
    static iteroffset getPtr(LsShmHash *pThis, ls_strpair_t *pParms,
                             int *pFlag);
    static iteroffset insertPtr(LsShmHash *pThis, ls_strpair_t *pParms);
    static iteroffset setPtr(LsShmHash *pThis, ls_strpair_t *pParms);
    static iteroffset updatePtr(LsShmHash *pThis, ls_strpair_t *pParms);

    iteroffset allocIter(int key_len, int val_len);

    //
    //  @brief eraseIterator_helper
    //  @brief  should only be called after SHM-HASH-LOCK has been acquired.
    //
    void eraseIteratorHelper(iteroffset iterOff);

#ifdef notdef
    static iteroffset doExpand(LsShmHash *pThis,
                               iteroffset iterOff, LsShmHKey key, ls_strpair_t *pParms, uint16_t flags);
#endif

    void setIterData(iterator iter, const void *pValue);

    void setIterKey(iterator iter, const void *pKey);

    static int release_hash_elem(iteroffset iterOff, void *pUData);

    int autoLock()
    {
        if (m_iAutoLock == 0)
        {
            assert(m_pPool->getShm()->isLocked(m_pShmLock));
            return 0;
        }
        return getPool()->getShm()->lockRemap(m_pShmLock);
    }

    int autoUnlock()
    {   
        assert(m_pPool->getShm()->isLocked(m_pShmLock));
        if (!m_iAutoLock)
            return 0;
        return LsShm::unlock(m_pShmLock);
    }

    void autoLockChkRehash();

    // stat helper
    int statIdx(iteroffset iterOff, for_each_fn2 fun, void *pUData);

    int setupLock()
    {   return m_iAutoLock && ls_shmlock_setup(m_pShmLock); }

    // auxiliary double linked list of hash elements
    int  set_linkNext(iteroffset offThis, iteroffset offNext);

    int  set_linkPrev(iteroffset offThis, iteroffset offPrev);

    void linkHElem(LsShmHElem *pElem, iteroffset offElem);

    void unlinkHElem(LsShmHElem *pElem);

    void linkSetTop(LsShmHElem *pElem, iteroffset offElem);
    int8_t addExtraSpace(uint8_t iterExtra, uint8_t dataExtra)
    {
        m_dataExtraSpace += dataExtra;
        m_iterExtraSpace += iterExtra;
        return m_iterExtraSpace;
    }

protected:
    uint32_t            m_iMagic;
    LsShmOffset_t       m_iOffset;
    LsShmPool          *m_pPool;
    LsShmHTable        *x_pTable;
    
    LsShmHasher_fn      m_hf;
    LsShmValComp_fn     m_vc;
    hash_insert         m_insert;
    hash_update         m_update;
    hash_set            m_set;
    hash_find           m_find;
    hash_get            m_get;

    uint8_t             m_iterExtraSpace;
    uint8_t             m_dataExtraSpace;
    int8_t              m_iAutoLock;
    int8_t              m_iMode;        // mode 0=Num, 1=Ptr
    uint8_t             m_iFlags;       // lru=0x01, tid=0x02, tid_slave=0x04
    ls_shmlock_t       *m_pShmLock;     // local lock for Hash
    LsShmStatus_t       m_status;
    LsShmHashLruAddon  *m_pLruAddon;
    LsShmObsIter_t     *m_pObservers;
    LsShmHIterOff       m_end;
    // house keeping
    int m_iRef;


private:
    // disable the bad boys!
    LsShmHash(const LsShmHash &other);
    LsShmHash &operator=(const LsShmHash &other);

    void releaseHTableShm();

    LsShmTidMgr        *m_pTidMgr;

#ifdef LSSHM_DEBUG_ENABLE
    // for debug purpose - should debug this later
    friend class debugBase;
#endif
};


typedef struct lsShm_hElem_s
{
    LsShmHElemLen_t      x_iLen;          // element size
    LsShmHElemOffs_t     x_iValOff;
    LsShmHIterOff        x_iNext;         // offset to next in element list
    LsShmHKey            x_hkey;          // the key itself
    uint32_t             x_aData[0];

    /* I'm going to assume these functions don't need validation as they get 
     * validated in advance to being called */
    uint8_t         *getKey() const
    { return ((ls_vardata_t *)x_aData)->x_data; }
    int32_t          getKeyLen() const
    { return ((ls_vardata_t *)x_aData)->x_size; }
    uint8_t         *getVal() const
    { return ((ls_vardata_t *)((uint8_t*)x_aData + x_iValOff))->x_data; }
    int32_t          getValLen() const
    { return ((ls_vardata_t *)((uint8_t*)x_aData + x_iValOff))->x_size; }
    int32_t          getValStartOff() const
    { return getVal() - (uint8_t *)this;    }
    void             setKeyLen(int32_t len)
    { ((ls_vardata_t *)x_aData)->x_size = len; }
    void             setValLen(int32_t len)
    { ((ls_vardata_t *)((uint8_t*)x_aData + x_iValOff))->x_size = len; }

    static int round4(int x)
    {   return (x + 0x3) & ~0x3; }
    
    int              validate(LsShmHash *lsShmHash, bool lru) const
    {
        /* The original calculation for x_iValOff (valOff) and x_iLen, missing 
         * the actual lengths of the key and value as they're not available 
         * until we're sure the buffer is intact.  This calculation makes a try
         * at determining if the buffer is correct (to avoid constant crashes). */
        LsShmHElemLen_t  iLen = x_iLen;         // Put this into locals for the debugger
        LsShmHElemOffs_t iValOff = x_iValOff;   // Put this into locals for the debugger
        LsShmHElemOffs_t valOff = sizeof(ls_vardata_t) //+ round4(keyLen)
                                  + (lru ? sizeof(LsShmHElemLink) : 0);
        int              valLen = /*realValLen +*/ (lru ? sizeof(LsShmHElemLink) : 0);
        LsShmHElemLen_t  len = sizeof(struct lsShm_hElem_s) /*+ valueOff*/
                               + sizeof(ls_vardata_t) + round4(valLen);
        if (!lsShmHash->getPool() ||
            !lsShmHash->isOffsetValid(iValOff + lsShmHash->ptr2offset(x_aData)) ||
            (x_iLen != 0 && x_iValOff > (uint32_t)x_iLen) || 
            x_iValOff <= valOff || 
            (x_iLen != 0 && x_iLen <= len) || 
            iLen > 100000000) // A large number
        {
            if (!s_Reported_Corruption)
            {
                s_Reported_Corruption = 1;
                LS_DBG_M("[PID: %d] Shared memory files should be deleted: "
                         "x_iValOff: %d (%d), x_iLen: %d (%d)\n", 
                         getpid(), x_iValOff, valOff, x_iLen, len);
            }
            return -1;
        }
        return 0;
    }
    LsShmHElemLink  *getLruLinkPtr(LsShmHash *lsShmHash) const
    {   
        /* The original calculation for x_iValOff (valOff) and x_iLen, missing 
         * the actual lengths of the key and value as they're not available 
         * until we're sure the buffer is intact.  This calculation makes a try
         * at determining if the buffer is correct (to avoid constant crashes). */
        LsShmHElemLen_t      iLen = x_iLen;         // Put this into locals for the debugger
        LsShmHElemOffs_t     iValOff = x_iValOff;   // Put this into locals for the debugger
        if (validate(lsShmHash, true))
            return NULL;
        return ((LsShmHElemLink *)((uint8_t*)x_aData + x_iValOff) - 1); 
    }
    LsShmHIterOff    getLruLinkNext(LsShmHash *lsShmHash) const
    { 
        LsShmHElemLink *link = getLruLinkPtr(lsShmHash);
        if (link)
        {
            LsShmHElemLen_t      iLen = x_iLen;         // Put this into locals for the debugger
            LsShmHElemOffs_t     iValOff = x_iValOff;   // Put this into locals for the debugger
            return link->x_iLinkNext; 
        }
        LsShmHIterOff offZero = { 0 };
        return offZero;
    }
    LsShmHIterOff    getLruLinkPrev(LsShmHash *lsShmHash) const
    { 
        LsShmHElemLink *link = getLruLinkPtr(lsShmHash);
        if (link)
        {
            LsShmHElemLen_t      iLen = x_iLen;         // Put this into locals for the debugger
            LsShmHElemOffs_t     iValOff = x_iValOff;   // Put this into locals for the debugger
            return link->x_iLinkPrev; 
        }
        LsShmHIterOff offZero = { 0 };
        return offZero;
    }
    time_t           getLruLasttime(LsShmHash *lsShmHash) const
    { 
        LsShmHElemLink *link = getLruLinkPtr(lsShmHash);
        if (link)
        {
            LsShmHElemLen_t      iLen = x_iLen;         // Put this into locals for the debugger
            LsShmHElemOffs_t     iValOff = x_iValOff;   // Put this into locals for the debugger
            return link->x_lasttime; 
        }
        return 0;
    }
    int              setLruLinkNext(LsShmHash *lsShmHash, LsShmHIterOff off)
    { 
        LsShmHElemLink *link = getLruLinkPtr(lsShmHash);
        if (link)
        {
            LsShmHElemLen_t      iLen = x_iLen;         // Put this into locals for the debugger
            LsShmHElemOffs_t     iValOff = x_iValOff;   // Put this into locals for the debugger
            link->x_iLinkNext = off; 
            return 0;
        }
        return -1;
    }
    int              setLruLinkPrev(LsShmHash *lsShmHash, LsShmHIterOff off)
    { 
        LsShmHElemLink *link = getLruLinkPtr(lsShmHash);
        if (link)
        {
            LsShmHElemLen_t      iLen = x_iLen;         // Put this into locals for the debugger
            LsShmHElemOffs_t     iValOff = x_iValOff;   // Put this into locals for the debugger
            link->x_iLinkPrev = off; 
            return 0;
        }
        return -1;
    }

    void            *getExtraPtr(LsShmOffset_t off)
    { return ((uint8_t *)x_aData + x_iValOff - off); }

    const uint8_t   *first() const   { return getKey(); }
    uint8_t         *second() const  { return getVal(); }

    // real value len
    LsShmSize_t      realValLen() const
    {
        return (x_iLen - sizeof(struct lsShm_hElem_s) - x_iValOff - sizeof(
                    ls_vardata_t));
    }
} LsShmHElem;


template< class T >
class TShmHash
    : public LsShmHash
{
private:
    TShmHash() {};
    TShmHash(const TShmHash &rhs);
    ~TShmHash() {};
    void operator=(const TShmHash &rhs);
public:
    class iterator
    {
        LsShmHash::iterator m_iter;
    public:
        iterator()
        {}

        iterator(LsShmHash::iterator iter) : m_iter(iter)
        {}
        iterator(LsShmHash::const_iterator iter)
            : m_iter((LsShmHash::iterator)iter)
        {}

        iterator(const iterator &rhs) : m_iter(rhs.m_iter)
        {}

        const void *first() const
        {  return  m_iter->first();   }

        T *second() const
        {   return (T *)(m_iter->second());   }

        operator LsShmHash::iterator()
        {   return m_iter;  }
    };
    typedef iterator const_iterator;

    LsShmOffset_t get(const void *pKey, int keyLen, int *valueLen, int *pFlag)
    {
        *valueLen = sizeof(T);
        return LsShmHash::get(pKey, keyLen, valueLen, pFlag);
    }

    LsShmOffset_t set(const void *pKey, int keyLen, T *pValue)
    {   return LsShmHash::set(pKey, keyLen, (const void *)pValue, sizeof(T));  }

    LsShmOffset_t insert(const void *pKey, int keyLen, T *pValue)
    {   return LsShmHash::insert(pKey, keyLen, (const void *)pValue, sizeof(T));  }

    LsShmOffset_t update(const void *pKey, int keyLen, T *pValue)
    {   return LsShmHash::update(pKey, keyLen, (const void *)pValue, sizeof(T));  }

    iteroffset begin()
    {   return LsShmHash::begin();  }

};

#endif // LSSHMHASH_H

