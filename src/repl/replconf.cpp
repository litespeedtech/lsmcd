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

#include "replconf.h"
#include "repldef.h"
#include <log4cxx/logger.h>
#include <socket/gsockaddr.h>
#include <strings.h>

ReplConf::ReplConf()
    : m_hbTimeout(60)
    , m_hbFreq(30)
    , m_lsntnrSvrAddr("127.0.0.1:12340")
    , m_bGzipStream(true)
    , m_bIncSync(true)
    , m_bSockCached(false)
{
}


ReplConf::~ReplConf()
{
    m_LbPeerAddrs.clear();
    m_LbLegalIps.clear();
}

bool ReplConf::setLBAddrs(const char* pEntry)
{
    char pSockAddr[64];
    const char* pAddr;
    StringList lbAddrs;
    if (pEntry == NULL)
    {
        LS_INFO("HA replication configuration is not setup or invalid");
        return false;
    }
    lbAddrs.split(pEntry, pEntry + strlen(pEntry), ",");
    StringList::const_iterator itr;
    for ( itr = lbAddrs.begin(); itr != lbAddrs.end(); itr++ )
    {
        GSockAddr sockAddr;
        pAddr = (*itr)->c_str();
        if (!strncmp(pAddr, "*:", 2) || sockAddr.set(pAddr, 0))
        {
            LS_INFO("HA replication configuration Address %s is invalid", pAddr);
            return false;
        }
        sockAddr.toString(pSockAddr, 64);
        pAddr = strchr(pSockAddr, ':');
        AutoStr2 autostr(pSockAddr, pAddr - pSockAddr);
        
        if (m_LbLegalIps.find(autostr.c_str()) == NULL)
            m_LbLegalIps.add(autostr.c_str());

        if (!strcmp(m_lsntnrSvrAddr.c_str(), pSockAddr))
            continue;
        m_LbPeerAddrs.add(pSockAddr);
    }
    
//     const char *pAddr2 = m_lsntnrSvrAddr.c_str();
//     pAddr = strchr(pAddr2, ':');
//     m_lsntnrSvrIp.setStr(pAddr2, (int)(pAddr - pAddr2) );
    AutoStr str;
    m_lsntnrSvrIp = getIpOfAddr(m_lsntnrSvrAddr.c_str(), str);
    
    return true;
}

StringList& ReplConf::getLBAddrs()
{
    return m_LbPeerAddrs;
}

const char *ReplConf::getLocalLsntnrIp() const
{
    return m_lsntnrSvrIp.c_str();
}

bool ReplConf::isAllowedIP(const char* pAddr) const
{
    AutoStr str;
    const char *Ip = getIpOfAddr(pAddr, str);
    
    if (!m_LbLegalIps.size())
        return false;

//     if ( strchr(pAddr, ':') != NULL )
//     {
//         int len = strchr(pAddr, ':') - pAddr;
//         ipStr.setStr(pAddr, len);
//         Ip = ipStr.c_str();    
//     }
//     else 
//         Ip = pAddr;
    
    StringList::const_iterator itr;
    for (itr = m_LbLegalIps.begin(); itr != m_LbLegalIps.end(); itr++ )
    {
        if ( !strcmp ((*itr)->c_str() , Ip) )
            return true;
    }
    return false;
}


bool ReplConf::setLisenSvrAddr(const char* pAddr)
{ 
    if (pAddr == NULL)
        return false;
  
    char pSockAddr[64];
    GSockAddr sockAddr;
    if (!strncmp(pAddr, "*:", 2) || sockAddr.set(pAddr, 0))
        return false;
        
    sockAddr.toString(pSockAddr, 64);
    m_lsntnrSvrAddr = pSockAddr;         
    return true;        
}   


ConfWrapper::ConfWrapper()
    : m_pConf(NULL)
{
}

ConfWrapper::~ConfWrapper()
{
   if ( !m_pConf )
       delete m_pConf;
}

bool ConfWrapper::loadConf(ReplConf *pConf)
{
    assert(m_pConf == NULL);
    m_pConf = pConf;
    return true;
}

ReplConf * ConfWrapper::operator->()
{
    return m_pConf;
}

ReplConf * ConfWrapper::getConf()
{
    return m_pConf;
}
