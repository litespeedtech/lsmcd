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

#include "bulkreplctx.h"
#include "replgroup.h"
#include "replsender.h"
#include "replregistry.h"
#include "lruhashreplbridge.h"
#include "replcontainer.h"

BulkReplCtx::BulkReplCtx(int opt, uint32_t lowTmSlot, uint32_t upTmSlot)
    : m_opt(opt)
    , m_startTm(0)
    , m_lowTmSlot(lowTmSlot)
    , m_upTmSlot(upTmSlot)
    , m_replPrgrssTcker()
    , m_pFReplConn(NULL)
{}

BulkReplCtx::~BulkReplCtx()
{}

int BulkReplCtx::start(ServerConn *pFReplConn, const char *pTmBuf, int len)
{
    int tmCnt, contId;
    m_startTm           = time(NULL);
    m_pFReplConn        = pFReplConn;
    
    while (len > 0)
    {
        tmCnt  =  *(int *)pTmBuf;
        pTmBuf += sizeof(int);
        len    -= sizeof(int);
        
        contId =  *(int *)pTmBuf;
        pTmBuf += sizeof(int);
        len    -= sizeof(int);
        
        LS_DBG_M("BulkReplCtx::start, TmCnt:%d, contId:%d", tmCnt, contId);
        if (tmCnt > 0)
        {
            m_replPrgrssTcker.addBReplTmSlot(contId, pTmBuf, tmCnt);
            pTmBuf += tmCnt * sizeof(uint32_t);
            len  -= tmCnt * sizeof(uint32_t);
        }
    }
    linkBReplCtx2Clnt();
    return LS_OK;
}

int BulkReplCtx::startBlocked(ReplConn *pConn, const char *pTmBuf, int len)
{
    int tmCnt, contId, bytes;
    AutoBuf autoBuf;
    ReplProgressTrker trker;
    while ( len > 0 )
    {
        tmCnt  =  *(int *)pTmBuf;
        pTmBuf += sizeof(int);
        len    -= sizeof(int);
        
        contId =  *(int *)pTmBuf;
        pTmBuf += sizeof(int);
        len    -= sizeof(int);
        LS_DBG_M("BulkReplCtx::startBlocked, tmCnt:%d, contId:%d", tmCnt, contId);
        if (tmCnt > 0)
        {
            trker.addBReplTmSlot(contId, pTmBuf, tmCnt);
            bytes = ReplRegistry::getInstance().get(contId)->readShmKeyVal(trker, uint32_t(-1), autoBuf);
            LS_DBG_M("BulkReplCtx::startBlocked read bytes:%d", bytes);
            if (bytes > 0)
            {
                getReplSender()->sendLruHashBlukData(pConn, contId, autoBuf);
                autoBuf.clear();
            }
            
            pTmBuf += tmCnt * sizeof(uint32_t);
            len  -= tmCnt * sizeof(uint32_t);
        }
    }
    return LS_OK;
}
    
uint32_t BulkReplCtx::getUsedSecs() const
{
    return time(NULL) - m_startTm;
}

//append countainer count in the front
void BulkReplCtx::resumeBulkRepl(ServerConn *pConn)
{
    //ReplGroup &group = ReplGroup::getInstance();    
    LS_DBG_L("Replication resumed by client addr=%s", pConn->getPeerAddr());
    if (isAllReplDone())
    {
        unlinkBReplCtx();
        LS_INFO("one step of %s Replication has completed in %d secs", isFRepl()? "Full" : "Bulk"
            , time(NULL) - m_startTm);
        if (isFRepl())
        {
            getReplGroup()->FReplDoneCallBack(pConn, m_lowTmSlot, m_upTmSlot);
            delete this;
        }
        else
        {
            m_upTmSlot++;
            getReplGroup()->firstLegAuditDone(pConn, m_lowTmSlot , m_upTmSlot);
        }
    }
    else
    {
        int contId, availSize, bytes;
        
        contId = m_replPrgrssTcker.trackNextCont();
        assert(contId > 0);
        AutoBuf autoBuf;
        availSize = FULLREPLBUFSZ - pConn->getOutgoingBufSize() - 100;
        LS_INFO("BulkReplCtx::resumeBulkRepl bufsz=%d", availSize);
        if (availSize <= 0 )
        {
            LS_DBG_L("BulkReplCtx::resumeBulkRepl bufsz=%d is not big enough, trying next event", availSize);
            return;
        }        
        
        bytes = ReplRegistry::getInstance().get(contId)->readShmKeyVal(m_replPrgrssTcker, availSize, autoBuf);
        LS_DBG_L("BulkReplCtx one step read bytes:%d to send", bytes);
        if (bytes > 0)
            getReplSender()->sendLruHashBlukData(pConn, contId, autoBuf);
    }
}


int BulkReplCtx::linkBReplCtx2Clnt()
{
    m_pFReplConn->setBulkReplCtx(this);
    m_pFReplConn->continueWrite();
    LS_DBG_L("%s Replication links context to client addr=%s"
        , isFRepl()? "Full" : "Bulk", m_pFReplConn->getPeerAddr());
    return LS_OK;
}


void BulkReplCtx::unlinkBReplCtx()
{
    LS_DBG_L("%s Replication unlinks context from addr=%s", m_pFReplConn->getPeerAddr()
        , isFRepl()? "Full" : "Bulk");
    m_pFReplConn->suspendWrite();
    m_pFReplConn->setBulkReplCtx(NULL);
}


bool BulkReplCtx::isAllReplDone()
{
    return m_replPrgrssTcker.isAllReplDone();
}


const char* BulkReplCtx::getBReplClntAddr() const
{
    if (m_pFReplConn != NULL)
        return m_pFReplConn->getPeerAddr();
    return NULL;
}


bool BulkReplCtx::isFRepl() const
{
    return m_opt == FULL_REPL;        
}

