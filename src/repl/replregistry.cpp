#include "repldef.h"
#include "replregistry.h"
#include <log4cxx/logger.h>
#include <openssl/md5.h>

extern hash_key_t repl_int_hash ( const void * p );
extern int  repl_int_comp ( const void * pVal1, const void * pVal2 );

ReplRegistry::ReplRegistry()
    : m_pConMap ( 13, repl_int_hash, repl_int_comp )
{}

ReplRegistry::~ReplRegistry()
{
    m_pConMap.release_objects();
}

ReplRegistry& ReplRegistry::operator= ( const ReplRegistry& other )
{
    return *this;
}

ReplContainer * ReplRegistry::get ( int32_t id )
{
    ContMap::iterator it = m_pConMap.find ( ( void* )( long )id );

    if ( it == m_pConMap.end() )
        return NULL;

    return it.second();
}

int ReplRegistry::add ( int32_t id, ReplContainer *pContainer )
{
    ContMap::iterator it = m_pConMap.find ( ( void* )( long )id );

    if ( it != m_pConMap.end() )
        return -1;

    m_pConMap.insert ( ( void* )( long )id, pContainer );
    return 0;
}

ReplContainer * ReplRegistry::remove ( int32_t id )
{
    ContMap::iterator it = m_pConMap.find ( ( void* )( long )id );

    if ( it == m_pConMap.end() )
        return NULL;

    ReplContainer* p = it.second();
    m_pConMap.erase ( it );
    return p;
}

int ReplRegistry::size() const
{
    return m_pConMap.size();
}


int stat(void *cbData, void * pUData1, void * pUData2)
{
    int id, num;
    ReplContainer *pContainer = (ReplContainer *)cbData;
    AutoBuf *pAutoBuf = (AutoBuf *)pUData1;
    
    id = pContainer->getId();
    pAutoBuf->append((const char*)&id, sizeof(uint32_t));
    num = pContainer->getLruHashCnt();
    pAutoBuf->append((const char*)&num, sizeof(uint32_t));
    return LS_OK;
}

int ReplRegistry::getContId2CntBuf(AutoBuf *pAutoBuf)
{
    for_each(stat, (void*)pAutoBuf, NULL);
    return LS_OK;

}

int purgeExpiredFn(void *cbData, void * pUData1, void * pUData2)
{
    ReplContainer *pContainer = (ReplContainer *)cbData;
    pContainer->purgeExpired();
    return LS_OK;
}

int ReplRegistry::purgeAllExpired()
{
    for_each(purgeExpiredFn, NULL, NULL);
    return LS_OK;
}

int ReplRegistry::for_each( for_each_fn2 func, void *pUData1, void *pUData2)
{
    ReplContainer *pContainer;
    ContMap::iterator it;
    for (it = m_pConMap.begin(); it != m_pConMap.end(); it = m_pConMap.next(it))
    {
        pContainer = it.second();
        func((void*)pContainer, pUData1, pUData2);
    }
    return LS_OK;
}


const char *ReplRegistry::getContNameById(int Id)
{
    ReplContainer *pContainer;
    ContMap::iterator it;
    for (it = m_pConMap.begin(); it != m_pConMap.end(); it = m_pConMap.next(it))
    {
        pContainer = it.second();
        if (pContainer->getId() == Id)
            return pContainer->getName();
    }
    return NULL;    
}

int packLruHashTmRangeFn(void *cbData, void * pUData1, void *pUData2 )
{
    uint32_t contId, headTm, tailTm;

    ReplContainer *pContainer   = (ReplContainer *)cbData;
    AutoBuf *pAutoBuf           = (AutoBuf *)pUData1;
   
    contId      = pContainer->getId();
    headTm      = pContainer->getHeadLruHashTm(); 
    tailTm      = pContainer->getTailLruHashTm();
    
    LS_DBG_M("packLruHashTmRangeFn contId:%d, headTm:%d, tailTm:%d", contId, headTm, tailTm);    
    pAutoBuf->append((const char*)&contId, sizeof(uint32_t));    
    pAutoBuf->append((const char*)&headTm, sizeof(uint32_t));
    pAutoBuf->append((const char*)&tailTm, sizeof(uint32_t));
    return LS_OK;    
}

