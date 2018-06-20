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
#include <memcache/lsmchashmulti.h>
#include <memcache/lsmemcache.h>
#include <shm/lsshmtidmgr.h>
#include <memcacheconn.h>
#include <log4cxx/logger.h>


#include <stdlib.h>
#include <string.h>
#include <stdio.h>

LsMcHashByUser::LsMcHashByUser()
    :m_pShm(NULL)
    ,m_pGPool(NULL)    
    ,m_pHashname(NULL)
    ,m_fnHasher(NULL)
    ,m_fnValComp(NULL)
    ,m_mode(0)
    ,m_usesasl(false)
    ,m_anonymous(false)
    ,m_byUser(false)
    ,m_userHashes(NULL)
    ,m_pHashDefault(NULL)
{}
    
void LsMcHashByUser::del()
{
    if (m_userHashes)
    {
        ls_hash_iter iterNext = ls_hash_begin(m_userHashes);
        ls_hash_iter iter;
        while ((iterNext != NULL) && (iterNext != ls_hash_end(m_userHashes)))
        {
            iter = iterNext;
            iterNext = ls_hash_next(m_userHashes, iterNext);
            void *pData = ls_hash_getdata(iter);
            if (pData)
            {
                getDataFromHashTableData((const char *)pData)->close();
                free(pData);
            }
        }
        ls_hash_delete(m_userHashes);
        m_userHashes = NULL;
    }
    if (m_pHashDefault)
    {
        m_pHashDefault->close();
        m_pHashDefault = NULL;
    }
}


LsMcHashByUser::~LsMcHashByUser()
{
    del();
}


bool LsMcHashByUser::init(
    LsMemcache      *pMemcache,
    LsShm           *pShm,
    LsShmPool       *pGPool,
    const char      *pHashname,
    LsShmHasher_fn   fnHasher,
    LsShmValComp_fn  fnValComp,
    int              mode,
    bool             usesasl,
    bool             anonymous,
    bool             byUser)
{
    m_pMemcache = pMemcache;
    m_pShm      = pShm;
    m_pGPool    = pGPool;
    m_pHashname = pHashname;
    m_fnHasher  = fnHasher;
    m_fnValComp = fnValComp;
    m_mode      = mode;
    m_usesasl   = usesasl;
    m_anonymous = anonymous;
    m_byUser    = byUser;
    
    if ((usesasl) && (byUser) && (!m_userHashes))
    {
        m_userHashes = ls_hash_new(0, ls_hash_hfstring, ls_hash_cmpstring, NULL);
        if (!m_userHashes)
        {
            LS_ERROR("Insufficient memory to create hash/hash table\n");
            return false; 
        }
    }
    if ((!usesasl) || (!byUser) || ((anonymous) && (byUser)))
    {
        LS_DBG_M("Allocate default hash\n");
        m_pHashDefault = pGPool->getNamedHash(pHashname, 500000, fnHasher, 
                                              fnValComp, mode);
        if (!m_pHashDefault)
        {
            LS_ERROR("Error creating default hash: %s\n", pHashname);
            ls_hash_delete(m_userHashes);
            return false; 
        }
    }
    return true;
}


bool LsMcHashByUser::insert_user_entry(const char* user, LsShmHash* hash)
{
    int user_len = strlen(user);
    int user_entry_len = user_len + 1 + sizeof(LsShmHash *);
    char *user_entry;
    user_entry = (char *)malloc(user_entry_len);
    if (!user_entry)
    {
        LS_ERROR("Insufficient memory allocating hash table user info\n");
        return false;
    }
    memcpy(user_entry, user, user_len + 1);
    memcpy(&user_entry[user_len + 1], &hash, sizeof(hash));
    if (ls_hash_insert(m_userHashes, (const void *)user, (void *)user) == NULL)
    {
        free(user_entry);
        hash->close();
        LS_ERROR("Error inserting into hash table user info\n");
        return false;
    }
    return true;
}


LsShmHash *LsMcHashByUser::getHash(char *user)
{
    LsShmHash *pHash = NULL;
    
    if (!user)
    {
        LS_DBG_M("getHash default\n");
        if (m_pHashDefault)
            return m_pHashDefault;
        
        LS_ERROR("You must specify a user at all times with your configuration\n");
        return NULL;
    }
    LS_DBG_M("getHash user: %s\n", user);
    ls_hash_iter it = ls_hash_find(m_userHashes, user);
    if (it != NULL)
    {
        
        char *keydata = (char *)ls_hash_getdata(it);
        pHash = getDataFromHashTableData(keydata);
        LS_DBG_M("getHash hash: %p\n", pHash);
        
        return pHash;
    }
    int userHashNameLen = strlen(m_pHashname) + 1 + strlen(user) + 1;
    char userHashName[userHashNameLen];
    snprintf(userHashName, userHashNameLen, "%s.%s", m_pHashname, user);
    LS_DBG_M("getHash for user use name: %s", userHashName);
    pHash = m_pGPool->getNamedHash(userHashName,
                                   500000, 
                                   m_fnHasher, 
                                   m_fnValComp, 
                                   m_mode);
    if (!pHash)
    {
        LS_ERROR("Error creating hash for user: %s (%s)\n", user, userHashName);
        return NULL;
    }
    if (!(insert_user_entry(user, pHash)))
        return NULL;
    LS_DBG_M("pHash: %p", pHash);
    return pHash;
}


