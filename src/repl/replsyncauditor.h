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


#ifndef __REPLSYNCAUDITOR_H__
#define __REPLSYNCAUDITOR_H__

#include "bulkreplctx.h"
#include "sockconn.h"
#include "nodeinfo.h"
#include <shm/lsshm.h>
#include <util/stringlist.h>

#include <log4cxx/logger.h>
#include <time.h>

uint32_t cmpTimeHashList(int localCount, const char *pBuf1
    , int peerCount, const char *pBuf2, AutoBuf &localOnly);

int  cmpRTReplHash(const char *localBuf, int len1, const char *peerBuf, 
                            int len2, AutoBuf &localOnly);
    
class IncReplAuditor 
{
public:    
    IncReplAuditor();
    ~IncReplAuditor();
    void        start(bool bInitiator, time_t freqTm, time_t cutoffTm);
    void        setCutOffTm(uint32_t tm);                
    void        continueAudit(const ClientConn * pConn);
    void        doneOneAudit(uint32_t endTm);
    int         addBReplTask(ServerConn *pConn, const AutoBuf &diffBuf );
    bool        isInitiator() const             {       return m_bInitiator;    }
    void        setPaused(bool f)               {       m_bPaused = f;          }
    AutoBuf     *cacheRTReplHash(time_t startTm, time_t endTm);
    AutoBuf     *getSelfLstRTHash()            {       return &m_localLstRTHash;    }
private:    
    void        init();
    bool        nextAuditTmSlot();
private:
    bool                m_bInitiator;
    bool                m_bPaused;
    time_t              m_cutOffTm;
    time_t              m_upBoundTm;
    time_t              m_freqTm;
    AutoBuf             m_localLstRTHash;
    BulkReplCtx        *m_pBulkReplCtx; 
    const ClientConn   *m_pClntConn;
};


class FullReplAuditor
{
public:
    FullReplAuditor();
    void init();
    int  start(ServerConn *pFReplConn, const char *pTmSlotBuf, int len, uint32_t startTm, uint32_t endTm);
    void setIsStarter(bool b);
    bool getIsStarter() const           {    return m_isStarter;        }
    bool isStarting() const             {    return m_startTm > 1;      }  
    void setStartTm(uint32_t tm)        {    m_startTm = tm;            }
    uint32_t getPeerMinTm() const       {    return m_peerMinTm;        }
    uint32_t getPeerMaxTm() const       {    return m_peerMaxTm;        }
    uint32_t getStartTm() const         {    return m_startTm;          }
    void setEndTm(uint32_t tm)          {    m_endTm = tm;              } 
    void addDataSize(uint32_t size)     {    m_dataSize += size;        }

    bool isFReplDone() const;
    bool getCurrTmSlots(uint32_t &lowTmSlot, uint32_t &upTmSlot);
    bool advTmSlots(uint32_t nextJumpTm);
    void setPeerMMTm(uint32_t minTm, uint32_t maxTm);
private:
    bool        m_isStarter; //
    uint32_t    m_currLowTm; //current low bound
    uint32_t    m_currUpTm;  
    uint32_t    m_peerMinTm; //peer container minTm
    uint32_t    m_peerMaxTm; //peer container maxTm

    uint32_t    m_startTm;   //replication start, end tm   
    uint32_t    m_endTm;     
    
    uint32_t    m_dataSize;
    AutoStr     m_srcAddr;
    BulkReplCtx        *m_pBulkReplCtx;  
};
#endif
