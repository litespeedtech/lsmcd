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
#ifndef LSMCHASHBYUSER_H
#define LSMCHASHBYUSER_H

#include <lsdef.h>
#include <shm/lsshmhash.h>
#include <lsr/ls_hash.h>

class LsShm;
class LsShmPool;
class LsMemcache;

class LsMcHashByUser
{
public:
    LsMcHashByUser(); 
    ~LsMcHashByUser();
    bool init(
        LsShm           *pShm,
        LsShmPool       *pGPool,
        const char      *pHashname,
        LsShmHasher_fn   fnHasher, 
        LsShmValComp_fn  fnValComp,
        int              mode);
    void del();
        
    LsShmHash  *getHash(char *user); // user can be NULL if !byUser or anon.
    
private:
    LsMcHashByUser(const LsMcHashByUser &other);
    LsMcHashByUser &operator=(const LsMcHashByUser &other);
    
    int hash_delete_fn(const void *pKey, void *pData);
    bool insert_user_entry(const char *user, LsShmHash *hash);
    
    LsShm           *m_pShm;
    LsShmPool       *m_pGPool;    
    const char      *m_pHashname;
    LsShmHasher_fn   m_fnHasher;
    LsShmValComp_fn  m_fnValComp;
    int              m_mode;
    ls_hash_t       *m_userHashes;
    LsShmHash       *m_pHashDefault;
};

#endif // HashbyUser include

