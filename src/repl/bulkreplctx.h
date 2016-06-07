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


#ifndef __BULKREPLCTX_H__
#define __BULKREPLCTX_H__

#include "replprgrsstrkr.h"
#include "replconn.h"
#include "nodeinfo.h"
#include <shm/lsshm.h>
#include <log4cxx/logger.h>
class ReplContainer;
class ServerConn;

class BulkReplCtx 
{
public:    
    BulkReplCtx(int opt, uint32_t lowTmSlot, uint32_t upTmSlot);
    ~BulkReplCtx();
    int  start(ServerConn *pFReplConn, const char *pTmBuf, int len);
    static int  startBlocked(ReplConn *pConn, const char *pTmBuf, int len);
    void resumeBulkRepl(ServerConn *pConn);
    uint32_t            getUsedSecs() const;
    const char         *getBReplClntAddr() const;
    void unlinkBReplCtx();
    ReplProgressTrker &getReplTmSlotLst()   {       return m_replPrgrssTcker; }
    bool isFRepl() const;
private:    
    int  linkBReplCtx2Clnt();
    bool isAllReplDone();
private:
    int                 m_opt;
    time_t              m_startTm;
    uint32_t            m_lowTmSlot;
    uint32_t            m_upTmSlot;
    ReplProgressTrker   m_replPrgrssTcker;
    ServerConn         *m_pFReplConn;
};

#endif
