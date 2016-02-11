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

#ifndef __LSSHMEVENT_H__
#define __LSSHMEVENT_H__

#include <lsdef.h>
#include <edio/eventnotifier.h>


class LsCache2ReplEvent : public EventNotifier
{
public:

    LsCache2ReplEvent();
    ~LsCache2ReplEvent();
    int init(int idx, Multiplexer *pMultiplexer);
    virtual int onNotified(int count);
    bool    canIgnoreEvent();
    void    setIdx(int idx)                     {   m_Idx =  idx;    }
    int     getIdx() const                      {   return m_Idx;   }
private:
    int         m_Idx;
    uint64_t    m_lstReadTid;
    LS_NO_COPY_ASSIGN(LsCache2ReplEvent)
};

class LsRepl2CacheEvent : public EventNotifier
{
public:

    LsRepl2CacheEvent();
    ~LsRepl2CacheEvent();
    virtual int onNotified(int count);
    bool    canIgnoreEvent();
    void    setIdx(int idx)   {   m_Idx =  idx;    }
    int     getIdx() const   {   return m_Idx;   }
private:
    int m_Idx;   
    LS_NO_COPY_ASSIGN(LsRepl2CacheEvent)
};

#endif
