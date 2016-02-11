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

#ifndef __REPLSHMHELPER_H__
#define __REPLSHMHELPER_H__

#include <shm/lsshmhash.h>
#include <memcache/lsmemcache.h>
#include <util/autobuf.h>
#include <repl/repldef.h>
#include <stdint.h>
#include <util/tsingleton.h>
class LcNodeInfo;
class ReplShmHelper : public TSingleton<ReplShmHelper>
{
    friend class TSingleton<ReplShmHelper>;
public:    
    LsShmHash * getLsShmHash(int idx) const      {   return LsMemcache::getInstance().getHash(idx); }
    
    int         tidGetNxtItems(int idx, uint64_t* tid, uint8_t* pBuf, int isize);
    
    //bool        readShmDBTid (int idx, uint64_t *pCurrTid, uint32_t *pTidNum);
    bool        readShmDBTid (int idx, LcNodeInfo* pStatus);
    uint64_t    getLastShmTid(int idx);
    
    int         tidSetItems(int idx, uint8_t *pData, int dataLen);
    
    int32_t     getMaxPacketSize();
    bool        incMaxPacketSize(int offBytes);
    uint64_t    bcastNewTidData (int idx, uint64_t iLstTid, void *pInst, for_each_fn2 func);
private:
    ReplShmHelper();
    ~ReplShmHelper();
    
private:
    uint32_t m_iMaxPacketSize;
    LsShmHash *m_pLsShmHash;

};
#endif
