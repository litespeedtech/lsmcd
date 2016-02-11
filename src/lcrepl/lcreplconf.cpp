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

#include "lcreplconf.h"
#include <repl/repldef.h>
#include <log4cxx/logger.h>
#include <socket/gsockaddr.h>
#include <log4cxx/appender.h>
#include <log4cxx/layout.h>
#include <log4cxx/logger.h>

#include <stdlib.h>
#include <stdio.h>
#include <strings.h>

#define DEF_HEARTBEATRETRY  3
#define DEF_HEARTBEATFREQ   1
#define DEF_MAXTIDPACKET    1024000
#define DEF_USECAS          true
#define DEF_USESASL         false
#define DEF_NOMEMFAIL       false
#define DEF_VALMAXSZ        0
#define DEF_MEMMAXSZ        0

static log4cxx::Logger* initLogger(const char* logFile, const char* logLevel)
{
    static char s_parttern[] = "%d [%p] [%c] %m";
    using namespace LOG4CXX_NS;    
    Logger *pLogger = Logger::getRootLogger() ;
    Appender * appender
    = Appender::getAppender( logFile, "appender.ps" );
    Layout* layout = Layout::getLayout( "patternErrLog", "layout.pattern" );
    layout->setUData( s_parttern );
    appender->setLayout( layout );
    log4cxx::Level::setDefaultLevel(Level::toInt(logLevel));
    pLogger->setLevel( logLevel );
    pLogger->setAppender( appender );
    return pLogger;
}

LcReplConf::LcReplConf()
    : m_useCas(true)
    , m_useSasl(false)
    , m_noMemFail(false)
    , m_iSubFileNum(1)
    , m_pPriorities(NULL)
    , m_pShmFiles(NULL)
{}

LcReplConf::~LcReplConf()
{
    m_LbPeerAddrs.clear();
    delete []m_pPriorities;
    delete []m_pShmFiles;
}

bool LcReplConf::parse(const char *szFile)
{
    char pSockAddr[64];
    const char* pAddr;    
    StringList lbAddrs;
    AutoStr str;
    if (!m_confParser.loadConfig(szFile, "="))
    {
        initLogger("stderr", "DBG");
        LS_ERROR (  "LcReplConf unable to load config %s, error:%s"
            , szFile, m_confParser.getErrorMessage());
        return false;               
    }
    initLogger(getLogFile(), getLogLevel());

    const char *pEntry = m_confParser.getConfig("REPL.LBADDRS");
    setLBAddrs(pEntry);
    
    GSockAddr sockAddr;
    //LISTENSVRADDR
    pAddr = m_confParser.getConfig("REPL.LISTENSVRADDR");
    if (!setLisenSvrAddr(pAddr))
    {
        LS_ERROR ( "LcReplConf fails to load Repl ListenSvrAddr %s", pAddr);
        return false;        
    }
    m_lsntnrSvrIp = getIpOfAddr(pAddr, str);
    
    if ( !isAllowedIP(getLocalLsntnrIp() ))
    {
        LS_ERROR ( "Repl ListenSvrIp %s is not in LB Addrs"
            , getLocalLsntnrIp());
        return false;
    }
    //MEMCACHEDADDR
    pAddr = m_confParser.getConfig("CACHED.ADDR");
    if (pAddr == NULL || sockAddr.set(pAddr, 0))
    {
        LS_ERROR (  "LcReplConf fails to load Repl MemCached SvrAddr %s", pAddr);
        return false;
    }
    sockAddr.toString(pSockAddr, 64);
    m_cachedAddr = pSockAddr;
    
    //GzipStream
    const char *ptr = m_confParser.getConfig("REPL.GZIPSTREAM");
    if(ptr != NULL && !strcasecmp(ptr, "YES"))
        m_bGzipStream = true;
    
    //SubFileNum
    int v;
    ptr = m_confParser.getConfig("CACHED.SLICES");
    if ((ptr != NULL) && ((v = atoi(ptr)) > 0))
        m_iSubFileNum = v;
    
    if ( m_confParser.getConfig("CACHED.SHMDIR")) 
        m_shmDir = m_confParser.getConfig("CACHED.SHMDIR");
    if ( m_confParser.getConfig("CACHED.SHMNAME"))
        m_shmName = m_confParser.getConfig("CACHED.SHMNAME");
    if (m_confParser.getConfig("CACHED.SHMHASHNAME"))
        m_shmHashName = m_confParser.getConfig("CACHED.SHMHASHNAME");

    ptr = m_confParser.getConfig("REPL.MAXTIDPACKET");
    m_maxTidPacket = ptr ? atoi(ptr) : DEF_MAXTIDPACKET;

    ptr = m_confParser.getConfig("REPL.VALMAXSZ");
    m_valMaxSz = ptr ? atoi(ptr) : DEF_VALMAXSZ;
    
    ptr = m_confParser.getConfig("REPL.MEMMAXSZ");
    m_memMaxSz = ptr ? atoi(ptr) : DEF_MEMMAXSZ;

    if ( (ptr = m_confParser.getConfig("CACHED.USECAS")) != NULL)
    {
        m_useCas = strcmp(ptr, "TRUE") ? false : true;
    }
    
    if ((ptr = m_confParser.getConfig("CACHED.USESASL")) != NULL )
    {
        if ( !strcmp(ptr, "TRUE"))
            m_useSasl = true;        
    }
    
    if ((ptr = m_confParser.getConfig("CACHED.NOMEMFAIL")) != NULL )
    {
        if ( !strcmp(ptr, "TRUE"))
            m_noMemFail = true;        
    }
    
    char pBuf[1024], pBuf2[1024];
    uint16_t num  = getSubFileNum();
    m_pPriorities = new int[num];
    m_pShmFiles = new char *[num];
    for (int i=0; i < num; ++i)
    {   //Cached.Slice.
        snprintf(pBuf, sizeof(pBuf), "CACHED.SLICE.PRIORITY.%d", i);
        const char *ptr = m_confParser.getConfig(pBuf);
        if ((ptr != NULL) && ((v = atoi(ptr)) >= 0))
        {
            LS_DBG_M (  "%s=%d", pBuf, v);
            m_pPriorities[i] = v;
        }
        snprintf(pBuf, sizeof(pBuf), "CACHED.SHMFILEPATH.%d", i);
        ptr = m_confParser.getConfig(pBuf);
        if (ptr != NULL)
        {
            snprintf(pBuf2, sizeof(pBuf2), "%s/%s", m_shmDir.c_str(), ptr);
            m_confParser.setConfig(pBuf, pBuf2);
            m_pShmFiles[i] = (char*)m_confParser.getConfig(pBuf);
            LS_DBG_M("%s=%s", pBuf, pBuf2);
        }
        else
        {
            LS_ERROR ( 
                "[ERROR] [Configure]: LcReplConf config file, missing required: %s\n", pBuf);
            return false;               
        }
    }
    
    return true;
}

