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
#include <memcache/lsmchashbyuser.h>
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
{
    LS_DBG_M("HashByUser contructor\n");
}
    
void LsMcHashByUser::del()
{
    LS_DBG_M("HashByUser destructor\n");
    if (m_userHashes)
    {
        LS_DBG_M("HashByUser clearing hash table\n");
        ls_hash_iter iterNext = ls_hash_begin(m_userHashes);
        ls_hash_iter iter;
        while ((iterNext != NULL) && (iterNext != ls_hash_end(m_userHashes)))
        {
            iter = iterNext;
            iterNext = ls_hash_next(m_userHashes, iterNext);
            void *pData = ls_hash_getdata(iter);
            if (pData)
            {
                LS_DBG_M("HashByUser clean up %s\n", (char *)pData);
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
        LS_DBG_M("HashByUser creating user hashes\n");
        m_userHashes = ls_hash_new(0, ls_hash_hfstring, ls_hash_cmpstring, NULL);
        if (!m_userHashes)
        {
            LS_ERROR("Insufficient memory to create hash/hash table\n");
            return false; 
        }
    }
    if ((!usesasl) || (!byUser) || ((anonymous) && (byUser)))
    {
        LS_DBG_M("HashByUser Allocate default hash\n");
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
    LS_DBG_M("HashByUser insert user: %s\n", user);
    
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
        LS_DBG_M("HashByUser: getHash default\n");
        if (m_pHashDefault)
            return m_pHashDefault;
        
        LS_ERROR("You must specify a user at all times with your configuration\n");
        return NULL;
    }
    LS_DBG_M("HashByUser getHash user: %s\n", user);
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
    LS_DBG_M("HashByUser getHash for user use name: %s", userHashName);
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


