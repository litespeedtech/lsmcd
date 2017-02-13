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

#include "replsyncauditor.h"
#include "replgroup.h"
#include "replcontainer.h"
#include "replconf.h"
#include "replsender.h"
#include "lruhashreplbridge.h"
#include "replregistry.h"

/*
 * return format:
 * container count 
 * tm count1, containerId1 tm1, tm2...
 * tm count2, containerId2 tm1, tm2...
 */
int cmpRTReplHash(const char *pBuf1, int len1
    , const char *pBuf2, int len2, AutoBuf &diffBuf)
{
    //LS_DBG_M("IncReplAuditor::cmpRTReplHash local len:%d, peer len:%d", len1, len2);
    int localContCnt, peerContCnt, moreTmCnt, diffNum;
    int  localTmCnt, peerTmCnt, localContId, peerContId, curSize;
    diffNum     = 0;
    localContCnt = *(uint32_t *)pBuf1;
    pBuf1       += sizeof(uint32_t);
    len1        -= sizeof(uint32_t);
    
    peerContCnt  = *(uint32_t *)pBuf2;
    pBuf2       += sizeof(uint32_t);
    len2        -= sizeof(uint32_t);
    
    while (localContCnt > 0 && peerContCnt > 0)
    {
        localTmCnt = *(int *)pBuf1;
        pBuf1       += sizeof(int); len1 -=sizeof(int); 
        localContId = *(int *)pBuf1;
        pBuf1       += sizeof(int); len1 -=sizeof(int); 
        
        peerTmCnt = *(int *)pBuf2;
        pBuf2       += sizeof(int); len2 -=sizeof(int); 
        peerContId = *(int *)pBuf2;
        pBuf2       += sizeof(int); len2 -=sizeof(int); 
        
        //LS_DBG_M("IncReplAuditor::cmpRTReplHash,localTmCnt:%d,localContId:%d, len1:%d , peerTmCnt:%d,peerContId:%d, len2:%d",
        //         localTmCnt,localContId, len1, peerTmCnt,peerContId, len2);
        assert(localContId == peerContId);
        
        curSize         = diffBuf.size();
        moreTmCnt       = 0;
        diffBuf.append((const char*)&moreTmCnt, sizeof(int));
        diffBuf.append((const char*)&localContId, sizeof(int));
        moreTmCnt       = cmpTimeHashList(localTmCnt, pBuf1, peerTmCnt, pBuf2, diffBuf);
        
        *(int*)(diffBuf.begin() + curSize) = moreTmCnt;
        diffNum         += moreTmCnt;
        
        pBuf1           += localTmCnt * (sizeof(uint32_t) + 32);
        pBuf2           += peerTmCnt  * (sizeof(uint32_t) + 32);
        
        localContCnt--;
        peerContCnt--;
    }
    assert(localContCnt == 0 && peerContCnt == 0);
    
    return diffNum;
}
/*
 * compare following pack format:
 * Tm1(uint32_t), Hash1(32bytes string), Tm2(uint32_t), Hash2(32bytes string).....
 * localOnly return: Time(uint32_t) list
 */

uint32_t cmpTimeHashList(int localCount, const char *pBuf1
    , int peerCount, const char *pBuf2
    , AutoBuf &localOnly)
{
    int sz0 = localOnly.size();
    uint32_t tm1, tm2;
    const char *pHash1, *pHash2;
    
    while ( localCount > 0  && peerCount > 0)
    {
        tm1 = *(uint32_t *)pBuf1;
        tm2 = *(uint32_t *)pBuf2;
        pHash1 = (const char*)(pBuf1 + sizeof(uint32_t));
        pHash2 = (const char*)(pBuf2 + sizeof(uint32_t));
        
        if (tm1 < tm2)
        {
            LS_DBG_L("cmpTimeHashList local has more data: local tm slot %d, peer tm:%d", tm1, tm2);
            localOnly.append((const char*)&tm1, sizeof(uint32_t));
            localCount--;
            pBuf1 += sizeof(uint32_t) + 32;
        }
        else if ( tm1 > tm2)
        {
            LS_DBG_L("cmpTimeHashList peer has more data: local tm slot %d, peer tm:%d", tm1, tm2);
            //peerOnly.append((const char*)&tm1, sizeof(uint32_t));
            peerCount--;
            pBuf2 += sizeof(uint32_t) + 32;
        }
        else 
        {
            if (strncmp(pHash1, pHash2, 32))
            {
                localOnly.append((const char*)&tm1, sizeof(uint32_t));
                LS_WARN("compare Hash is different at time slot %d", tm1);
            }
            localCount--;
            peerCount--;
            pBuf1 += sizeof(uint32_t) + 32;
            pBuf2 += sizeof(uint32_t) + 32;
            
        }
    }
    while (localCount > 0)
    {
        tm1 = *(uint32_t *)pBuf1;
        localOnly.append((const char*)&tm1, sizeof(uint32_t));
        localCount--;
        pBuf1 += sizeof(uint32_t) + 32;
    }
    LS_DBG_M("IncReplAuditor::cmpTimeHashList return tm count:%ld", (localOnly.size() - sz0)/sizeof(uint32_t));
    return (localOnly.size() - sz0)/sizeof(uint32_t);
}


