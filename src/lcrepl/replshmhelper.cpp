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
/**
 * For reference of the usage of plain text configuration, please see wiki
 */

#include "replshmhelper.h"
#include "lcreplconf.h"
#include "lcreplgroup.h"
#include "replicableshmtid.h"
#include <repl/replcontainer.h>
#include <log4cxx/logger.h>
#include <shm/lsshmtidmgr.h>
#include <stdlib.h>
#include <stdio.h>


ReplShmHelper::ReplShmHelper():
              m_iMaxPacketSize(5120)
{}

ReplShmHelper::~ReplShmHelper()
{}

int ReplShmHelper::tidGetNxtItems(int idx, uint64_t* tid, uint8_t* pBuf, int isize)
{
    return LsMemcache::getInstance().tidGetNxtItems(getLsShmHash(idx), tid, pBuf, isize);
}

bool ReplShmHelper::readShmDBTid (int idx, LcNodeInfo* pStatus)
{
    pStatus->setCurrTid(idx, getLastShmTid(idx));
    //pStatus->setTidNum (idx)
    return true;
}

int ReplShmHelper::tidSetItems(int idx, uint8_t *pData, int dataLen)
{
    return LsMemcache::getInstance().tidSetItems(getLsShmHash(idx), pData, dataLen);
}


uint64_t ReplShmHelper::getLastShmTid(int idx)
{
    return getLsShmHash(idx)->getTidMgr()->getLastTid();
}

bool ReplShmHelper::getShmSliceRole(int idx)
{
    return getLsShmHash(idx)->isTidMaster();
}


int32_t ReplShmHelper::getMaxPacketSize()
{
    return m_iMaxPacketSize;
}


bool ReplShmHelper::incMaxPacketSize(int offBytes)
{
    uint32_t iTmp = m_iMaxPacketSize;

    if( offBytes > 0 )
        iTmp = offBytes;
    else if( offBytes < 0 ) //missing bytes
        iTmp = 1024 * ( (-offBytes / 1024 ) + 1 );
    
    if(getReplConf()->getMaxTidPacket() > iTmp )
    {
        m_iMaxPacketSize = iTmp;
        return true;
    }
    else
        return false;

}

uint64_t ReplShmHelper::bcastNewTidData (int idx, uint64_t iLstTid, void *pInst, for_each_fn2 func)
{
    uint64_t iLocalTid, iNewTid = iLstTid;
    ReplShmHelper &replShmHelper = ReplShmHelper::getInstance();
    uint32_t uContID = idx + 1;
    iLocalTid   = getLastShmTid(idx);
    int offset  = sizeof(iLocalTid);
    int extra   = 0;
    int loop    = 0;
    AutoBuf autoBuf(64000);
    while (loop++ < 10)
    {
        if (autoBuf.size() > 0)
            autoBuf.clear();
        autoBuf.reserve(64000 + extra);
        autoBuf.append((const char*)&iLocalTid, offset);
        int ret = replShmHelper.tidGetNxtItems(idx, &iNewTid,
            (uint8_t *)(autoBuf.begin() + offset), 64000 + extra - autoBuf.size() );
        LS_DBG_M("ReplShmHelper::bcastNewTidData ret:%d", ret);
        if ( !ret )
            break;
        else if (ret < 0 )
            extra = -ret; //trying with bigger buffer
        else 
        {
            assert(ret > 0);
            autoBuf.used(ret);
            LS_DBG_M("bcastNewTidData idx:%d tid:%lld, ret=%d, autoBuf size=%d"
                , idx, (long long)iNewTid, ret, autoBuf.size());
            func(pInst, (void*)&uContID , (void*)&autoBuf);
        }
    }
    return iNewTid;
}
