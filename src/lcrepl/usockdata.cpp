
#include "usockdata.h"
#include "usockconn.h"
#include "lcreplconf.h"
#include "replshmhelper.h"
#include <log4cxx/logger.h>

RoleTracker::RoleTracker(int sliceCnt)
    : m_sliceCnt(sliceCnt)
{
    m_nLen = 1 + sizeof(role_addr_t) * sliceCnt;
    m_pMem = new uint8_t[ m_nLen ];
    
    m_pMem[0] = UsockConn::UX_NONE;
    memset(m_pMem + 1,  0xFF, m_nLen - 1); //make dirty
}

RoleTracker::~RoleTracker()
{
    delete [] m_pMem;
}

void RoleTracker::setRoleAddr(int idx, const char * const pAddr, int len)
{
    role_addr_t *pRole = (role_addr_t *)(m_pMem + 1);
    pRole += idx;
    if (pAddr == NULL) //master
    {
        pRole->x_ip     = 0;
        pRole->x_port   = 0;
    }
    else
    {
        GSockAddr sockAddr;
        sockAddr.set(pAddr, 0);
        pRole->x_ip         = uint32_t(((struct sockaddr_in *)sockAddr.get())->sin_addr.s_addr);
        pRole->x_port       = uint16_t(((struct sockaddr_in *)sockAddr.get())->sin_port);
    }
}

uint8_t *RoleTracker::getRawData(int &len) const
{
    len = m_nLen;
    return m_pMem;
}

void RoleTracker::getRoleAddr(int idx, char *pAddr, int len) const 
{
    role_addr_t *pRole = (role_addr_t *)(m_pMem + 1);
    pRole += idx;

    if (pRole->x_ip != 0 && pRole->x_port != 0)
    {
        struct in_addr ip_addr;
        ip_addr.s_addr = pRole->x_ip;
        snprintf(pAddr, len, "%s:%d", inet_ntoa(ip_addr), ntohs(pRole->x_port));
    }
    else
    {
        pAddr[0] = '\0';
    }
}

TidTracker::TidTracker(int sliceCnt)
    : m_nLen(sliceCnt)
    , m_pTidArr( new uint64_t[ sliceCnt ] )
{
    for (int idx = 0; idx < m_nLen; ++idx)
    {
        setCurrTid(idx, ReplShmHelper::getInstance().getLastShmTid(idx));
    }
}

TidTracker::~TidTracker()
{
    delete [] m_pTidArr;            
}
   
