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

#ifndef __LCREPLCONF_H__
#define __LCREPLCONF_H__

#include "config.h"
#include <repl/replconf.h>
#include <util/stringlist.h>
#include <util/tsingleton.h>
#include <stdint.h>

class LcReplConf : public ReplConf
{
public:    
    LcReplConf();
    ~LcReplConf();
    bool         parse(const char *szFile);

    uint16_t     getSliceCount() const;
    int        * getPriorities(int *count) const;       
    char      ** getShmFiles() const;
    const char * getMemCachedAddr() const;
    const char * getCachedPriAddr() const;
    const char * getShmDir() const;
    const char * getShmName() const;
    const char * getShmHashName() const;
    
    uint32_t     getMaxTidPacket() const;

    bool         getUseCas() const;
    uint8_t      getUseSasl() const;
    bool         getNomemFail() const;
    uint32_t     getValMaxSz() const;
    uint32_t     getMemMaxSz() const;
    
    const char  *getLogLevel();
    const char  *getLogFile();
    const char  *getTmpDir();
    const char  *getUser();
    const char  *getGroup();
    uint16_t     getCachedProcCnt() const;
    const char  *getDispatchAddr() const;
    
    const char  *getRepldUsPath() const;
    const char  *getCachedUsPath() const;
    
private:
    AutoStr             m_repldUsPath;
    AutoStr             m_cachedUsPath;
    AutoStr             m_dispatchAddr;
    AutoStr             m_cachedAddr;
    AutoStr             m_cachedPriAddr;
    AutoStr             m_shmDir;
    AutoStr             m_shmName;
    AutoStr             m_shmHashName;
    uint32_t            m_maxTidPacket;
    uint32_t            m_valMaxSz;
    uint32_t            m_memMaxSz;
    bool                m_useCas;
    bool                m_useSasl;
    bool                m_noMemFail;
    uint16_t            m_sliceCnt;
    uint16_t            m_cachedProcCnt;
    AutoStr             m_user;
    AutoStr             m_group;
    
    int                *m_pPriorities;
    char              **m_pShmFiles;
   
    Config              m_confParser;
};

LcReplConf * getReplConf();

#endif
