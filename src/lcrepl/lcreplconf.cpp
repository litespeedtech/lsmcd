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
#define DEF_ANONYMOUS       false
#define DEF_BYUSER          false

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
    , m_sliceCnt(1)
    , m_cachedProcCnt(1)
    , m_user("nobody")
    , m_group("nobody")
    , m_anonymous(false)
    , m_byUser(false)
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
    char pSockAddr[256];
    const char* pAddr;    
    StringList lbAddrs;
    GSockAddr sockAddr;
    int v;
    AutoStr str;

    if (!m_confParser.loadConfig(szFile, "="))
    {
        initLogger("stderr", "DBG");
        LS_ERROR("LcReplConf unable to load config %s, error:%s"
            , szFile, m_confParser.getErrorMessage());
        return false;               
    }
    initLogger(getLogFile(), getLogLevel());

   
    //MEMCACHEDADDR
    pAddr = m_confParser.getConfig("CACHED.ADDR");
    if (pAddr == NULL || sockAddr.set(pAddr, 0))
    {
        LS_ERROR("LcReplConf fails to load Repl MemCached SvrAddr %s", pAddr);
        return false;
    }
    //sockAddr.toString(pSockAddr, 256);
    m_cachedAddr = pAddr;
    
    pAddr = m_confParser.getConfig("CACHED.PRIADDR");
    if (pAddr == NULL || sockAddr.set(pAddr, 0))
    {
        LS_ERROR("LcReplConf fails to load Repl MemCached SvrAddr %s", pAddr);
        return false;
    }
    //sockAddr.toString(pSockAddr, 256);
    m_cachedPriAddr = pAddr;



    pAddr = m_confParser.getConfig("CachedSockPath");
    if (pAddr == NULL)
    {
        LS_ERROR("LcReplConf fails to load cached sock path %s", pAddr);
        return false;
    }
    m_cachedUsPath = pAddr;

    //GzipStream
    const char *ptr;
    //SubFileNum
    ptr = m_confParser.getConfig("CACHED.SLICES");
    if ((ptr != NULL) && ((v = atoi(ptr)) > 0))
        m_sliceCnt = v;
    
    if ( (ptr = m_confParser.getConfig("CACHED.SHMDIR")) != NULL ) 
        m_shmDir = ptr;
    else
        m_shmDir = "/dev/shm/lsmcd";

    if ( (ptr = m_confParser.getConfig("CACHED.SHMNAME")) != NULL)
        m_shmName = ptr;
    
    if ( (ptr = m_confParser.getConfig("CACHED.SHMHASHNAME")) != NULL)
        m_shmHashName = ptr;

    ptr = m_confParser.getConfig("CACHED.VALMAXSZ");
    m_valMaxSz = ptr ? atoi(ptr) : DEF_VALMAXSZ;
    
    ptr = m_confParser.getConfig("CACHED.MEMMAXSZ");
    m_memMaxSz = ptr ? atoi(ptr) : DEF_MEMMAXSZ;


    if ( (ptr = m_confParser.getConfig("CACHED.USECAS")) != NULL)
    {
        m_useCas = strcasecmp(ptr, "TRUE") ? false : true;
    }
    LS_DBG_M("About to test USESASL\n");
    
    if ((ptr = m_confParser.getConfig("CACHED.USESASL")) != NULL )
    {
        LS_DBG_M("CACHED.USESASL = %s\n", ptr);
        if ( !strcasecmp(ptr, "TRUE"))
            m_useSasl = true;        
    }
    
    if ((ptr = m_confParser.getConfig("CACHED.ANONYMOUS")) != NULL )
    {
        if ( !strcasecmp(ptr, "TRUE"))
            m_anonymous = true;        
    }
    
    if ((ptr = m_confParser.getConfig("CACHED.DATABYUSER")) != NULL )
    {
        if ( !strcasecmp(ptr, "TRUE"))
            m_byUser = true;        
    }
    
    if ((ptr = m_confParser.getConfig("CACHED.NOMEMFAIL")) != NULL )
    {
        if ( !strcasecmp(ptr, "TRUE"))
            m_noMemFail = true;        
    }
    
    if ((ptr = m_confParser.getConfig("User")) != NULL )
        m_user = ptr;
    
    if ((ptr = m_confParser.getConfig("Group")) != NULL )
        m_group = ptr;

    
    ptr = m_confParser.getConfig("CachedProcCnt");
    if ((ptr != NULL) && ((v = atoi(ptr)) > 0))
        m_cachedProcCnt = v;
        
    char pBuf[1024], pBuf2[1024];
    uint16_t num  = getSliceCount();
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
        snprintf(pBuf2, sizeof(pBuf2), "%s/data%d", m_shmDir.c_str(), i);
        m_confParser.setConfig(pBuf, pBuf2);
        m_pShmFiles[i] = (char*)m_confParser.getConfig(pBuf);
        LS_DBG_M("%s=%s", pBuf, pBuf2);
    }
    
    const char *pEntry = m_confParser.getConfig("REPL.LBADDRS");
    setLBAddrs(pEntry);
 
    ptr = m_confParser.getConfig("REPL.GZIPSTREAM");
    if(ptr != NULL && !strcasecmp(ptr, "YES"))
        m_bGzipStream = true;
    
    //timeout
    ptr = m_confParser.getConfig("REPL.HEARTBEATREQ");
    if ((ptr != NULL) && ((v = atoi(ptr)) > 0))
        m_hbFreq = v;
    LS_DBG_M("repld heartbeatreq:%d", getHbFreq());
    
    ptr = m_confParser.getConfig("REPL.HEARTBEATRETRY");
    if ((ptr != NULL) && ((v = atoi(ptr)) > 0))
        m_hbTimeout = v;
    LS_DBG_M("repld heartbtimeout:%d", getHbTimeout());
    
    //LISTENSVRADDR
    pAddr = m_confParser.getConfig("REPL.LISTENSVRADDR");
    if (setLisenSvrAddr(pAddr))
    {
        m_lsntnrSvrIp = getIpOfAddr(pAddr, str);
        if ( !isAllowedIP(getLocalLsntnrIp() ))
        {
            LS_ERROR("Repl ListenSvrIp %s is not in LB Addrs"
                , getLocalLsntnrIp());
            return false;
        }
    }
    
    //dispatch addr
    pAddr = m_confParser.getConfig("REPL.DispatchAddr");
    if ( pAddr == NULL || sockAddr.set(pAddr, 0) )
    {
        LS_ERROR("LcReplConf fails to load Repl DispatchAddr %s", pAddr);
        return false;        
    }
    sockAddr.toString(pSockAddr, 64);
    m_dispatchAddr = pSockAddr;
    
    //usock path
    pAddr = m_confParser.getConfig("RepldSockPath");
    if (pAddr != NULL)
    {
        m_repldUsPath = pAddr;
    }
    
    ptr = m_confParser.getConfig("REPL.MAXTIDPACKET");
    m_maxTidPacket = ptr ? atoi(ptr) : DEF_MAXTIDPACKET;

    
    
    return true;
}
const char* LcReplConf::getCachedPriAddr() const
{
    return m_cachedPriAddr.c_str();
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

uint16_t LcReplConf::getSliceCount() const
{
    return m_sliceCnt;
}

int *LcReplConf::getPriorities(int *count) const
{
    *count = getSliceCount();
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

const char * LcReplConf::getUser()
{
    return m_user.c_str();
}

const char * LcReplConf::getGroup()
{
    return m_group.c_str();
}

bool LcReplConf::getAnonymous()
{
    return m_anonymous;
}

bool LcReplConf::getByUser()
{
    return m_byUser;
}

uint16_t LcReplConf::getCachedProcCnt() const 
{
    return m_cachedProcCnt;
}

const char  *LcReplConf::getRepldUsPath() const
{
    return m_repldUsPath.c_str();
}

const char  *LcReplConf::getCachedUsPath() const
{
    return m_cachedUsPath.c_str();
}


const char *LcReplConf::getDispatchAddr() const
{
    return m_dispatchAddr.c_str();
}

LcReplConf * getReplConf()
{
    return static_cast<LcReplConf*>(ConfWrapper::getInstance().getConf());
}


