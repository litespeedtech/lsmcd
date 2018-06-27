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
#ifndef LSSHMHASHMULTI_H
#define LSSHMHASHMULTI_H

#include <lsdef.h>
#include <shm/lsshmhash.h>
#include <lsr/ls_hash.h>
#include <memcache/lsmchashbyuser.h>

class MemcacheConn;
class Multiplexer;
typedef struct
{
    uint8_t          m_idx;    
    LsMcHashByUser   m_hashByUser;
    LsShmOffset_t    m_iHdrOff;
    MemcacheConn    *m_pConnSlaveToMaster;
} LsMcHashSlice;

class LsMcHashMulti
{
private:
    int  key2hashNum(LsShmHKey hkey, MemcacheConn *pConn);
    
public:
    LsMcHashMulti();
    ~LsMcHashMulti();

    int  init(int iCnt, const char **ppPathName, 
              const char *pHashName, LsShmHasher_fn fnHashKey, 
              LsShmValComp_fn fnValComp, int mode);

    int  foreach(int (*func)(LsMcHashSlice *pSlice, MemcacheConn *pConn, 
                             void *pArg), MemcacheConn *pConn, void *pArg);

    int  getMultiCnt()
    {   return m_iCnt;  }
    
    LsShmHKey getHKey(const void *pKey, int iLen)
    {   return (*m_fnHashKey)(pKey, iLen);  }

    LsMcHashSlice  *indx2hashSlice(int indx)
    {   return (m_pLastSlice = &m_pSlices[indx]);  }

    LsMcHashSlice  *key2hashSlice(LsShmHKey hkey, MemcacheConn *pConn)
    {   return (m_pLastSlice = &m_pSlices[key2hashNum(hkey, pConn)]);  }

    Multiplexer *getMultiplexer()
    {   return m_pMultiplexer;  }

    void setMultiplexer(Multiplexer *pMultiplexer)
    {   m_pMultiplexer = pMultiplexer;  }
    
    LsMemcache *getMemcache()
    {   return m_pMemcache; }

//     LOG4CXX_NS::Logger *getLogger()
//     {   return m_pLastSlice->m_pHash->getLogger();  }

private:
    LsMcHashMulti(const LsMcHashMulti &other);
    LsMcHashMulti &operator=(const LsMcHashMulti &other);

private:
    int             m_iCnt;
    LsShmHasher_fn  m_fnHashKey;
    uint32_t        m_iLastHashKey;
    LsMcHashSlice  *m_pSlices;
    LsMcHashSlice  *m_pLastSlice;
    Multiplexer    *m_pMultiplexer;
    LsMemcache     *m_pMemcache;
};

#endif // LSSHMHASHMULTI_H