IncReplAuditor::IncReplAuditor()
    : m_bInitiator(false)
    , m_bPaused(false)
    , m_cutOffTm(0)
    , m_upBoundTm(0)
    , m_freqTm(0)
    , m_pBulkReplCtx(NULL)
    , m_pClntConn(NULL)
{}

IncReplAuditor::~IncReplAuditor()
{
}

void IncReplAuditor::start(bool bInitiator, time_t freqTm, time_t cutoffTm)
{
    m_bInitiator        = bInitiator;
    m_freqTm            = freqTm;
    m_cutOffTm          = cutoffTm;  
}

bool IncReplAuditor::nextAuditTmSlot()
{
    time_t currTm = time(NULL);
    if ( (m_cutOffTm + m_freqTm) >= (currTm - 1))
        return false;
    m_upBoundTm = m_cutOffTm + m_freqTm;
    return true;
}

void IncReplAuditor::setCutOffTm(uint32_t tm)
{
    LS_DBG_M("auditor time pushed to %d", tm);
    m_cutOffTm = tm;
}                

void IncReplAuditor::continueAudit(const ClientConn * pConn)
{
    LS_DBG_M("continue run time repl to listener server %s", pConn->getPeerAddr());
    if (m_bPaused)
    {
        LS_INFO("incremental sync is being paused");
        return;
    }
    m_pClntConn = pConn;
    
    if (!nextAuditTmSlot())
        return ;
    AutoBuf *pLocalHash  = cacheRTReplHash(m_cutOffTm, m_upBoundTm);
    getReplSender()->clntSendSelfRTHash((ClientConn*)pConn, pLocalHash);
}

/* pack format:
 * startTm, endTm, 
 * tm count1, containerId1 tm1, tm2...
 * tm count2, containerId2 tm1, tm2...
*/ 
int IncReplAuditor::addBReplTask(ServerConn *pConn, const AutoBuf &diffBuf )
{
    uint32_t startTm, endTm;
    const char *pBuf    = diffBuf.begin();

    startTm             = *(uint32_t*)pBuf;
    pBuf               += sizeof(uint32_t);
    
    endTm               = *(uint32_t*)pBuf;
    pBuf               += sizeof(uint32_t);
    
    LS_DBG_L("IncReplAuditor::addBReplTask, startTm:%d, endTm:%d, buf left size:%zd", 
             startTm, endTm, diffBuf.size() - sizeof(uint32_t) * 2);
    m_pBulkReplCtx      = new BulkReplCtx(BULK_REPL, startTm, endTm);
    m_pBulkReplCtx->start(pConn, pBuf, diffBuf.size() - sizeof(uint32_t) * 2);
    return LS_OK;
}


void IncReplAuditor::doneOneAudit(uint32_t endTm )
{
    //LS_DBG_M("local LB audit time pushed to %d", endTm);
    m_pClntConn = NULL;
    setCutOffTm(endTm);
    if (m_pBulkReplCtx != NULL)
    {
        delete m_pBulkReplCtx;
        m_pBulkReplCtx = NULL;
    }
}

AutoBuf *IncReplAuditor::cacheRTReplHash(time_t startTm, time_t endTm)
{
    m_localLstRTHash.clear();
    ReplRegistry::getInstance().calRTReplHash(startTm, endTm, m_localLstRTHash);
    return &m_localLstRTHash;
}

#define FREPLINT 600
FullReplAuditor::FullReplAuditor()
{
    init();
}

void FullReplAuditor::init()
{
    m_pBulkReplCtx = NULL;
    m_isStarter = false;
    m_currLowTm = 0;
    m_currUpTm  = 0;  
    m_peerMinTm = 0; 
    m_peerMaxTm = 0; 

    m_startTm   = 0;
    m_endTm     = 0;         
    m_dataSize  = 0;
}

/*
 * flag who is starter to push fully replication
 * currently new client is starter, then switch to master
*/

void FullReplAuditor::setIsStarter(bool b)
{   
    m_isStarter  = b;        
}   

int FullReplAuditor::start(ServerConn *pFReplConn, const char *pTmSlotBuf, int len, uint32_t startTm, uint32_t endTm)
{
    m_pBulkReplCtx = new BulkReplCtx(FULL_REPL, startTm, endTm);
    m_pBulkReplCtx->start(pFReplConn, pTmSlotBuf, len);
    return LS_OK;
}

bool FullReplAuditor::isFReplDone() const
{
    return m_endTm >= m_startTm  && m_startTm > 0;
}

bool FullReplAuditor::getCurrTmSlots(uint32_t &lowTmSlot, uint32_t &upTmSlot)
{
    if (m_currLowTm > m_peerMaxTm)
        return false;
    
    if (m_currLowTm + FREPLINT > m_peerMaxTm)
        m_currUpTm = m_peerMaxTm;
    else
        m_currUpTm = m_currLowTm + FREPLINT ;
    lowTmSlot = m_currLowTm;
    upTmSlot  = m_currUpTm;
    return true ;
}

bool FullReplAuditor::advTmSlots(uint32_t nextJumpTm)
{
    m_currLowTm = nextJumpTm;//m_currUpTm + 1;
    return true ;
}

void FullReplAuditor::setPeerMMTm(uint32_t minTm, uint32_t maxTm)
{
    m_peerMinTm = minTm;
    m_peerMaxTm = maxTm;
    m_currLowTm = m_peerMinTm;   

}



