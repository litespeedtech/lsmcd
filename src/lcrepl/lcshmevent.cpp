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

#include "lcshmevent.h"
#include "replshmhelper.h"
#include "lcreplgroup.h"
#include "lcreplsender.h"

#include <memcache/lsmemcache.h>
#include <util/autobuf.h>
#include <log4cxx/logger.h>

LsCache2ReplEvent::LsCache2ReplEvent()
    : m_Idx(0)
    , m_lstReadTid(0)
{}

LsCache2ReplEvent::~LsCache2ReplEvent()
{}

int LsCache2ReplEvent::init(int idx, Multiplexer *pMultiplexer)
{
    m_Idx               = idx;
    m_lstReadTid        = ReplShmHelper::getInstance().getLastShmTid(idx);
    if (initNotifier(pMultiplexer))
        return LS_FAIL;
    return LS_OK;
}

int LsCache2ReplEvent::onNotified(int count)
{
    if ( !LcReplGroup::getInstance().getSockConnMgr()->getAcptedConnCnt())
    {
        LS_DBG_M("LsCache2ReplEvent::onNotified: no replication connection!");
        return LS_FAIL;
    }
    
    int idx = getIdx();
    LcNodeInfo * pLocalStatus = LcReplGroup::getInstance().getLocalNodeInfo();
    
    if (R_MASTER != pLocalStatus->getRole(idx))
    {
        char pBuf[32];
        LS_ERROR("LsCache2ReplEvent::onNotified pid:%d, IP:%s, role:[%s, idx:%d, but received cached's data event!"
            , getpid(), pLocalStatus->getIp()
            , LcNodeInfo::printStrRole(pLocalStatus->getRole(idx), pBuf, sizeof(pBuf)), idx);
        return LS_FAIL;
    }
    m_lstReadTid = LcReplSender::getInstance().bcastReplicableData(idx, m_lstReadTid);
    return LS_OK;
}    

LsRepl2CacheEvent::LsRepl2CacheEvent()
    : m_Idx(0)
{}

LsRepl2CacheEvent::~LsRepl2CacheEvent()
{}

int LsRepl2CacheEvent::onNotified(int count)
{
    char pAddr[64];
    int len = sizeof(pAddr);
    int idx = getIdx();
    LS_INFO("LsRepl2CacheEvent::onNotified idx:%d", idx);
    int ret = RoleMemCache::getInstance().readRoleViaShm(idx, pAddr, len);
    if (ret > 0)
    {
        LS_INFO("LsRepl2CacheEvent::onNotified pid:%d, received cached's event idx %d to connect to master addr:%s"
            , getpid(), idx, pAddr);
        LsMemcache::getInstance().setSliceDst(idx, pAddr);
    }
    else
    {   
        LS_INFO("LsRepl2CacheEvent::onNotified pid:%d, received cached's event idx[%d], I will be master"
            , getpid(), idx);
        LsMemcache::getInstance().setSliceDst(idx, NULL);
    }
    return LS_OK;
}    