//pack order: container num, each containerID, sessCnt...
int ReplRegistry::packLruHashTmRange(AutoBuf &rAutoBuf)
{
    for_each(packLruHashTmRangeFn, (void *)&rAutoBuf, NULL);   
    return LS_OK;
}

int getContsLruMinMaxTmFn(void *cbData, void * pUData1, void *pUData2 )
{
    uint32_t tm;
    uint32_t *pMinTm, *pMaxTm;

    ReplContainer *pContainer   = (ReplContainer *)cbData;
    pMinTm       = (uint32_t *)pUData1;
    pMaxTm       = (uint32_t *)pUData2;
    
    tm          = pContainer->getHeadLruHashTm(); 
    if ( tm > 0 && tm < *pMinTm )
        *pMinTm = tm;

    tm          = pContainer->getTailLruHashTm(); 
    if ( tm > *pMaxTm )
        *pMaxTm = tm;

    return LS_OK;    
}

int ReplRegistry::getContsLruMinMaxTm(uint32_t &minTm, uint32_t &maxTm)
{
    uint32_t  currTm = time(NULL);
    minTm = currTm;
    maxTm = 0;
    for_each(getContsLruMinMaxTmFn, (void *)&minTm, (void *)&maxTm);
    LS_DBG_M("eplRegistry::getContsLruMinMaxT, minTm:%d, maxTm:%d", minTm, maxTm);
    
    if (maxTm == 0)
        maxTm = currTm;
    if (minTm == 0) //empty 
        minTm = maxTm;

    return LS_OK;
}

//find next min lru time
int cmpAllContNextTmFn(void *cbData, void * pUData1, void *pUData2 )
{
    ReplContainer *pContainer   = (ReplContainer *)cbData;
    const time_t *tm            = (const time_t *)pUData1;
    time_t *nextJumpTm          = (time_t *)pUData2;
    time_t tmp                  = pContainer->getCurrLruNextTm(*tm);
    LS_DBG_M("cmpAllContNextTmFn, min NextTm:%d, currTm:%d", *nextJumpTm, tmp );    
    if (tmp > 0 && tmp < *nextJumpTm)
        *nextJumpTm =  tmp;
    return LS_OK;    
}

time_t ReplRegistry::getAllContNextTm(time_t tm)
{
    time_t nextJumpTm = (tm +1) * 2;
    for_each(cmpAllContNextTmFn, (void *)&tm, (void *)&nextJumpTm);
    assert( nextJumpTm >= tm );
    return nextJumpTm;
}

int fillContsAllTmFn(void *cbData, void * pUData1, void *pUData2 )
{
    ReplContainer *pContainer   = (ReplContainer *)cbData;
    const uint32_t *pTm         = (const uint32_t *)pUData1;
    AutoBuf *pAutoBuf           = (AutoBuf *)pUData2;
    uint32_t contId             = pContainer->getId();
    int diffCnt                 = pTm[1] - pTm[0] + 1;
    LS_DBG_M("fillContsAllTmFn contId:%d, diffCnt:%d", contId, diffCnt);
    pAutoBuf->append((const char*)&diffCnt, sizeof(int) );
    pAutoBuf->append((const char*)&contId, sizeof(uint32_t) );
    for (uint32_t tm = pTm[0] ; tm <= pTm[1]; ++tm)
    {
        pAutoBuf->append((const char*)&tm, sizeof(uint32_t) );
    }
    return LS_OK;    
}

/*
 * pack format:
 * container count 
 * tm count1, containerId1 tm1, tm2...
 * tm count2, containerId2 tm1, tm2...
 */
