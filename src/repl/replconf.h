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


#ifndef __REPLCONF_H__
#define __REPLCONF_H__

#include <util/stringlist.h>
#include <util/autostr.h>
#include <util/tsingleton.h>

class ReplConf
{
public:    
    ReplConf();
    ~ReplConf();

    StringList& getLBAddrs();
    bool        setLBAddrs(const char* pEntry);
    bool        isAllowedIP(const char* IP) const;
    const char *getLisenSvrAddr() const         {       return m_lsntnrSvrAddr.c_str();  }
    const char *getLocalLsntnrIp() const;
    bool        setLisenSvrAddr(const char* pAddr);
    
    bool isGzipStream() const                   {       return m_bGzipStream;   }
    void setIsGzip(bool b)                      {       m_bGzipStream = b;}

    int    getHbTimeout() const          {       return m_hbTimeout;      }
    void   setHbTimeout(int secs);
    
    int    getHbFreq() const             {       return m_hbFreq;         }
    void   setHbFreq(int secs);
    
    void setIncSync(bool b)                     {       m_bIncSync = b;         }
    bool isIncSync() const                      {       return m_bIncSync;      }
    
    void setSockCached(bool b)                  {       m_bSockCached = b;         }
    bool isSockCached() const                   {       return m_bSockCached;      }
    
    
    const StringList &getLbLegalIps() const     {       return m_LbLegalIps;    } 

protected:
    int                 m_hbTimeout;
    int                 m_hbFreq;
    AutoStr             m_lsntnrSvrAddr;
    AutoStr             m_lsntnrSvrIp;
    bool                m_bGzipStream;
    bool                m_bIncSync;
    bool                m_bSockCached;
    StringList          m_LbPeerAddrs;
    StringList          m_LbLegalIps;
};


class ConfWrapper : public TSingleton<ConfWrapper>
{
    friend class TSingleton<ConfWrapper>;
public:
    bool loadConf(ReplConf *pConf);
    ReplConf * operator->();
    ReplConf * getConf();
private:
    ConfWrapper();
    ~ConfWrapper();
private:
    ReplConf *m_pConf;    
};

#endif
