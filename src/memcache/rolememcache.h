/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */

#ifndef ROLETRACKER_H
#define ROLETRACKER_H


#include <util/autostr.h>
#include <util/hashstringmap.h>
#include <util/refcounter.h>
#include <util/gpointerlist.h>
#include <util/autobuf.h>
#include <util/tsingleton.h>
#include <shm/lsshmhash.h>
#include <shm/lsshmtypes.h>

#include <time.h>
#include <stdint.h>

class ReplProgressTrker;
class ExtWorker;
class LsShmPool;
class LsShmHash;
struct sockaddr;

class RoleData 
{
public:
    RoleData()
        : m_Idx( 0 )
        {}

    ~RoleData()      {}
private:
    int         m_Idx;
    AutoStr     m_addr;
};

typedef struct role_data_s
{
    uint32_t    x_idx;
    uint32_t    x_ip;
    uint16_t    x_port;    
    
}role_data_t;


class RoleMemCache : public TSingleton<RoleMemCache>
{
    friend class TSingleton<RoleMemCache>;
public: 
    RoleMemCache();
    ~RoleMemCache();

    int initShm(const char * pName);

    int readRoleViaShm(int idx, char *pAddr, int len) const;

    LsShmHash *getHash() const      {   return m_pShmCache;     }
    int addUpdateRole(int idx, const char *pAddr);
private:
    int initShmTables(LsShmPool *pPool, const char * pName);
private:
    LsShmHash   *m_pShmCache; 
};

#endif