const char *LcReplConf::getMemCachedAddr() const
{
    return m_cachedAddr.c_str();
}

const char *LcReplConf::getShmDir() const
{
    return  m_shmDir.c_str();
}

const char *LcReplConf::getShmName() const
{
    return m_shmName.c_str();
}

const char *LcReplConf::getShmHashName() const
{
    return m_shmHashName.c_str();
}

uint16_t LcReplConf::getSubFileNum() const
{
    return m_iSubFileNum;
}

int *LcReplConf::getPriorities(int *count) const
{
    *count = getSubFileNum();
    return m_pPriorities;
}

char **LcReplConf::getShmFiles() const
{
    return m_pShmFiles;
}

uint32_t LcReplConf::getMaxTidPacket() const
{
    return m_maxTidPacket;
}


bool LcReplConf::getUseCas() const
{
    return m_useCas;
}

bool LcReplConf::getUseSasl() const
{
    return m_useSasl;
}

bool LcReplConf::getNomemFail() const
{
    return m_noMemFail;
}

uint32_t LcReplConf::getValMaxSz() const
{
    return m_valMaxSz;
}

uint32_t LcReplConf::getMemMaxSz() const
{
    return m_memMaxSz;
}

const char * LcReplConf::getLogLevel()
{
    const char *ptr = m_confParser.getConfig("LOGLEVEL");
    if ( !ptr)
    {
        m_confParser.setConfig("LOGLEVEL", "DBG_MEDIUM");
        ptr = m_confParser.getConfig("LOGLEVEL");
    }
    return ptr;
}

const char * LcReplConf::getLogFile()
{
    const char *ptr = m_confParser.getConfig("LOGFILE");
    if ( !ptr)
    {
        m_confParser.setConfig("LOGFILE", "stderr");
        ptr = m_confParser.getConfig("LOGFILE");
    }
    return ptr;
}

const char * LcReplConf::getTmpDir()
{
    const char *ptr = m_confParser.getConfig("TMPDIR");
    if ( !ptr)
    {
        m_confParser.setConfig("TMPDIR", "/tmp/lsmcd");
        ptr = m_confParser.getConfig("TMPDIR");
    }
    return ptr;
}

LcReplConf * getReplConf()
{
    return static_cast<LcReplConf*>(ConfWrapper::getInstance().getConf());
}
