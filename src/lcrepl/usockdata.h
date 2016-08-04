
#ifndef __USOCKDATA_H
#define __USOCKDATA_H

#include <inttypes.h>

typedef struct
{
    uint8_t     x_procId;
    uint8_t     x_sliceId;
}DispatchData_t;

typedef struct
{
    uint32_t x_ip;
    uint16_t x_port;
}role_addr_t;

class RoleTracker
{
public:
    RoleTracker(int sliceCnt);
    virtual ~RoleTracker();
    void getRoleAddr(int idx, char *pAddr, int len) const;
    void setRoleAddr(int idx, const char * const pAddr, int len);
    uint8_t *getRawData(int &len) const;
    
private:
    int         m_sliceCnt;
    uint8_t     *m_pMem;
    int         m_nLen;
private:    
    RoleTracker & operator=(const RoleTracker & rhs);
};

class TidTracker
{
public:
    TidTracker(int sliceCnt);
    virtual ~TidTracker();
    void        setCurrTid(int idx, uint64_t currTid)     {   m_pTidArr[idx] = currTid;  }
    uint64_t    getCurrTid(int idx) const                 {   return m_pTidArr[idx];     }
private:
    int         m_nLen;
    uint64_t    *m_pTidArr;     
};

#endif
