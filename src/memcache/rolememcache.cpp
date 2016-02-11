/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */

#include "rolememcache.h"
#include <util/datetime.h>
#include <repl/replprgrsstrkr.h>
#include <log4cxx/logger.h>
#include <lsr/ls_atomic.h>
#include <lsr/xxhash.h>
#include <shm/lsshmhash.h>
#include <shm/lsshmhashobserver.h>
#include <util/stringtool.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdio.h>
#include <openssl/md5.h>
#include <socket/gsockaddr.h>

RoleMemCache::RoleMemCache()
{
}

RoleMemCache::~RoleMemCache()
{
}

int RoleMemCache::initShmTables(LsShmPool *pPool, const char * pName)
{

    m_pShmCache = pPool->getNamedHash(pName, 128, LsShmHash::hashXXH32,
                                         memcmp, LSSHM_FLAG_LRU);
    if (!m_pShmCache)
        return -1;
    m_pShmCache->disableLock();
    return 0;
}

int RoleMemCache::initShm(const char * pPath)
{
    LsShm *pShm;
    LsShmPool *pPool;
    const char * pFileName = "roletracker";
    int attempts;
    int ret = -1;
    LS_DBG_L("RoleMemCache::initShm path:%s", pPath);
    for (attempts = 0; attempts < 3; ++attempts)
    {
        pShm = LsShm::open(pFileName, 128, pPath);
        if (!pShm)
        {
            pShm = LsShm::open(pFileName, 128, pPath);
            if (!pShm)
            {
                LS_DBG_L("RoleMemCache::initShm return LS_FAIL");
                LsShm::logError("initShm::initShm() open SHM file");
                return LS_FAIL;
            }
        }

        pPool = pShm->getGlobalPool();
        if (!pPool)
        {
            pShm->deleteFile();
            pShm->close();
            continue;
        }

        pPool->disableLock();
        pPool->lock();

        if ((ret = initShmTables(pPool, pFileName)) == LS_FAIL)
        {
            pPool->unlock();
            pPool->close();
            pShm->deleteFile();
            pShm->close();
        }
        else
            break;
    }

    pPool->unlock();
    pPool->enableLock();
    LS_DBG_L("RoleMemCache::initShm return %d", ret); 
    return ret;

}

//return 0 if memory null, return 1 if memory is not null,  otherwise return -1
int RoleMemCache::readRoleViaShm(int idx, char *pAddr, int len) const
{
    role_data_t *pData;
    char pId[4];
    int valLen, ret;
    ret = -1;
    sprintf(pId, "%d", idx);
    if (!m_pShmCache)
        return ret;
    m_pShmCache->lock();
    LsShmOffset_t offVal = m_pShmCache->find( (const char *)pId, strlen(pId), &valLen);
    if (offVal != 0)
    {
        pData = (role_data_t *)m_pShmCache->offset2ptr(offVal);
        if (pData->x_port != 0)
        {
            struct in_addr ip_addr;
            ip_addr.s_addr = pData->x_ip;
            snprintf(pAddr, len, "%s:%d", inet_ntoa(ip_addr), ntohs(pData->x_port));
            LS_DBG_L("RoleMemCache::readRoleViaShm return slave, master addr:%s", pAddr);
            ret = 1;
        }
        else
        {
            LS_DBG_L("RoleMemCache::readRoleViaShm return master");
            ret = 0;
        }
    }
    m_pShmCache->unlock();
    return ret;
}

int RoleMemCache::addUpdateRole(int idx, const char *pAddr)
{
    char pId[4];
    snprintf(pId, 4, "%d", idx);
    int ret = 0;
    
    role_data_t *pData;
    int valLen = sizeof(*pData);
    int flag = LSSHM_VAL_NONE;
    ls_strpair_t parms;
    
    if (!m_pShmCache)
        return -1;
    m_pShmCache->lock();
    LsShmHIterOff offIter = m_pShmCache->getIterator(
        m_pShmCache->setParms(&parms, pId, strlen(pId), NULL, valLen), &flag);
    if (offIter.m_iOffset != 0)
    {
        pData = (role_data_t *)m_pShmCache->offset2iteratorData(offIter);
        if (pAddr == NULL)
        {
            pData->x_port       = 0;
            LS_DBG_L("RoleMemCache::addUpdateRole idx:%d, set port=0, role is master", idx);
        }
        else
        {
            GSockAddr sockAddr;
            sockAddr.set(pAddr, 0);
            pData->x_ip         = uint32_t(((struct sockaddr_in *)sockAddr.get())->sin_addr.s_addr);
            pData->x_port       = uint16_t(((struct sockaddr_in *)sockAddr.get())->sin_port);
            LS_DBG_L("RoleMemCache::addUpdateRole idx:%d, set ip:%d, port:%d, role is slave"
                , idx, pData->x_ip, pData->x_port);
            
        }
        ret = 1;
    }
    m_pShmCache->unlock();
    return ret;
}

