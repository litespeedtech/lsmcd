
#ifndef __USOCKCONN_H
#define __USOCKCONN_H

#include "usockdata.h"
#include <edio/eventreactor.h>
#include <socket/gsockaddr.h>

struct sockaddr;
class Multiplexer;
class GSockAddr;
class UsockConn : public EventReactor
{
public:
    enum
    {
        UX_NONE = 0,
        UXUP_CACHED_DATA_UPD,
        UXDW_FD_PASSING,
        UXDW_REPLD_ROLE_UPD,
        UXDW_REPLD_CONN_UPD
    };
    enum
    {
        US_DISCONNECTED,
        US_CONNECTING,
        US_CONNECTED,
        US_CLOSING,
        US_SHUTDOWN,
        US_CLOSED
    };

    UsockConn();
    virtual ~UsockConn();
    
    void closeConnection(bool bRecycle);
    bool isActive() const;    
    int SendEx(const char *msg, int len, int flags); 
    void SetMultiplexer( Multiplexer * pMplx )  {       m_pMultiplexer = pMplx;         }
    Multiplexer * getMultiplexer() const        {   return m_pMultiplexer;   }
    
    virtual const char * getLocalAddr() const   {       return NULL;                    }
    virtual int handleEvents( short events );
    void setLocalAddr(const char *pAddr);
    static RoleTracker * getRoleData()           {       return m_pRoleData;             } 
    
protected:
    virtual void recycle() = 0;
    virtual int onRead() = 0;
    
    int Read( char * pBuf, int len );
    int onHup();
    int onErr();
protected:
    int             m_iConnState;
    GSockAddr       m_localAddr;//ip:port
    GSockAddr       m_remoteAddr;
    Multiplexer    *m_pMultiplexer;
    uint8_t         m_proto;
    static RoleTracker  *m_pRoleData;
};

#endif
