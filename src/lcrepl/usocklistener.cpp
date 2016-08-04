
#include "usocklistener.h"
#include "lcreplgroup.h"
#include "lcreplconf.h"
#include "usockmcd.h"
#include <util/fdpass.h>
#include <log4cxx/logger.h>
#include <edio/multiplexer.h>
#include <edio/multiplexerfactory.h>
#include <socket/coresocket.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <stdio.h>

UsocklListener::UsocklListener()
    : m_procCnt(0)
    , m_pMultiplexer( NULL )
    , m_svrMap(13, GHash::hash_fn(), GHash::val_comp())
{
}

UsocklListener::~UsocklListener()
{
    LS_DBG_L("UsocklListener::~UsocklListener called");
    if ( getfd() != -1 )
    {
        //Stop();
    }
    
    UsockSvr  *pSvr;
    SvrMap_t::iterator itr;
    for (itr = m_svrMap.begin(); itr != m_svrMap.end(); itr = m_svrMap.next(itr))
    {
        if (itr.second() != NULL )
        {
            pSvr = itr.second();
            pSvr->closeConnection(false);
            delete pSvr;
        }
    }
    m_svrMap.clear();
    
}

int UsocklListener::Start(int procCnt)
{
    m_procCnt           = procCnt;
    int fd;
    int ret = CoreSocket::listen( m_lstnrAddr, 100, &fd, 0, 0 );
    if ( ret != 0 )
    {
        LS_ERROR("lsmcd failed to start up usock listener");
        return ret;
    }    
    setfd(fd);    
    return SetSockAttr( fd );
}

int UsocklListener::SetSockAttr( int fd)
{
    ::fcntl( fd, F_SETFD, FD_CLOEXEC );
    ::fcntl( fd, F_SETFL, getMultiplexer()->getFLTag());
    int nodelay = 1;
    ::setsockopt( fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof( int ) );

    return getMultiplexer()->add( this, POLLIN|POLLHUP|POLLERR );
}

int UsocklListener::Stop()
{
    if ( getfd() != -1 )
    {
        //LOG_INFO(( "Stop listener %s.", getAddrStr() ));
        getMultiplexer()->remove( this );
        close( getfd() );
        setfd( -1 );
    }
    return 0;
}

int UsocklListener::SetListenerAddr( const char * pAddr )
{
    m_lstnrAddr.set(AF_UNIX, getReplConf()->getRepldUsPath(), 0);
    unlink(getReplConf()->getRepldUsPath());
    return 0;
}

//only master calls this function
int UsocklListener::handleEvents( short events )
{
    struct sockaddr_un peerAddr;
    int         fd;
    socklen_t   len;

    if ( !(events & POLLIN) )
        return 0;
    len = sizeof(peerAddr);
    fd = accept( getfd(), (struct sockaddr *)(&peerAddr), &len );
    if ( fd == -1 )
        return -1;
    
    int procId  = atoi(peerAddr.sun_path + strlen(getReplConf()->getCachedUsPath()));
    LS_DBG_L("usock accepts client connection, procId:%d ", procId);
    if (procId < 1 || procId > getReplConf()->getCachedProcCnt())
    {
        LS_ERROR("usock listener accepted a invalid procId");
        close (fd);
        return -1;
    }
    SvrMap_t::iterator itr = m_svrMap.find((void *)long (procId));
    if ( itr != m_svrMap.end() )
        delete itr.second();

    UsockSvr * pSvr = new UsockSvr();
    itr = m_svrMap.insert((void *)long (procId), pSvr);
    
    pSvr->SetMultiplexer( getMultiplexer() );
    ::fcntl( fd, F_SETFD, FD_CLOEXEC );
    ::fcntl( fd, F_SETFL, getMultiplexer()->getFLTag());
    int nodelay = 1;
    ::setsockopt( fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof( int ) );
    
    LS_DBG_L("usock acccepted client fd:%d, peer len:%d, path:%s", fd, len, peerAddr.sun_path);
     
    AutoStr addr(peerAddr.sun_path);
    if ( pSvr->svrInitConn( fd, addr) )
    {
        close( fd );
        m_svrMap.erase(itr);
        delete pSvr;
        LS_ERROR("Replication usock failed to initialize the client connection");
        return -1;
    }
    pSvr->initTidTracker(getReplConf()->getSliceCount());
    return 0;
    
}

int UsocklListener::forwardFd(const DispatchData_t &dispData, int fwFd)
{
    UsockSvr *pSvr;
    SvrMap_t::iterator itr = m_svrMap.find( (void *)long(dispData.x_procId));
    if ( itr != m_svrMap.end() && itr.second() != NULL )
    {
        pSvr= itr.second();
        pSvr->sendPassingFd((const char *)&dispData, sizeof(dispData), fwFd);
        return 0;
    }
    else
    {
        LS_ERROR("usock listener can't find the proc %d when forwarding fd", dispData.x_procId);
        return -1;
    }
}

void UsocklListener::repldNotifyConn(int conn)
{
    LS_DBG_M("UsocklListener::repldNotifyConn %d", conn);
    UsockSvr *pSvr;
    SvrMap_t::iterator itr;
    char bytes[2];
    bytes[0] = UsockConn::UXDW_REPLD_CONN_UPD;
    bytes[1] = (uint8_t)conn ;
    for (itr = m_svrMap.begin(); itr != m_svrMap.end(); itr = m_svrMap.next(itr))
    {
        if (itr.second() != NULL )
        {
            pSvr = itr.second();
            pSvr->SendEx(bytes, sizeof(bytes) , 0);
        }
    }
}

void UsocklListener::repldNotifyRole()
{
    UsockSvr *pSvr;
    SvrMap_t::iterator itr;
    int len;

    uint8_t *ptr = UsockSvr::getRoleData()->getRawData(len);
    ptr[0] = UsockConn::UXDW_REPLD_ROLE_UPD;
    for (itr = m_svrMap.begin(); itr != m_svrMap.end(); itr = m_svrMap.next(itr))
    {
        if (itr.second() != NULL )
        {
            pSvr = itr.second();
            pSvr->SendEx((char*)ptr, len , 0);
        }
    }
}