LsMcHashMulti::LsMcHashMulti()
    : m_iCnt(0)
    , m_fnHashKey(NULL)
    , m_iLastHashKey(0)
    , m_pSlices(NULL)
    , m_pLastSlice(NULL)
    , m_pMultiplexer(NULL)
    , m_pMemcache(NULL)
{
}


LsMcHashMulti::~LsMcHashMulti()
{
    if (m_pSlices != NULL)
    {
        LsMcHashSlice *pSlice = m_pSlices;
        while (m_iCnt > 0)
        {
            pSlice->m_hashByUser.del();
            delete pSlice->m_pConn;
            ++pSlice;
            --m_iCnt;
        }
        delete[] m_pSlices;
        m_pSlices = NULL;
    }
}


int LsMcHashMulti::init(LsMemcache *memcache, int iCnt, const char **ppPathName, 
                        const char *pHashName, LsShmHasher_fn fnHashKey, 
                        LsShmValComp_fn fnValComp, int mode, bool usesasl, 
                        bool anonymous, bool byUser)
{
    char *pDirName;
    char *pShmName;
    LsShm *pShm;
    LsShmPool *pGPool;
    LsMcHashSlice *pSlice;
    char buf[4096];

    m_pMemcache = memcache;
    if ((iCnt < 1) || (ppPathName == NULL) || (pHashName == NULL))
        return LS_FAIL;
    if (fnHashKey == NULL)
        fnHashKey = LsShmHash::hashXXH32;
    if (fnValComp == NULL)
        fnValComp = memcmp;
    m_fnHashKey = fnHashKey;

    int lstIdx = iCnt - 1 ;
    m_pSlices = new LsMcHashSlice [iCnt];
    pSlice = m_pSlices;
    int ret = LS_OK;
    while (--iCnt >= 0)
    {
        if ((pShmName = strrchr((char *)*ppPathName, '/')) != NULL)
        {
            int cnt;
            if ((cnt = pShmName - *ppPathName) > (int)(sizeof(buf) - 1))
            {
                LS_ERROR("LsShmHashMulti::init Name too big! [%.64s...]!\n",
                    *ppPathName);
                ret = LS_FAIL;
                break;
            }
            memcpy(buf, *ppPathName, cnt);
            buf[cnt] = '\0';
            pDirName = buf;
            ++pShmName;
        }
        else
        {
            pDirName = NULL;
            pShmName = (char *)*ppPathName;
        }
        if ((pShm = LsShm::open(pShmName, 0, pDirName)) == NULL)
        {
            LS_ERROR("LsShm::open [%s] failed!\n", *ppPathName);
            ret = LS_FAIL;
            break;
        }

        if ((pGPool = pShm->getGlobalPool()) == NULL)
        {
            LS_ERROR("getGlobalPool failed! [%s]\n", *ppPathName);
            ret = LS_FAIL;
            break;
        }

        if (!(pSlice->m_hashByUser.init(memcache, pShm, pGPool, pHashName, 
                                        fnHashKey, fnValComp, mode, usesasl, 
                                        anonymous, byUser)))
        {
            ret = LS_FAIL;
            break;
        }
        pSlice->m_idx = lstIdx - iCnt;
        LS_DBG_M(" LsMcHashMulti::init m_idx:%d", pSlice->m_idx);
        pSlice->m_iHdrOff = 0;
        pSlice->m_pConn = NULL;

        m_pLastSlice = pSlice;
        ++ppPathName;
        ++pSlice;
        ++m_iCnt;
    }

    if (ret != LS_OK)
    {
        while (m_iCnt > 0)
        {
            (--pSlice)->m_hashByUser.del();
            --m_iCnt;
        }
        delete[] m_pSlices;
        m_pSlices = NULL;
    }
    return ret;
}


int LsMcHashMulti::foreach(
    int (*func)(LsMcHashSlice *pSlice, void *pArg), void *pArg)
{
    int cnt = m_iCnt;
    LsMcHashSlice *pSlice = &m_pSlices[0];
    while (--cnt >= 0)
    {
        if ((*func)(pSlice, pArg) == LS_FAIL)
            return LS_FAIL;
        ++pSlice;
    }
    return m_iCnt;
}