int ReplRegistry::fillContsAllTm(uint32_t startTm, uint32_t endTm, AutoBuf &autoBuf)
{
    uint32_t tm[2];
    tm[0] = startTm;
    tm[1] = endTm;
    
    for_each(fillContsAllTmFn, (void *)tm, (void *)&autoBuf);
    return LS_OK;
}


int getContNameFn(void *cbData, void * pUData1, void *pUData2 )
{
    ReplContainer *pContainer = (ReplContainer *)cbData;
    StringList *pContNameLst  = (StringList *)pUData1;
    pContNameLst->add(pContainer->getName());
    return LS_OK;    
}

//get container name MD5 (16 bytes) 
int ReplRegistry::getContNameMD5Lst(AutoBuf &rAutoBuf)
{
    unsigned char buf[16];
    const char *ptr;
    StringList contNameLst;
    StringList::const_iterator itr;
    
    for_each(getContNameFn, (void *)&contNameLst, NULL); 
    contNameLst.sort();
    
    for (itr = contNameLst.begin(); itr != contNameLst.end(); itr++ )
    {
        ptr = (*itr)->c_str();
        MD5_CTX     md5_ctx;
        MD5_Init(&md5_ctx);
        MD5_Update(&md5_ctx, ptr, strlen(ptr));
        MD5_Final(buf, &md5_ctx);
        rAutoBuf.append((const char*)buf, 16);
    }
    return contNameLst.size();
}

//compare container name MD5 string 
int ReplRegistry::cmpContNameMD5Lst(const char *pLocalMD5Lst, int len1, const char *pPeerMD5Lst, int len2) const
{
    if (len1 != len2)
    {
        LS_ERROR("HA replication config len is different, unable to replicate correctly");
        return LS_FAIL;
    }
    int cnt = len1 / 16;
    
    for (int i = 0; i < cnt; ++i)
    {
        if (memcmp( (pLocalMD5Lst + 16 * i), (pPeerMD5Lst + 16 * i), 16))
        {
            LS_ERROR("HA replication config MD5 is different, unable to replicate correctly");
            return LS_FAIL;
        }
    }
    return LS_OK;
}

/*
 * format:
 * startTm(uint32_t), endTm(uint32_t),  container count
 * Tm count, containerId1, Tm1, Hash1, Tm2, Hash2 .....
 * Tm count, containerId2, Tm1, Hash1, Tm2, Hash2 .....
 */
int ReplRegistry::calRTReplHash(time_t startTm, time_t endTm, AutoBuf& rAutoBuf)
{
    int contId, tmCnt, totTmCnt, size;
    ReplContainer *pContainer;
    ContMap::iterator it;
    
    rAutoBuf.append((const char*)&startTm, sizeof(uint32_t));
    rAutoBuf.append((const char*)&endTm, sizeof(uint32_t));
    
    int num     = this->size();
    rAutoBuf.append((const char*)&num, sizeof(int));
    totTmCnt    = 0;

    for (it = m_pConMap.begin(); it != m_pConMap.end(); it = m_pConMap.next(it))
    {
        pContainer      = it.second();
        contId          = pContainer->getId();
        size            = rAutoBuf.size();
        tmCnt           = 0;
        
        rAutoBuf.append((const char*)&tmCnt, sizeof(int));
        rAutoBuf.append((const char*)&contId, sizeof(int));
        
        tmCnt           = pContainer->getLruHashKeyHash(startTm, endTm, rAutoBuf);
        if (tmCnt > 0)
        {
            *(int*)(rAutoBuf.begin() + size) = tmCnt;
            totTmCnt += tmCnt;
        }
    }
    LS_DBG_M("ReplGroup::calRTReplHash startTm:%d, endTm:%d, container num:%d, size:%d, totTmCnt:%d"
        , startTm, endTm, num, rAutoBuf.size(), totTmCnt);    
    return totTmCnt;
}
