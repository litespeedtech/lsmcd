
#include "usockmcd.h"

#include "lcreplconf.h"
#include "lcnodeinfo.h"
#include <lsmcd.h>
#include <edio/multiplexer.h>
#include <socket/coresocket.h>
#include <socket/gsockaddr.h>
#include "lcreplgroup.h"
#include <log4cxx/logger.h>
#include <util/hashstringmap.h>
#include "replshmhelper.h"
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <util/fdpass.h>
#include <memcache/lsmemcache.h>
#include "lcreplsender.h"

UsockClnt::UsockClnt()
    : m_bNotifyRepld(false)
    , m_clntMap(13, GHash::hash_fn(), GHash::val_comp())
{
}

UsockClnt::~UsockClnt()
{
    ClntMap_t::iterator itr;
    for (itr = m_clntMap.begin(); itr != m_clntMap.end(); itr = m_clntMap.next(itr))
    {
        if (itr.second() != NULL )
            delete itr.second();
    }

    if (m_pRoleData != NULL)
        delete m_pRoleData;
}

int UsockClnt::connectTo(const char *pAddr)
{
    int fd, ret;
    m_remoteAddr.set(AF_UNIX, pAddr, 0);
    
    Multiplexer * pMplex = getMultiplexer();
    ret = CoreSocket::connect( m_remoteAddr,
                               pMplex->getFLTag() , &fd, 1, &m_localAddr );
    if (( fd == -1 )&&( errno == ECONNREFUSED ))
    {
        ret = CoreSocket::connect( m_remoteAddr,
                                   pMplex->getFLTag(), &fd, 1, &m_localAddr );
    }
    
    if ( fd != -1 )
    {
//         int nodelay = 1;
//         ::setsockopt( fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof( int ) );
        ::fcntl( fd, F_SETFD, FD_CLOEXEC );
        setfd( fd );
        pMplex->add( this, POLLIN|POLLHUP|POLLERR );
        if ( ret == 0 )
        {
            m_iConnState = US_CONNECTED;
            //onWrite();
        }
        else
            m_iConnState = US_CONNECTING;
        
        return 0;
    }
    return -1;    
}

int UsockClnt::recvConnChange()
{
    uint8_t byte;
    Read( (char *)&byte, 1);
    m_bNotifyRepld = ( byte > 0);
    return 0;
}

int UsockClnt::recvRoleChange()
{
    int len;
    RoleTracker newRole( getReplConf()->getSliceCount());
    uint8_t *pBuf = newRole.getRawData(len);
    Read( (char *)(pBuf + 1), len -1);
    
    char pOldAddr[32], pNewAddr[32];
    for( int idx = 0; idx < getReplConf()->getSliceCount(); ++idx)
    {
        m_pRoleData->getRoleAddr(idx, pOldAddr, sizeof(pOldAddr));
        newRole.getRoleAddr(idx, pNewAddr, sizeof(pNewAddr));
        
        if (strcmp(pOldAddr, pNewAddr))//address changed
        {
            if (pNewAddr[0] != '\0')
            {
                LS_INFO("cached onNotified pid:%d, slice[%d] MASTER => SLAVE, pointing to master:%s"
                    , getpid(), idx, pNewAddr);
                LsMemcache::getInstance().setSliceDst(idx, pNewAddr);
                m_pRoleData->setRoleAddr(idx, pNewAddr, sizeof(pNewAddr) );    
            }
            else
            {   
                LS_INFO("cached onNotified pid:%d, slice[%d] SLAVE =>MASTER"
                    , getpid(), idx);
                LsMemcache::getInstance().setSliceDst(idx, NULL);
                m_pRoleData->setRoleAddr(idx, NULL, 0);
            }
        }
        else
        {
            LS_INFO("cached onNotified pid:%d, slice[%d] unchanged, isMaster:%d", 
                    getpid(), idx, pOldAddr[0] == '\0');            
        }
    }
    return 0;
}

int UsockClnt::onRead( )
{
    switch (m_proto)
    {
        case UX_NONE:
            Read((char *)&m_proto , 1);
            break;
            
        case UXDW_FD_PASSING:
            m_proto = UX_NONE;
            recvPassingFd();
            break;
            
        case UXDW_REPLD_ROLE_UPD:
            m_proto = UX_NONE;
            recvRoleChange();    
            break;
            
        case UXDW_REPLD_CONN_UPD:
            m_proto = UX_NONE;
            recvConnChange();    
            break;
    };
    return 0;
}

void UsockClnt::recycle()
{
    //delete this;
}

void UsockClnt::acceptRepldFwFd(uint8_t sliceId, int fd, sockaddr *pSockAddr)
{
    ClntMap_t::iterator itr;
    itr = m_clntMap.find( (void *) long (sliceId));
    if ( itr != m_clntMap.end() )
    {
        MemcacheConn *pConn = itr.second();
        m_clntMap.erase(itr);
        delete pConn;
    }

    MemcacheConn *pConn = new MemcacheConn();
    m_clntMap.insert( (void *) long (sliceId),  pConn);
    
    pConn->SetMultiplexer( getMultiplexer() );
    ::fcntl( fd, F_SETFL, getMultiplexer()->getFLTag());
    pConn->InitConn(fd, pSockAddr);
}

void UsockClnt::cachedNotifyData(uint8_t procId, uint8_t sliceId)
{
    if ( !m_bNotifyRepld )
        return ;
    char pBuf[32];
    assert (sizeof(pBuf) >= sizeof(DispatchData_t) + 1);
    
    pBuf[0] = UXUP_CACHED_DATA_UPD;
    pBuf[1] = procId;
    pBuf[2] = sliceId;
    SendEx(pBuf, sizeof(DispatchData_t) + 1, 0);
}

int UsockClnt::recvPassingFd()
{
    int fwFd, ret;
    DispatchData_t dispData; 
    ret = FDPass::read_fd(getfd(), &dispData, sizeof(DispatchData_t), &fwFd);
    LS_DBG_L("cached read_fd ret:%d, pid:%d, procId:%d received procId:%d, sliceId:%d  forwarded fd %d", 
            ret, getpid(), Lsmcd::getInstance().getProcId(), dispData.x_procId, dispData.x_sliceId, fwFd);
    if ( ret > 0)
    {
        if ( fwFd > 0)
        {
            acceptRepldFwFd(dispData.x_sliceId, fwFd, NULL);
            return 0;
        }
    }
    
    LS_ERROR("FDPass::read_fd read fails, ret:%d, error:%s", ret, strerror(errno));
    return -1;    
}

/////////////////////////////////////////////////////////////////////////
/////////////////////            UsockSvr           /////////////////////
/////////////////////////////////////////////////////////////////////////
TidTracker *UsockSvr::m_pTidTracker = NULL ;  
UsockSvr::UsockSvr()
{
}

UsockSvr::~UsockSvr()
{
    if (m_pTidTracker != NULL)
    {
        delete m_pTidTracker;
        m_pTidTracker = NULL;
    }
}

int UsockSvr::onRead( )
{
    switch (m_proto)
    {
        case UX_NONE:
            Read((char *)&m_proto , 1);
            break;
            
        case UXUP_CACHED_DATA_UPD:
            m_proto = UX_NONE;
            recvCachedNotify();
            break;
    };
    return 0;
}

int UsockSvr::svrInitConn( int fd, const AutoStr &remoteAddr  )
{
    m_remoteAddr.set(AF_UNIX, remoteAddr.c_str(), 0);
    setfd( fd );
    m_iConnState      = US_CONNECTED;
    return getMultiplexer()->add( this, POLLIN|POLLHUP|POLLERR );
}

int UsockSvr::recvCachedNotify()
{
    int ret;
    uint64_t tid;
    DispatchData_t dispData;
    ret = Read((char*)&dispData, sizeof(dispData));
    if (ret == sizeof(dispData) && m_pTidTracker != NULL)
    {
        tid = m_pTidTracker->getCurrTid(dispData.x_sliceId) ;
        LS_DBG_L("repld notified from cached procId:%d, sliceId:%d, currTid:%lld", 
                dispData.x_procId, dispData.x_sliceId, tid );

        tid = LcReplSender::getInstance().bcastReplicableData(dispData.x_sliceId, tid);
        m_pTidTracker->setCurrTid(dispData.x_sliceId, tid);
    }
    return 0;
}

int UsockSvr::sendPassingFd(const char *pData, int len, int fwFd)
{
    char type = UsockConn::UXDW_FD_PASSING;
    SendEx((char*)&type, 1, 0);
    return FDPass::write_fd(getfd(), pData, len, fwFd);
}

void UsockSvr::initTidTracker(int sliceCnt)
{
    if (m_pTidTracker == NULL)
        m_pTidTracker = new TidTracker(sliceCnt);
}

void UsockSvr::recycle()
{
    
    delete this;
}
