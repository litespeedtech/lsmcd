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
#include <memcache/lsmemcache.h>
#include <shm/lsshmtidmgr.h>
#include <lsmcd.h>
#include <log4cxx/logger.h>
#include <lcrepl/usockmcd.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <util/mysleep.h>
#include <limits.h>

struct LsMcUpdOpt
{
    uint16_t    m_iFlags;
    uint8_t     m_iRetcode;
    uint8_t     m_notused;
    void       *m_pRet;     // returned stuff
    void       *m_pMisc;    // miscellaneous stuff
    uint64_t    m_value;
    uint64_t    m_cas;
};

#define UPDRET_DONE         0
#define UPDRET_NOTFOUND     1
#define UPDRET_NONNUMERIC   2
#define UPDRET_EEXISTS      3
#define UPDRET_CASFAIL      UPDRET_EEXISTS
#define UPDRET_APPEND       10
#define UPDRET_PREPEND      11
#define UPDRET_NONE         20

typedef struct
{
    uint32_t    m_iSize;    // total packet size
    uint8_t     m_flags;
    uint8_t     m_type;
    uint16_t    m_iKeySz;
    uint64_t    m_tid;
} LsShmPktHdr;

// m_type
#define LSSHM_PKTADD    0x00
#define LSSHM_PKTDEL    0x01

typedef struct
{
    uint32_t    m_iValSz;
    uint32_t    m_hkey;     // hash key
    uint32_t    m_timestamp;
    uint8_t     m_data[0];
} LsShmTidAdd;

typedef struct
{
    uint64_t    m_tid;
} LsShmTidDel;

struct LsMcTidPkt
{
    LsShmPktHdr m_hdr;
    union
    {
        LsShmTidAdd     m_add;
        LsShmTidDel     m_del;
    };
    uint8_t         *getKey() const
    { return (uint8_t *)m_add.m_data; }
    uint8_t         *getVal() const
    { return (uint8_t *)m_add.m_data + m_hdr.m_iKeySz; }
};

typedef struct
{
    LsShmSize_t     x_iSize;
    LsShmOffset_t   x_iOff;
} LsMcTidInfoHelper;

#define ENDIAN_LITTLE

static uint64_t swap64(uint64_t val) {
#ifdef ENDIAN_LITTLE
    int64_t ret = 0;
    int i = 8;
    while (--i >= 0)
    {
        ret = (ret << 8) | (val & 0xff);
        val >>= 8;
    }
    return ret;
#else
    return val;
#endif
}

uint64_t ntohll(uint64_t val)
{
   return swap64(val);
}

uint64_t htonll(uint64_t val)
{
   return swap64(val);
}
 

char *LsMemcache::setUser(const char *user)
{
    LS_DBG_M("lsmemcache::setUser: %s\n", user);
    return m_pUser = strdup(user);
}


char *LsMemcache::getUser()
{
    return m_pUser;
}


int LsMemcache::notImplemented(LsMemcache *pThis, ls_strpair_t *pInput, int arg, 
                               MemcacheConn *pConn)
{
    pThis->respond("NOT_IMPLEMENTED" "\r\n", pConn);
    return 0;
}


//NOTICE: Any additions/changes need to be updated in the getCmdFunction definition.
LsMcCmdFunc LsMemcache::s_LsMcCmdFuncs[] =
{
    { "test1",      sizeof("test1")-1,      0,          doCmdTest1 },
    { "test2",      sizeof("test2")-1,      0,          doCmdTest2 },
    { "printtids",  sizeof("printtids")-1,  0,          doCmdPrintTids },
    { "get",        sizeof("get")-1,        MC_BINCMD_GET,          doCmdGet },
    { "bget",       sizeof("bget")-1,       MC_BINCMD_GET,          doCmdGet },
    { "gets",       sizeof("gets")-1,       MC_BINCMD_GET|LSMC_WITHCAS,  doCmdGet },
    { "add",        sizeof("add")-1,        MC_BINCMD_ADD,          doCmdUpdate },
    { "set",        sizeof("set")-1,        MC_BINCMD_SET,          doCmdUpdate },
    { "replace",    sizeof("replace")-1,    MC_BINCMD_REPLACE,      doCmdUpdate },
    { "append",     sizeof("append")-1,     MC_BINCMD_APPEND,       doCmdUpdate },
    { "prepend",    sizeof("prepend")-1,    MC_BINCMD_PREPEND,      doCmdUpdate },
    { "cas",        sizeof("cas")-1,        MC_BINCMD_REPLACE|LSMC_WITHCAS, doCmdUpdate },
    { "incr",       sizeof("incr")-1,       MC_BINCMD_INCREMENT,    doCmdArithmetic },
    { "decr",       sizeof("decr")-1,       MC_BINCMD_DECREMENT,    doCmdArithmetic },
    { "delete",     sizeof("delete")-1,     MC_BINCMD_DELETE,       doCmdDelete },
    { "touch",      sizeof("touch")-1,      MC_BINCMD_TOUCH,        doCmdTouch },
    { "stats",      sizeof("stats")-1,      0,              doCmdStats },
    { "flush_all",  sizeof("flush_all")-1,  0,              doCmdFlush },
    { "version",    sizeof("version")-1,    MC_BINCMD_VERSION,      doCmdVersion },
    { "quit",       sizeof("quit")-1,       MC_BINCMD_QUIT,         doCmdQuit },
    { "shutdown",   sizeof("shutdown")-1,   0,              notImplemented },
    { "verbosity",  sizeof("verbosity")-1,  MC_BINCMD_VERBOSITY,    doCmdVerbosity },
    { "clear",      sizeof("clear")-1,      0,    doCmdClear },
};

const char errorStr[] = "ERROR\r\n";
const char badCmdLineFmt[] = "CLIENT_ERROR bad command line format\r\n";

const char tokNoreply[] = "noreply";

static inline bool chkNoreply(char *tokPtr, int tokLen)
{
    return ((tokLen == sizeof(tokNoreply)-1)
        && (memcmp(tokPtr, tokNoreply, tokLen) == 0));
}


void LsMemcache::ackNoreply(MemcacheConn *pConn)
{
    int len;
    lenNbuf lenNbuf;
    len = sizeof(lenNbuf.len);  // no data, only ack
    lenNbuf.len = (uint16_t)htons(len);
    if (pConn->SendBuff((const char *)&lenNbuf, len, 0) != len)
    {
        LS_ERROR("SERVER_ERROR Unable to respond to client!\n");
    }
    return;
}


void LsMemcache::respond(const char *str, MemcacheConn *pConn)
{
    int len;
    lenNbuf lenNbuf;
    if (m_noreply)
    {
        if (getVerbose() > 1)
        {
            LS_INFO(">%d NOREPLY %s", pConn->getfd(), str);
        }
        if (pConn->GetConnFlags() & CS_INTERNAL)
        {
            ackNoreply(pConn);
        }
        m_noreply = false;
    }
    else
    {
        if (getVerbose() > 1)
        {
            LS_INFO(">%d %s", pConn->getfd(), str);
        }
        len = strlen(str);
        if (pConn->GetConnFlags() & CS_INTERNAL)
        {
            if ((unsigned int)len > sizeof(lenNbuf.buf))
            {
                str = "SERVER_ERROR Response too large\r\n";
                len = strlen(str);
                LS_ERROR("SERVER_ERROR Response too large!\n");
            }
            str = (const char *)buf2lenNbuf((char *)str, &len, &lenNbuf);
        }
        if (pConn->SendBuff(str, len, 0) != len)
        {
            LS_ERROR("SERVER_ERROR Unable to respond to client!\n");
        }
    }
    return;
}


void LsMemcache::sendResult(MemcacheConn *pConn, const char *fmt, ...)
{
    int len;
    char buf[4096];
    va_list va;
    if (pConn->GetConnFlags() & CS_INTERNAL)
        return;
    va_start(va, fmt);
    len = vsnprintf(buf, sizeof(buf), fmt, va);
    if (len >= (int)(sizeof(buf) - 1))
    {
        LS_ERROR("SERVER_ERROR Truncated result to client!\n");
    }
    va_end(va);
    if (pConn->appendOutput(buf, len) != len)
    {
        LS_ERROR("SERVER_ERROR Unable to send to client!\n");
    }
}


void LsMemcache::binRespond(uint8_t *buf, int cnt, MemcacheConn *pConn)
{
    if (m_noreply)
    {
        if (pConn->GetConnFlags() & CS_INTERNAL)
        {
            ackNoreply(pConn);
        }
        m_noreply = false;
    }
    else
    {
        if (pConn->GetConnFlags() & CS_INTERNAL)
        {
            uint16_t len = htons(cnt+2);
            pConn->appendOutput((char*)&len, 2);
        }
        if (pConn->appendOutput((char *)buf, cnt) != cnt)
        {
            LS_ERROR("SERVER_ERROR Unable to send to client!\n");
        }
    }
    return;
}


void LsMemcache::fwdCommand(LsMcHashSlice *pSlice, const char *buf, int len, 
                            MemcacheConn *pConn)
{
    if (getVerbose() > 1)
    {
        LS_INFO(">%d (fwd) %.*s", pSlice->m_pConn->getfd(),
                (isprint(*buf) ? len : 0), buf);
    }
    if (pSlice->m_pConn->SendBuff(buf, len, 0) != len)
        LS_ERROR("SERVER_ERROR Unable to fwd to remote!\n");
    else
    {
        pConn->SetRespWait();
        pSlice->m_pConn->SetLink(pConn);
        pSlice->m_pConn->SetRemoteBusy();
    }

    return;
}


int LsMemcache::processInternal(
    uint8_t *pBuf, int iLen, MemcacheConn *pConn)
{
    MemcacheConn *pLink;
    lenNbuf *ptr = (lenNbuf *)pBuf;
    LS_DBG_M("processInternal iLen1:%d, bufLen:%zd", iLen, sizeof(ptr->len));
    if (iLen < (int)sizeof(ptr->len))
        return -1;
    int len = ntohs(ptr->len);
    LS_DBG_M("processInternal iLen2:%d, orig Len:%d, conv Len:%d", iLen, 
             ptr->len, len);
    if (iLen < len)
        return -1;
    len -= sizeof(ptr->len);
    LS_DBG_M("INTERNAL: size=%d [%.*s]\n", len,
             (isprint(*ptr->buf) ? len : 0), ptr->buf);
    if ((pLink = pConn->GetLink()) == NULL)
    {
        LS_ERROR("No Link to send response to!\n");
    }
    else
    {
        LS_DBG_M("processInternal ClrRespWait addr:%p", pLink);
        pLink->ClrRespWait();
        if ((len > 0) && (pLink->SendBuff(ptr->buf, len, 0) != len))
            LS_ERROR("SERVER_ERROR Unable to respond to link!\n");
    }
    pConn->ClrRemoteBusy();
    m_noreply = false;
    return len + sizeof(ptr->len);
}


LsMemcache::LsMemcache()
    : m_pWaitQ(NULL)
    , m_pWaitTail(NULL)
    , m_pStrt(NULL)
    , m_iHdrOff(0)
    , m_hkey(0)
    , m_rescas(0)
    , m_needed(0)
    , m_retcode(0)
    , m_noreply(false)
    , m_keyPool()
    , m_keyList(10)
    , m_pUser(NULL)
{
    m_iterOff.m_iOffset = 0;
    ls_str_blank(&m_parms.key);
    ls_str_blank(&m_parms.val);
    m_mcparms.m_usecas = false;
    m_mcparms.m_usesasl = false;
    m_mcparms.m_nomemfail = false;
    m_mcparms.m_iValMaxSz = VAL_MAXSIZE;
    m_mcparms.m_iMemMaxSz = MEM_MAXSIZE;
}

static const char *g_pShmName = "SHMMCTEST";
static const char *g_pHashName = "SHMMCHASH";


void LsMemcache::notifyChange(MemcacheConn *pConn)
{
    LS_DBG_M("LsMemcache::notifyChange pid:%d, pHash:%p is %s, sliceIdx:%d", 
             getpid(), pConn->getHash(), 
             pConn->getHash()->isTidMaster() ? "master" : "slave", 
             m_pCurSlice->m_idx);
    if (pConn->getHash()->isTidMaster())
    {
        Lsmcd::getInstance().getUsockConn()->cachedNotifyData(
            Lsmcd::getInstance().getProcId(), m_pCurSlice->m_idx );
    }
}

int LsMemcache::multiInitFunc(LsMcHashSlice *pSlice, void *pArg)
{
    LsShmHash *pHash = pSlice->m_hashByUser.getHash(pSlice->m_hashByUser.
        getMemcache()->getUser());
    LsMcParms *pParms = (LsMcParms *)pArg;
    LsShmSize_t size;
    LsShmOffset_t off, iHelperOff;
    LsMcTidInfoHelper *pHelper;
    char *fileName = (char *)pHash->getPool()->getShm()->fileName();
    LsMcHdr *pHdr;
    int ret = LS_OK;
    LS_DBG_M("ShmSlice: [%s].\n", fileName);
    pHash->disableAutoLock();
    pHash->lockChkRehash();

    iHelperOff = pHash->getHTableReservedOffset();
    pHelper = (LsMcTidInfoHelper *)pHash->offset2ptr(iHelperOff);
    off = pHelper->x_iOff;
    if (off == 0)
    {
        int remapped;
        size = LsShmPool::size2roundSize(sizeof(*pHdr));
        if (pParms->m_usecas)
            size += sizeof(pHdr->x_data->cas);
        // if (mode & LSSHM_TID_MODE)
            size += sizeof(LsShmTidInfo);
        if ((off = pHash->alloc2(size, remapped)) != 0)
        {
            // if (mode & LSSHM_TID_MODE)
            {
                pHash->getTidMgr()->init(pHash,
                    off + size - sizeof(LsShmTidInfo), true);
            }
            pHdr = (LsMcHdr *)pHash->offset2ptr(off);
            pHdr->x_verbose = 0;
            pHdr->x_withcas = pParms->m_usecas;
            pHdr->x_withsasl = pParms->m_usesasl;
            memset((void *)&pHdr->x_dstaddr, 0, sizeof(pHdr->x_dstaddr));
            memset((void *)&pHdr->x_stats, 0, sizeof(pHdr->x_stats));
            if (pParms->m_usecas)
                pHdr->x_data->cas = 0;
            pHelper->x_iOff = off;
            pHelper->x_iSize = size;
        }
    }
    else
    {
        // if (mode & LSSHM_TID_MODE)
        {
            pHash->getTidMgr()->init(pHash,
                off + pHelper->x_iSize - sizeof(LsShmTidInfo), false);
        }
        pHdr = (LsMcHdr *)pHash->offset2ptr(off);
        if (pHdr->x_withcas != pParms->m_usecas)
        {
            LS_ERROR("MisMatch: existing database %sUSING cas.\n",
                     (pHdr->x_withcas ? "" : "NOT "));
            ret = LS_FAIL;
        }
        if (pHdr->x_withsasl != pParms->m_usesasl)
        {
            LS_ERROR("MisMatch: existing database %sUSING sasl.\n",
                     (pHdr->x_withsasl ? "" : "NOT "));
            ret = LS_FAIL;
        }
    }
    pHash->unlock();
    pHash->enableAutoLock();
    pSlice->m_iHdrOff = off;
    return ((off != 0) ? ret : LS_FAIL);
}


int LsMemcache::initMcShm(int iCnt, const char **ppPathName,
    const char *pHashName, LsMcParms *pParms)
{
    if (pHashName == NULL)
        pHashName = (char *)g_pHashName;

    int mode = (LSSHM_FLAG_TID|LSSHM_FLAG_LRU);//|LSSHM_FLAG_TID_SLAVE);
    m_pHashMulti = new LsMcHashMulti;
    if (m_pHashMulti->init(this, iCnt, ppPathName, pHashName,
        LsShmHash::hashXXH32, memcmp, mode, pParms->m_usesasl, 
        pParms->m_anonymous, pParms->m_byUser) != LS_OK)
    {
        delete m_pHashMulti;
        m_pHashMulti = NULL;
        return -1;
    }
    if (m_pHashMulti->foreach(multiInitFunc, (void *)pParms) == LS_FAIL)
    {
        delete m_pHashMulti;
        m_pHashMulti = NULL;
        return -1;
    }
    m_pCurSlice = m_pHashMulti->indx2hashSlice(0);
    m_iHdrOff = m_pCurSlice->m_iHdrOff;

    m_mcparms.m_usecas = pParms->m_usecas;
    m_mcparms.m_usesasl = pParms->m_usesasl;
    m_mcparms.m_anonymous = pParms->m_anonymous;
    m_mcparms.m_byUser = pParms->m_byUser;
    m_mcparms.m_nomemfail = pParms->m_nomemfail;
    if (pParms->m_iValMaxSz > 0)
        m_mcparms.m_iValMaxSz = pParms->m_iValMaxSz;
    if (pParms->m_iMemMaxSz > 0)
        m_mcparms.m_iMemMaxSz = pParms->m_iMemMaxSz;
#ifdef USE_SASL
    LS_DBG_M("SASL defined: %s\n",m_mcparms.m_usesasl ? "YES" : "NO");
    setVerbose(getVerbose());
    if (m_mcparms.m_usesasl && (LsMcSasl::initSasl() < 0))
    {
        LS_DBG_M("initSASL FAILED!\n");
        delete m_pHashMulti;
        m_pHashMulti = NULL;
        return -1;
    }
#endif

    return 0;
}


int LsMemcache::initMcShm(const char *pDirName, const char *pShmName,
    const char *pHashName, LsMcParms *pParms)
{
    char achDefaultDir[255];
    if (pDirName == NULL)
    {
        if ((pDirName = LsShm::detectDefaultRamdisk()) == NULL)
            pDirName = "/tmp";
        snprintf(achDefaultDir, sizeof(achDefaultDir), "%s/lsmcd", pDirName);
        pDirName = achDefaultDir;
        LsShm::addBaseDir(pDirName);
    }
    if (pShmName == NULL)
        pShmName = g_pShmName;
    if (pHashName == NULL)
        pHashName = g_pHashName;

    char achShmFileName[255];
    snprintf(achShmFileName, sizeof(achShmFileName), "%s/%s",
        pDirName, pShmName);
    pShmName = achShmFileName;
    return initMcShm(1, (const char **)&pShmName, pHashName, pParms);
}

int LsMemcache::multiMultiplexerFunc(LsMcHashSlice *pSlice, void *pArg)
{
    pSlice->m_pConn = new MemcacheConn();
    pSlice->m_pConn->SetMultiplexer((Multiplexer *)pArg);
    return LS_OK;
}

int LsMemcache::multiConnFunc(LsMcHashSlice *pSlice, void *pArg)
{
    pSlice->m_pConn->SetMultiplexer((Multiplexer *)pArg);
    return LS_OK;
}

int LsMemcache::reinitMcConn(Multiplexer *pMultiplexer)
{
    return m_pHashMulti->foreach(multiConnFunc, (void *)pMultiplexer);
}

int LsMemcache::initMcEvents()
{
    Multiplexer *pMultiplexer;
    if ((pMultiplexer = m_pHashMulti->getMultiplexer()) == NULL)
        return LS_FAIL;
    return m_pHashMulti->foreach(multiMultiplexerFunc, (void *)pMultiplexer);
}

int LsMemcache::setSliceDst(int which, char *pAddr)
{
    if ((m_pHashMulti == NULL) || (which >= m_pHashMulti->getMultiCnt()))
        return LS_FAIL;
    LsMcHashSlice *pSlice = m_pHashMulti->indx2hashSlice(which);
    LsShmHash *pHash = pSlice->m_hashByUser.getHash(getUser());
    uint8_t *pDstAddr =
        ((LsMcHdr *)pHash->offset2ptr(pSlice->m_iHdrOff))->x_dstaddr;
    if (*pDstAddr != 0)
    {
        LS_DBG_M("closing slice[%d] last svr addr [%s].\n", which, pDstAddr);
        pSlice->m_pConn->CloseConnection();
        *pDstAddr = 0;
    }
    pHash->setTidMaster();
    DispatchData_t dispData;
    dispData.x_procId   = Lsmcd::getInstance().getProcId();
    dispData.x_sliceId  = which;
    if (pAddr != NULL)
    {
        GSockAddr dstAddr;
        if ((dstAddr.set(pAddr, 0) < 0)
          || (dstAddr.toString((char *)pDstAddr, DSTADDR_MAXLEN) == NULL))
            return LS_FAIL;
        if (pSlice->m_pConn->connectTo(&dstAddr) < 0)
        {
            LS_ERROR("pid:%d, slice[%d] Unable to connect to [%s]!\n", getpid(), 
                     which, pAddr);
        }
        else
        {
            LS_DBG_M("pid:%d, slice[%d] Trying to connect to [%s].\n", getpid(), 
                     which, pAddr);
            
            if (pSlice->m_pConn->SendBuff((char *)&dispData, 
                sizeof(dispData), 0) != sizeof(dispData))
            {
                LS_ERROR("SERVER_ERROR Unable to connect fdpass listener!\n");
            }
            my_sleep(100);
            pSlice->m_pConn->SetConnInternal();
            uint8_t flag = MC_INTERNAL_REQ;
            if (pSlice->m_pConn->SendBuff((char *)&flag, 1, 0) != 1)
            {
                LS_ERROR("SERVER_ERROR Unable to init remote!\n");
            }
            else
            {
                LS_DBG_M("pid:%d, slice:%d, pConn Addr:%p, cached is connected "
                         "to peer master", getpid(), which, pSlice->m_pConn );
            }
                
                
        }
        pHash->setTidSlave();
    }
    
    LS_DBG_M("LsMemcache::setSliceDst pid:%d, procId:%d, pSlice->m_idx:%d, "
             "which:%d, pAddr:%s, result role:%d", getpid(), dispData.x_procId, 
             pSlice->m_idx, which, pAddr, pHash->isTidMaster());
    
    return LS_OK;
}


bool LsMemcache::isSliceRemote(LsMcHashSlice *pSlice)
{
    return (pSlice->m_pConn->GetConnState() == MemcacheConn::CS_CONNECTED);
}


bool LsMemcache::fwdToRemote(LsMcHashSlice *pSlice, char *pNxt, 
                             MemcacheConn *pConn)
{
    if (!pConn->getHash()->isTidMaster())
    {
        if (isSliceRemote(pSlice))
        {
            pNxt[-1] = '\n';
            fwdCommand(pSlice, m_pStrt, pNxt - m_pStrt, pConn);
        }
        else
        {
            LS_ERROR("Unable to forward to remote\n");
            respond("SERVER_ERROR unable to forward" "\r\n", pConn);
        }
        return true;
    }
    return false;
}


bool LsMemcache::fwdBinToRemote(LsMcHashSlice *pSlice, McBinCmdHdr *pHdr, 
                                MemcacheConn *pConn)
{
    if (!pConn->getHash()->isTidMaster())
    {
        if (isSliceRemote(pSlice))
        {
            fwdCommand(pSlice, (const char *)pHdr,
                sizeof(*pHdr) + ntohl(pHdr->totbody), pConn);
        }
        else
        {
            LS_ERROR("Unable to forward to remote\n");
            binErrRespond(pHdr, MC_BINSTAT_REMOTEERROR, pConn);
        }
        return true;
    }
    return false;
}


void LsMemcache::putWaitQ(MemcacheConn *pConn)
{
    if (pConn->GetConnFlags() & CS_CMDWAIT) // already on queue
        return;
    pConn->SetCmdWait();
    pConn->SetLink(NULL);
    if (m_pWaitQ == NULL)
        m_pWaitQ = pConn;
    else
        m_pWaitTail->SetLink(pConn);
    m_pWaitTail = pConn;
    return;
}


void LsMemcache::processWaitQ()
{
    MemcacheConn *pNext;
    MemcacheConn *pConn = m_pWaitQ;
    m_pWaitQ = NULL;
    m_pWaitTail = NULL;
    while (pConn != NULL)
    {
        pNext = pConn->GetLink();
        pConn->ClrCmdWait();
        pConn->SetLink(NULL);
        pConn->processIncoming();
        pConn = pNext;
    }
    return;
}


void LsMemcache::onTimer()
{
    static time_t lastTime = time(NULL);
    time_t curTime = time(NULL);
    if (lastTime + 30 < curTime)
    {
        m_keyPool.shrinkTo(10);
        lastTime = curTime;
    }
}


int LsMemcache::processCmd(char *pStr, int iLen, MemcacheConn *pConn)
{
    int datasz;
    int consumed;
    char *endp;
    char *pCmd;
    size_t len;
    LsMcCmdFunc *p;
    ls_strpair_t input;

    if (m_mcparms.m_usesasl)
    {
        const char *message = "ASCII messages will not be processed with SASL "
                              "enabled\n";
        LS_ERROR(message);
        respond(message, pConn);
        return 0; // To exit immediately
    }
    
    if ((endp = (char *)memchr(pStr, '\n', iLen)) == NULL)
        return -1;  // need more data
    consumed = endp - pStr + 1;
    *endp = '\0';
    if ((consumed > 1) && (endp[-1] == '\r'))
        endp[-1] = '\0';

    if (getVerbose() > 1)
    {
        LS_INFO("<%d %s\n", pConn->getfd(), pStr);
    }

    m_pStrt = pStr;     // save for possible fwd
    pStr = advToken(pStr, endp - 1, &pCmd, &len);
    input.key.ptr = pStr;
    if (len == 0)
        return consumed;
    if ((p = getCmdFunction(pCmd, len)) == NULL)
    {
        respond(errorStr, pConn);
        pConn->Flush();
        return consumed;
    }
    input.key.len = endp - input.key.ptr - 1;
    input.val.ptr = endp + 1;
    input.val.len = iLen - consumed;
    if ((datasz =
        (*p->func)(this, &input, p->arg, pConn)) >= 0)
//         (*p->func)(this, pStr, endp + 1, iLen - consumed, p->arg)) >= 0)
    {
        pConn->Flush();
        return consumed + datasz;
    }
    if (datasz == -1)   // need more data
    {
        *endp = '\n';   // restore for next time
        return -1;
    }
    return 0;   // else close connection
}


LsMcCmdFunc *LsMemcache::getCmdFunction(const char *pCmd, int len)
{
    LsMcCmdFunc *p;
    int iPossible, iOffset = 1;
    if ((pCmd == NULL) || (len < 3))
        return NULL;
    switch (*pCmd)
    {
    case 'b': // bget 
        iPossible = 4;
        break;
    case 'f': // flush_all 
        iPossible = 17;
        break;
    case 'i': // incr 
        iPossible = 12;
        break;
    case 'q': // quit 
        iPossible = 19;
        break;
    case 'r': // replace 
        iPossible = 8;
        break;
    case 'a': // add append 
        if ((*(pCmd + 1) == 'd'))
            iPossible = 6;
        else if ((*(pCmd + 1) == 'p'))
            iPossible = 9;
        else
            return NULL;
        iOffset = 2;
        break;
    case 'c': // cas clear
        if (*(pCmd + 1) == 'a')
            iPossible = 11;
        else if (*(pCmd + 1) == 'l')
            iPossible = 22;
        else
            return NULL;
        iOffset = 2;
        break;
    case 'd': // decr delete 
        if (*(pCmd + 1) != 'e')
            return NULL;
        else if (*(pCmd + 2) == 'c')
            iPossible = 13;
        else if (*(pCmd + 2) == 'l')
            iPossible = 14;
        else
            return NULL;
        iOffset = 3;
        break;
    case 'p': // printtids prepend 
        if (*(pCmd + 1) != 'r')
            return NULL;
        else if (*(pCmd + 2) == 'i')
            iPossible = 2;
        else if (*(pCmd + 2) == 'e')
            iPossible = 10;
        else
            return NULL;
        iOffset = 3;
        break;
    case 's': // set stats shutdown 
        switch (*(pCmd + 1))
        {
        case 'e':
            iPossible = 7;
            break;
        case 't':
            iPossible = 16;
            break;
        case 'h':
            iPossible = 20;
            break;
        default:
            return NULL;
        };
        iOffset = 2;
        break;
    case 'v': // version verbosity 
        if ((len < 7) || (memcmp("er", pCmd + 1, 2) != 0))
            return NULL;
        else if (pCmd[3] == 's')
            iPossible = 18;
        else if (pCmd[3] == 'b')
            iPossible = 21;
        else
            return NULL;
        iOffset = 4;
        break;
    case 't':  // test1 test2 touch 
        if ((*(pCmd + 1)) == 'o')
            iPossible = 15;
        else if ((len != 5) || (memcmp("est", pCmd + 1, 3) != 0))
            return NULL;
        else if (pCmd[4] == '1')
            return &s_LsMcCmdFuncs[0];
        else if (pCmd[4] == '2')
            return &s_LsMcCmdFuncs[1];
        else
            return NULL;
        iOffset = 2;
        break;
    case 'g': // get gets 
        if (memcmp("et", pCmd + 1, 2) == 0)
        {
            if (len == 3)
                return &s_LsMcCmdFuncs[3];
            else if ((len == 4) && (pCmd[3] == 's'))
                return &s_LsMcCmdFuncs[5];
        }
        // fall through
    default:
        return NULL;
    };
    p = &s_LsMcCmdFuncs[iPossible];
    if ((len == p->len)
        && (memcmp(pCmd + iOffset, p->cmd + iOffset, len - iOffset) == 0))
        return p;
    return NULL;
}


/*
 * this code needs the lock continued from the parsed command function.
 */
int LsMemcache::doDataUpdate(uint8_t *pBuf, MemcacheConn *pConn)
{
    LS_DBG_M("doDataUpdate, pConn: %p, key.len: %ld, offset: %d\n", pConn, 
             m_parms.key.len, m_iterOff.m_iOffset);
    if (m_parms.key.len <= 0)
    {
        unlock();
        respond("CLIENT_ERROR invalid data update" "\r\n", pConn);
        return -1;
    }
    m_parms.key.len = 0;

    if (m_iterOff.m_iOffset != 0)
    {
        dataItemUpdate(pBuf, pConn);
        unlock();
        notifyChange(pConn);
        if (m_retcode != UPDRET_NONE)
            respond("STORED" "\r\n", pConn);
        m_iterOff.m_iOffset = 0;
    }
    else
    {
        unlock();
        if (m_retcode == UPDRET_NOTFOUND)
            respond("NOT_FOUND" "\r\n", pConn);
        else if (m_retcode == UPDRET_CASFAIL)
            respond("EXISTS" "\r\n", pConn);
        else
            respond("NOT_STORED" "\r\n", pConn);
    }

    return 0;
}


void LsMemcache::dataItemUpdate(uint8_t *pBuf, MemcacheConn *pConn)
{
    int valLen;
    uint8_t *valPtr;
    LsShmHElem *iter = pConn->getHash()->offset2iterator(m_iterOff);
    LsMcDataItem *pItem = mcIter2data(iter, m_mcparms.m_usecas, &valPtr, 
                                      &valLen);
    LS_DBG_M("dataItemUpdate, pConn: %p, iter: %p, retcode: %d, valLen: %d\n",
             pConn, iter, m_retcode, valLen);
    if (m_retcode == UPDRET_APPEND)
    {
        valLen = m_parms.val.len;
        valPtr = iter->getVal() + iter->getValLen() - valLen;
    }
    else if (m_retcode == UPDRET_PREPEND)
        valLen = m_parms.val.len;
    else
    {
        if ((m_item.x_exptime != 0)
          && (m_item.x_exptime <= LSMC_MAXDELTATIME))
        {
            m_item.x_exptime += iter->getLruLasttime();
        }
        ::memcpy((void *)pItem, (void *)&m_item, sizeof(m_item));
    }
    if (m_mcparms.m_usecas)
        m_rescas = pItem->x_data->withcas.cas = getCas();
    if (valLen > 0)
        ::memcpy(valPtr, (void *)pBuf, valLen);
    m_needed = 0;
    return;
}


int LsMemcache::tidGetNxtItems(LsShmHash *pHash, uint64_t *pTidLast,
                                  uint8_t *pBuf, int iBufSz)
{
    int ret;
    uint8_t *pStrt = pBuf;
    int cnt = 0;
    uint64_t tidStrt = *pTidLast + 1;
    uint64_t tidEnd = 0;
    void *pBlk = NULL;
    int isAutoLock = pHash->isAutoLock();
    pHash->disableAutoLock();
    pHash->lockChkRehash();
    while ((ret = getNxtTidItem(pHash, pTidLast, &pBlk, (LsMcTidPkt *)pBuf,
                                iBufSz)) > 0)
    {
        tidEnd = *pTidLast;
        ++cnt;
        pBuf += ret;
        iBufSz -= ret;
    }
    if (ret >= 0)
        *pTidLast = tidEnd = pHash->getTidMgr()->getLastTid(); // might be a del
    pHash->unlock();
    if (isAutoLock)
        pHash->enableAutoLock();
    if ((cnt > 0) || (ret == 0))
    {
        LS_DBG_M("tidGetNxtItems: cnt=%d, [%llu-%llu].\n", cnt, 
                 (long long)tidStrt, (long long)tidEnd);
    }
    else
    {
        LS_DBG_M("tidGetNxtItems: Need larger buffer, next size=%d.\n", -ret);
    }
    int size;
    return (((size = pBuf - pStrt) > 0) ? size : ret);
}


int LsMemcache::getNxtTidItem(LsShmHash *pHash, uint64_t *pTidLast,
                                 void **ppBlk, LsMcTidPkt *pPkt, int iBufSz)
{
    uint64_t tid = *pTidLast;
    uint64_t *pVal;
    LsShmHIterOff iIterOff;
    int totSz;
    LsShmTidMgr *pTidMgr = pHash->getTidMgr();
    while (1)
    {
        if ((pVal = pTidMgr->nxtTidTblVal(&tid, ppBlk)) == NULL)
            return 0;
        if (pTidMgr->isTidValIterOff(*pVal))
        {
            iIterOff.m_iOffset = pTidMgr->tidVal2iterOff(*pVal);
            LsShmHElem *pElem = pHash->offset2iterator(iIterOff);
            if (isExpired((LsMcDataItem *)pElem->getVal()))
            {
                *ppBlk = NULL;  // block may be deleted or remapped on del tid
                LS_DBG_M("getNxtTidItem: expired (%llu)[%.*s]\n",
                         (long long)tid, pElem->getKeyLen(), 
                         (char *)pElem->getKey());
                pHash->eraseIterator(iIterOff);
                continue;
            }
            totSz = tidAddPktSize(pElem);
            if (totSz > iBufSz)
                return -totSz;  // not enough space
            iter2tidPack(pElem, &tid, pPkt, totSz);
        }
        else
        {
            if (*pVal == TIDDEL_FLUSHALL)
            {
                LS_DBG_M("getNxtTidItem: FLUSHALL tid=%llu.\n", (long long)tid);
            }
            totSz = tidDelPktSize();
            if (totSz > iBufSz)
                return -totSz;  // not enough space
            del2tidPack(pVal, &tid, pPkt, totSz);
        }
        break;
    }
    *pTidLast = tid;
    return totSz;
}


int LsMemcache::tidSetItems(LsShmHash *pHash, uint8_t *pBuf, int iBufSz)
{
    LsMcTidPkt *pPkt;
    uint8_t *pStrt = pBuf;
    int cnt = 0;
    uint64_t tidStrt = 0;
    uint64_t tidEnd = 0;
    int isAutoLock = pHash->isAutoLock();
    LsShmTidMgr *pTidMgr = pHash->getTidMgr();
    pHash->disableAutoLock();
    pHash->lockChkRehash();
    while (((unsigned int)iBufSz > (int)sizeof(LsShmPktHdr))
        && ((unsigned int)iBufSz >= ((LsShmPktHdr *)pBuf)->m_iSize))
    {
        pPkt = (LsMcTidPkt *)pBuf;
        if (pPkt->m_hdr.m_tid <= pTidMgr->getLastTid())
        {
            LS_ERROR(
                "Unable to insert Tid(%llu) out of sequence! last=(%llu).\n",
                (long long)pPkt->m_hdr.m_tid, (long long)pTidMgr->getLastTid());
            break;
        }
        if (pPkt->m_hdr.m_type == LSSHM_PKTADD)
        {
            if (setTidItem(pHash, pPkt) != LS_OK)
                break;
        }
        else if (delTidItem(pHash, pPkt, pBuf + pPkt->m_hdr.m_iSize,
                            iBufSz - pPkt->m_hdr.m_iSize) != LS_OK)
            break;
        tidEnd = ((LsMcTidPkt *)pBuf)->m_hdr.m_tid;
        if (tidStrt == 0)
            tidStrt = tidEnd;
        ++cnt;
        iBufSz -= ((LsMcTidPkt *)pBuf)->m_hdr.m_iSize;
        pBuf += ((LsMcTidPkt *)pBuf)->m_hdr.m_iSize;
    }
    pHash->unlock();
    if (isAutoLock)
        pHash->enableAutoLock();
    LS_DBG_M("tidSetItems: cnt=%d, [%llu-%llu].\n", cnt, (long long)tidStrt, 
             (long long)tidEnd);
    return (pBuf - pStrt);
}


int LsMemcache::setTidItem(LsShmHash *pHash, LsMcTidPkt *pPkt)
{
    LsShmOffset_t ret = pHash->getTidMgr()->setIter(pPkt->m_add.m_data,
        pPkt->m_hdr.m_iKeySz, pPkt->m_add.m_data + pPkt->m_hdr.m_iKeySz,
        pPkt->m_add.m_iValSz, &pPkt->m_hdr.m_tid);
    return ret;
}


int LsMemcache::delTidItem(LsShmHash *pHash, LsMcTidPkt *pPkt,
                              uint8_t *pBuf, int iBufSz)
{
    LsShmTidMgr *pTidMgr = pHash->getTidMgr();
    LsShmHIterOff off;
    LsShmTidTblBlk *pBlk = NULL;
    if (pPkt->m_del.m_tid == TIDDEL_FLUSHALL)
    {
        LS_DBG_M("setTidItem: FLUSHALL lastTid=%llu.\n",
                 (long long)pTidMgr->getLastTid());
        pHash->clear();
    }
    else
    {
        if ((off = pTidMgr->tid2iterOff(pPkt->m_del.m_tid, &pBlk)).m_iOffset != 0)
        {
            LsShmHElem *pElem = pHash->offset2iterator(off);
            LsMcTidPkt *pNxtPkt = (LsMcTidPkt *)pBuf;
            if (((unsigned int)iBufSz <= sizeof(LsShmPktHdr))
                || ((unsigned int)iBufSz < pNxtPkt->m_hdr.m_iSize)
                || (pNxtPkt->m_hdr.m_type != LSSHM_PKTADD)
                || (memcmp(pNxtPkt->m_add.m_data, pElem->getKey(),
                    pElem->getKeyLen()) != 0))
            {
                pTidMgr->delIter(off);
            }
        }
    }
    return pTidMgr->setTidTblDel(pPkt->m_del.m_tid, &pPkt->m_hdr.m_tid);
}

void LsMemcache::iter2tidPack(LsShmHElem *pElem, uint64_t *pTid,
                                 LsMcTidPkt *pPkt, int totSz)
{
    pPkt->m_hdr.m_iSize = totSz;
    pPkt->m_hdr.m_flags = 0;
    pPkt->m_hdr.m_type = LSSHM_PKTADD;
    pPkt->m_hdr.m_iKeySz = pElem->getKeyLen();
    pPkt->m_hdr.m_tid = *pTid;
    pPkt->m_add.m_iValSz = pElem->getValLen();
    pPkt->m_add.m_hkey = pElem->x_hkey;
    pPkt->m_add.m_timestamp = 0;
    ::memcpy(pPkt->getKey(), pElem->getKey(), pPkt->m_hdr.m_iKeySz);
    ::memcpy(pPkt->getVal(), pElem->getVal(), pPkt->m_add.m_iValSz);
    return;
}


void LsMemcache::del2tidPack(uint64_t *pVal, uint64_t *pTid,
                                LsMcTidPkt *pPkt, int totSz)
{
    pPkt->m_hdr.m_iSize = totSz;
    pPkt->m_hdr.m_flags = 0;
    pPkt->m_hdr.m_type = LSSHM_PKTDEL;
    pPkt->m_hdr.m_iKeySz = 0;
    pPkt->m_hdr.m_tid = *pTid;
    pPkt->m_del.m_tid = *pVal;
    return;
}


int LsMemcache::tidAddPktSize(LsShmHElem *pElem)
{
    return LsShmHash::round4(sizeof(LsShmPktHdr) + sizeof(LsShmTidAdd)
                                + pElem->getKeyLen() + pElem->getValLen());
}


int LsMemcache::tidDelPktSize()
{
    return LsShmHash::round4(sizeof(LsShmPktHdr) + sizeof(LsShmTidDel));
}


int LsMemcache::doCmdTest1(LsMemcache *pThis, ls_strpair_t *pInput, int arg, 
                           MemcacheConn *pConn)
{
    size_t tokLen;
    char *pIndx;
    uint64_t tid = 0;
    uint32_t indx = 0;
    pThis->advToken(pInput->key.ptr, pInput->key.ptr + pInput->key.len,
                    &pIndx, &tokLen);
    if (tokLen > 0)
    {
        pThis->myStrtoul(pIndx, &indx);
    }
    pThis->respond("TEST1" "\r\n", pConn);

    int cnt = 0;
    int size;
    uint8_t *p;
    LsMcTidPkt *pPkt;
    union
    {
        uint8_t data[2048];
        LsMcTidPkt x;
    } buf;
    int ret;
    while ((ret = tidGetNxtItems(pThis->getHash(indx), &tid, buf.data,
                                 sizeof(buf))) > 0)
    {
        fprintf(stdout, "tidGetNxtItems ret=%d\n", ret);
        p = buf.data;
        size = ret;
        do
        {
            pPkt = (LsMcTidPkt *)p;
            if (pPkt->m_hdr.m_type == LSSHM_PKTADD)
            {
                fprintf(stdout, 
                        "(%lu)ADD[%.*s]/[%.*s]\n",
                        pPkt->m_hdr.m_tid,
                        pPkt->m_hdr.m_iKeySz, 
                        (char *)pPkt->m_add.m_data,
                        (int)(pPkt->m_add.m_iValSz - sizeof(LsMcDataItem) - 
                            sizeof(uint64_t)),
                        (char *)pPkt->m_add.m_data + pPkt->m_hdr.m_iKeySz + 
                            sizeof(LsMcDataItem) + sizeof(uint64_t)
                );
            }
            else // if (pPkt->m_hdr.type == LSSHM_PKTDEL)
            {
                fprintf(stdout, 
                        "(%lu)DEL[%lu]\n",
                        pPkt->m_hdr.m_tid,
                        pPkt->m_del.m_tid);
            }

            p += pPkt->m_hdr.m_iSize;
            ret -= pPkt->m_hdr.m_iSize;
        }
        while (ret > 0);

        if (--cnt >= 0)
        {
            ret = tidSetItems(pConn->getHash(), buf.data, size);
            fprintf(stdout, "tidSetItems ret=%d\n", ret);
        }
    }

    return 0;
}


int LsMemcache::doCmdTest2(LsMemcache *pThis, ls_strpair_t *pInput, int arg, 
                           MemcacheConn *pConn)
{
    char *tokPtr;
    size_t tokLen;
    char *pVal;
    uint32_t which;
    char *pStr = pInput->key.ptr;
    char *pStrEnd = pStr + pInput->key.len;

    pStr = pThis->advToken(pStr, pStrEnd, &pVal, &tokLen);
    if ((tokLen <= 0) || !pThis->myStrtoul(pVal, &which))
    {
        pThis->respond("usage: test2 which [addr]" "\r\n", pConn);
        return 0;
    }
    pStr = pThis->advToken(pStr, pStrEnd, &tokPtr, &tokLen);
    if (tokLen <= 0)
        tokPtr = NULL;
    if (pThis->setSliceDst((int)which, tokPtr) != LS_OK)
        pThis->respond("TEST2 FAILED!" "\r\n", pConn);
    else
        pThis->respond("TEST2 O.K." "\r\n", pConn);

#ifdef notdef
    uint64_t tid = 0;
    int cnt = 0;
    while (pConn->getHash()->deqTidLst(&tid) == LS_OK)
    {
        if (tid & TIDLST_DELETE)
            fprintf(stdout, "deqTidLst DELETE tid=%llu\n", tid & ~TIDLST_DELETE);
        else
            fprintf(stdout, "deqTidLst NEW tid=%llu\n", tid);
        ++cnt;
    }
    fprintf(stdout, "deqTidLst cnt=%d\n", cnt);
#endif

    return 0;
}


int LsMemcache::doCmdPrintTids(LsMemcache *pThis, ls_strpair_t *pInput, int arg, 
                               MemcacheConn *pConn)
{
    const int maxOutBuf = 1024;
    char outBuf[maxOutBuf];
    size_t tokLen; 
    int curOutLen = 0;
    char *pIndx;
    uint64_t tid = 0;
    uint32_t indx = 0;
    pThis->advToken(pInput->key.ptr, pInput->key.ptr + pInput->key.len,
                    &pIndx, &tokLen);
    if (tokLen > 0)
    {
        pThis->myStrtoul(pIndx, &indx);
    }

    int cnt = 0;
    int size;
    uint8_t *p;
    LsMcTidPkt *pPkt;
    union
    {
        uint8_t data[2048];
        LsMcTidPkt x;
    } buf;
    int ret;
    while ((ret = tidGetNxtItems(pThis->getHash(indx), &tid, buf.data,
                                 sizeof(buf))) > 0)
    {
        p = buf.data;
        size = ret;
        do
        {
            pPkt = (LsMcTidPkt *)p;
            if (pPkt->m_hdr.m_type == LSSHM_PKTADD)
            {
                curOutLen +=
                    snprintf(outBuf + curOutLen, maxOutBuf - curOutLen,
                             "(%lu)ADD[%.*s]/[%.*s]\n",
                             pPkt->m_hdr.m_tid,
                             pPkt->m_hdr.m_iKeySz, (char *)pPkt->m_add.m_data,
                             (int)(pPkt->m_add.m_iValSz
                             - sizeof(LsMcDataItem) - sizeof(uint64_t)),
                             (char *)pPkt->m_add.m_data + pPkt->m_hdr.m_iKeySz
                             + sizeof(LsMcDataItem) + sizeof(uint64_t)
                    );
            }
            else // if (pPkt->m_hdr.type == LSSHM_PKTDEL)
            {
                curOutLen +=
                    snprintf(outBuf + curOutLen, maxOutBuf - curOutLen,
                             "(%lu)DEL[%lu]\n",
                                pPkt->m_hdr.m_tid,
                                pPkt->m_del.m_tid
                    );
            }

            p += pPkt->m_hdr.m_iSize;
            ret -= pPkt->m_hdr.m_iSize;
            if (curOutLen >= maxOutBuf)
            {
                curOutLen = -1;
                break;
            }
        }
        while (ret > 0);

        if (--cnt >= 0)
        {
            ret = tidSetItems(pConn->getHash(), buf.data, size);
        }
    }
    if (curOutLen == 0)
        pThis->respond("NO TIDS" "\r\n", pConn);
    else if (curOutLen == -1 )
        pThis->respond("TOO MUCH DATA TO RESPOND" "\r\n", pConn);
    else
    {
        outBuf[curOutLen] = '\0';
        pThis->respond(outBuf, pConn);
    }

    return 0;
}


void LsMemcache::clearKeyList()
{
    if (m_keyList.size() <= 0)
        return;

    m_keyPool.recycle((void **)m_keyList.begin(), m_keyList.size());
    m_keyList.clear();
}


int LsMemcache::doCmdGet(LsMemcache *pThis, ls_strpair_t *pInput, int arg, 
                         MemcacheConn *pConn)
{
    LsShmHash::iteroffset iterOff;
    LsShmHElem *iter;
    LsMcDataItem *pItem;
    int valLen;
    uint8_t *valPtr;
    pThis->m_noreply = false;
    char *pStr = pInput->key.ptr;
    char *pStrEnd = pStr + pInput->key.len;
    // memcached compatibility: check: if any key is bad, return no results.
    ls_str_t *pTok, **tokIter;
    // NOTICE: The list should never have any entries prior to this function.
    // If that is the case, there is an error somewhere.
    assert(pThis->m_keyList.size() == 0);
    while (1)
    {
        pTok = pThis->m_keyPool.get();
        pStr = pThis->advToken(pStr, pStrEnd, &pTok->ptr, &pTok->len);
        if ((pTok->len <= 0) || (pTok->len > KEY_MAXLEN))
        {
            pThis->m_keyPool.recycle(pTok);
            break;
        }
        pThis->m_keyList.push_back(pTok);
    }
    if (pTok->len > KEY_MAXLEN)
    {
        pThis->respond(badCmdLineFmt, pConn);
        pThis->clearKeyList();
        return 0;
    }
    if (pThis->m_keyList.size() == 0) // no keys
    {
        pThis->respond(errorStr, pConn);
        return 0;
    }
    tokIter = pThis->m_keyList.begin();
    for (; tokIter < pThis->m_keyList.end(); ++tokIter)
    {
        pTok = *tokIter;
        pThis->m_parms.key.ptr = pTok->ptr;
        pThis->m_parms.key.len = pTok->len;

        pThis->setSlice(pTok->ptr, pTok->len, pConn);
        pThis->lock();
        iterOff = pConn->getHash()->findIteratorWithKey(pThis->m_hkey,
                                                        &pThis->m_parms);
        if (iterOff.m_iOffset != 0)
        {
            iter = pConn->getHash()->offset2iterator(iterOff);
            pItem = mcIter2data(iter, pThis->m_mcparms.m_usecas, &valPtr, 
                                &valLen);
            if (pThis->isExpired(pItem))
            {
                pConn->getHash()->eraseIterator(iterOff);
                iter = pConn->getHash()->offset2iterator(iterOff); // remap?
                pThis->statGetMiss();
            }
            else if (arg & LSMC_WITHCAS)
            {
                pThis->statGetHit();
                pThis->sendResult(
                    pConn,
                    "VALUE %.*s %d %d %llu\r\n",
                    iter->getKeyLen(), iter->getKey(),
                    pItem->x_flags,
                    valLen,
                    (pThis->m_mcparms.m_usecas ? pItem->x_data->withcas.cas : 
                                                 (uint64_t)0)
                );
                pThis->binRespond(valPtr, valLen, pConn);  // data binary
                pThis->binRespond((uint8_t *)"\r\n", 2, pConn);
            }
            else
            {
                pThis->statGetHit();
                pThis->sendResult(
                    pConn,
                    "VALUE %.*s %d %d\r\n",
                    iter->getKeyLen(), iter->getKey(),
                    pItem->x_flags,
                    valLen
                );
                pThis->binRespond(valPtr, valLen, pConn);  // data binary
                pThis->binRespond((uint8_t *)"\r\n", 2, pConn);
            }
        }
        else
            pThis->statGetMiss();
        pThis->unlock();
    }
    pThis->clearKeyList();
    if (pThis->getVerbose() > 1)
    {
        LS_INFO(">%d END\n", pConn->getfd());
    }
    pThis->sendResult(pConn, "END\r\n");
    return 0;
}


LsShmHash::iteroffset LsMemcache::doHashInsert(ls_strpair_t *pParms,
                                               LsMcUpdOpt *pOpt,
                                               MemcacheConn *pConn)
{
    LsShmHash::iteroffset iterOff = pConn->getHash()->
        findIteratorWithKey(m_hkey, pParms);
    if (iterOff.m_iOffset != 0)
    {
        if (LsMemcache::isExpired((LsMcDataItem *)pConn->getHash()->
            offset2iteratorData(iterOff)))
        {
            pOpt->m_iRetcode = UPDRET_DONE;
            iterOff = pConn->getHash()->doSet(iterOff, m_hkey, pParms);
        }
        else
        {
            pOpt->m_iRetcode = UPDRET_EEXISTS;
            iterOff.m_iOffset = 0;
        }
        return iterOff;
    }
    return pConn->getHash()->doInsert(iterOff, m_hkey, pParms);
}


LsShmHash::iteroffset LsMemcache::doHashUpdate(ls_strpair_t *pParms,
                                               LsMcUpdOpt *pOpt, 
                                               MemcacheConn *pConn)
{
    LsShmHash::iteroffset iterOff = pConn->getHash()->
        findIteratorWithKey(m_hkey, pParms);
    if (iterOff.m_iOffset == 0)
    {
        pOpt->m_iRetcode = UPDRET_NOTFOUND;
        return iterOff;
    }
    LsShmHElem *iter = pConn->getHash()->offset2iterator(iterOff);
    LsMcDataItem *pItem = (LsMcDataItem *)iter->getVal();
    if (LsMemcache::isExpired(pItem))
    {
        pConn->getHash()->eraseIterator(iterOff);
        pOpt->m_iRetcode = UPDRET_NOTFOUND;
        return pConn->getHash()->end();
    }
    if (((pOpt->m_iFlags & LSMC_WITHCAS) || (pOpt->m_cas != 0))
        && (pItem->x_data->withcas.cas != pOpt->m_cas))
    {
        pOpt->m_iRetcode = UPDRET_CASFAIL;
        return pConn->getHash()->end();
    }

    int cmd = (pOpt->m_iFlags & LSMC_CMDMASK);
    if (cmd == MC_BINCMD_REPLACE)
    {
        pOpt->m_iRetcode = UPDRET_DONE;
    }
    else if ((cmd == MC_BINCMD_INCREMENT) || (cmd == MC_BINCMD_DECREMENT))
    {
        uint64_t num;
        pItem = LsMemcache::mcIter2num(iter,
            pOpt->m_iFlags & LSMC_USECAS, (char *)pOpt->m_pMisc, &num);
        if (pItem == NULL)
        {
            pOpt->m_iRetcode = UPDRET_NONNUMERIC;
            return pConn->getHash()->end();
        }
        if (cmd == MC_BINCMD_INCREMENT)
        {
            num += pOpt->m_value;
        }
        else /* if (cmd == MC_BINCMD_DECREMENT) */
        {
            if (pOpt->m_value > num)
                num = 0;
            else
                num -= pOpt->m_value;
        }
        pParms->val.len += snprintf(
            (char *)pOpt->m_pMisc, ULL_MAXLEN+1, "%llu", (unsigned long long)num);
        *((LsMcDataItem *)pOpt->m_pRet) = *pItem;
        //FIXME: may need more code to update the value

    }
    else if ((cmd == MC_BINCMD_APPEND) || (cmd == MC_BINCMD_PREPEND))
    {
        uint8_t *valPtr;
        int valLen;
        int lenExp = ls_str_len(&pParms->val);
        iterOff = pConn->getHash()->iterGrowValue(iterOff, lenExp, 0);
        if (iterOff.m_iOffset == 0 || cmd == MC_BINCMD_APPEND)
            return iterOff;
        iter = pConn->getHash()->offset2iterator(iterOff);
        mcIter2data(iter, pOpt->m_iFlags & LSMC_USECAS, &valPtr, &valLen);
        ::memmove(valPtr + lenExp, valPtr, valLen - lenExp);
        return iterOff;
    }


    return pConn->getHash()->doUpdate(iterOff, m_hkey, pParms);
}


int LsMemcache::doCmdUpdate(LsMemcache *pThis, ls_strpair_t *pInput, int arg, 
                            MemcacheConn *pConn)
{
    char *tokPtr;
    size_t tokLen;
    char *pFlags;
    char *pExptime;
    char *pLength;
    char *pCas;
    uint32_t flags;
    uint32_t exptime;
    int32_t length;
    uint64_t cas;
    bool badcmdline;
    LsMcUpdOpt updOpt;
    McBinStat ret;
    LsMcHashSlice *pSlice;
    char *pStr = pInput->key.ptr;
    char *pStrEnd = pStr + pInput->key.len;
    char *pNxt = pInput->val.ptr;
    int iLen = pInput->val.len;

    pStr = pThis->advToken(pStr, pStrEnd, &pThis->m_parms.key.ptr,
                           &pThis->m_parms.key.len);
    if ((pSlice = pThis->canProcessNow(
        pThis->m_parms.key.ptr, pThis->m_parms.key.len, pConn)) == NULL)
    {
        return -1;  // cannot process now
    }

    pStr = pThis->advToken(pStr, pStrEnd, &pFlags, &tokLen);
    pStr = pThis->advToken(pStr, pStrEnd, &pExptime, &tokLen);
    pStr = pThis->advToken(pStr, pStrEnd, &pLength, &tokLen);
    badcmdline = ((tokLen <= 0)
        || (!pThis->myStrtoul(pFlags, &flags))
        || (!pThis->myStrtoul(pExptime, &exptime))
        || (!pThis->myStrtol(pLength, &length))
        || (pThis->m_parms.key.len > KEY_MAXLEN));
    if (arg & LSMC_WITHCAS)
    {
        pStr = pThis->advToken(pStr, pStrEnd, &pCas, &tokLen);
        if ((tokLen <= 0) || (!pThis->myStrtoull(pCas, &cas)))
            badcmdline = true;
        else
            updOpt.m_cas = (uint64_t)cas;
    }
    else
    {
        updOpt.m_cas = 0;
    }
    pStr = pThis->advToken(pStr, pStrEnd, &tokPtr, &tokLen);     // opt noreply
    pThis->m_noreply = chkNoreply(tokPtr, tokLen);
    if (badcmdline || (length < 0) || (length > (INT_MAX - 2)))   // watch wrap!
    {
        pThis->respond(badCmdLineFmt, pConn);
        return 0;
    }

    if (length != 0)
    {
        if (iLen < length + 2)  // terminating "\r\n"
            return -1;  // need more data
        if ((pNxt[length] != '\r') || (pNxt[length + 1] != '\n'))
        {
            LS_ERROR("CLIENT_ERROR bad data chunk\n");
            return 0;
        }
    }
    if (!pConn->getHash()->isTidMaster())
    {
        if (length != 0)
            length += 2;
        pNxt[-1] = '\n';
        if (pThis->isSliceRemote(pSlice))
        {
            pThis->fwdCommand(pSlice, pThis->m_pStrt, 
                              pNxt - pThis->m_pStrt + length, pConn);
        }
        else
        {
            LS_ERROR("Unable to forward to remote\n");
            pThis->respond("SERVER_ERROR unable to forward" "\r\n", pConn);
        }
        LS_DBG_M("is not master, forwarding , return length:%d", length);
        return length;
    }

    pThis->m_item.x_flags = (uint32_t)flags;
    pThis->m_item.x_exptime = (time_t)exptime;
    pThis->m_parms.val.ptr = NULL;
    pThis->m_parms.val.len = length;   // adjusted later
    updOpt.m_iFlags = arg;
    if ((arg == MC_BINCMD_APPEND) || (arg == MC_BINCMD_PREPEND))
    {
        if (pThis->m_mcparms.m_usecas)
            updOpt.m_iFlags |= LSMC_USECAS;
    }
    else
    {
        pThis->m_parms.val.len = pThis->parmAdjLen(pThis->m_parms.val.len);
    }

    pThis->lock();
    if ((ret = pThis->chkMemSz(arg)) != MC_BINSTAT_SUCCESS)
    {
        pThis->unlock();
        if (ret == MC_BINSTAT_E2BIG)
            pThis->respond("SERVER_ERROR object too large for cache" "\r\n", pConn);
        else /* if (ret == MC_BINSTAT_ENOMEM) */
            pThis->respond("SERVER_ERROR out of memory storing object" "\r\n", pConn);
        return (length != 0) ? (length + 2) : 0;
    }

    pThis->statSetCmd();
    switch (arg)
    {
        case MC_BINCMD_ADD:
            pThis->m_iterOff = pThis->doHashInsert(&pThis->m_parms, &updOpt, 
                                                   pConn);
            pThis->m_retcode = UPDRET_DONE;
            break;
        case MC_BINCMD_SET:
            pThis->m_iterOff = pConn->getHash()->setIteratorWithKey(pThis->m_hkey,
                                                                    &pThis->m_parms);
            pThis->m_retcode = UPDRET_DONE;
            break;
        case MC_BINCMD_REPLACE:
            pThis->m_iterOff = pThis->doHashUpdate(&pThis->m_parms, &updOpt,
                                                   pConn);
            pThis->m_retcode = UPDRET_DONE;
            break;
        case MC_BINCMD_APPEND:
            pThis->m_iterOff = pThis->doHashUpdate(&pThis->m_parms, &updOpt,
                                                   pConn);
            pThis->m_retcode = UPDRET_APPEND;
            break;
        case MC_BINCMD_PREPEND:
            pThis->m_iterOff = pThis->doHashUpdate(&pThis->m_parms, &updOpt,
                                                   pConn);
            pThis->m_retcode = UPDRET_PREPEND;
            break;
        case MC_BINCMD_REPLACE|LSMC_WITHCAS:
            pThis->m_iterOff = pThis->doHashUpdate(&pThis->m_parms, &updOpt, 
                                                   pConn);
            pThis->m_retcode = updOpt.m_iRetcode;
            if (pThis->m_retcode == UPDRET_NOTFOUND)
                pThis->statCasMiss();
            else if (pThis->m_retcode == UPDRET_CASFAIL)
                pThis->statCasBad();
            else
                pThis->statCasHit();
            break;
        default:
            pThis->unlock();
            pThis->respond("SERVER_ERROR unhandled type" "\r\n", pConn);
            return 0;
    }
    // unlock after data is updated
    pThis->m_needed = length;
    pThis->doDataUpdate((uint8_t *)pNxt, pConn);
    return (length != 0) ? (length + 2) : 0;
}


McBinStat LsMemcache::chkMemSz(int arg)
{
    if (!chkItemSize(m_parms.val.len))
    {
        // memcached compatibility - remove `stale' data
        LsShmHash::iteroffset iterOff;
        if (arg == MC_BINCMD_SET)
        {
            if ((iterOff = m_pCurSlice->m_hashByUser.getHash(getUser())->
                findIteratorWithKey(m_hkey,&m_parms)).m_iOffset != 0)
                m_pCurSlice->m_hashByUser.getHash(getUser())->
                eraseIterator(iterOff);
        }
        return MC_BINSTAT_E2BIG;
    }
    // size here is conservative approximate,
    // since we do not ascertain the size of the possible existing item
    LsShmXSize_t more;
    LsShmXSize_t total;
    if ((arg == MC_BINCMD_ADD) || (arg == MC_BINCMD_SET))
    {
        more = LsShmPool::size2roundSize(
            + m_pCurSlice->m_hashByUser.getHash(getUser())->
                round4(m_parms.key.len)
            + sizeof(ls_vardata_t)
            + m_pCurSlice->m_hashByUser.getHash(getUser())->
                round4(m_parms.val.len)
            + sizeof(ls_vardata_t)
            + sizeof(LsShmHElem)
            + m_pCurSlice->m_hashByUser.getHash(getUser())->getExtraTotal()
        );
    }
    else
    {
        more = LsShmPool::size2roundSize(m_parms.val.len);   // additional
    }

    total = m_pCurSlice->m_hashByUser.getHash(getUser())->getHashDataSize() + 
            more;
    LsShmOffset_t helperOff = m_pCurSlice->m_hashByUser.getHash(getUser())->
        getHTableReservedOffset();
    LsMcTidInfoHelper *pHelper = (LsMcTidInfoHelper *)m_pCurSlice->m_hashByUser.
        getHash(getUser())->offset2ptr(helperOff);
    total -= pHelper->x_iSize;
    if (total > m_mcparms.m_iMemMaxSz)
    {
        if (m_mcparms.m_nomemfail)
            return MC_BINSTAT_ENOMEM;
        m_pCurSlice->m_hashByUser.getHash(getUser())->
            trimsize((int)(total - m_mcparms.m_iMemMaxSz)<<3, NULL, 0);
    }
    return MC_BINSTAT_SUCCESS;
}


int LsMemcache::doCmdArithmetic(LsMemcache *pThis, ls_strpair_t *pInput,
                                int arg, MemcacheConn *pConn)
{
    char *tokPtr;
    size_t tokLen;
    char *pDelta;
    uint64_t delta;
    LsMcUpdOpt updOpt;
    char numBuf[ULL_MAXLEN+2+1];
    LsMcHashSlice *pSlice;
    char *pStr = pInput->key.ptr;
    char *pStrEnd = pStr + pInput->key.len;
    char *pNxt = pInput->val.ptr;

    pStr = pThis->advToken(pStr, pStrEnd, &pThis->m_parms.key.ptr,
                           &pThis->m_parms.key.len);
    if ((pSlice = pThis->canProcessNow(
        pThis->m_parms.key.ptr, pThis->m_parms.key.len, pConn)) == NULL)
    {
        return -1;  // cannot process now
    }

    pStr = pThis->advToken(pStr, pStrEnd, &pDelta, &tokLen);
    if ((tokLen <= 0) || (pThis->m_parms.key.len > KEY_MAXLEN))
    {
        pThis->respond(badCmdLineFmt, pConn);
        return 0;
    }
    if (!pThis->myStrtoull(pDelta, &delta))
    {
        pThis->respond("CLIENT_ERROR invalid numeric delta argument" "\r\n", pConn);
        return 0;
    }

    if (pThis->fwdToRemote(pSlice, pNxt, pConn))
        return 0;

    pStr = pThis->advToken(pStr, pStrEnd, &tokPtr, &tokLen);    // opt noreply
    pThis->m_noreply = chkNoreply(tokPtr, tokLen);
    pThis->m_parms.val.ptr = NULL;
    pThis->m_parms.val.len = pThis->parmAdjLen(0);
    updOpt.m_iFlags = arg;
    updOpt.m_value = (uint64_t)delta;
    updOpt.m_cas = (uint64_t)0;
    updOpt.m_pRet = (void *)&pThis->m_item;
    updOpt.m_pMisc = (void *)numBuf;
    if (pThis->m_mcparms.m_usecas)
        updOpt.m_iFlags |= LSMC_USECAS;

    pThis->lock();
    if ((pThis->m_iterOff =
        pThis->doHashUpdate(&pThis->m_parms, &updOpt, pConn)).m_iOffset != 0)
    {
        if (arg == MC_BINCMD_INCREMENT)
            pThis->statIncrHit();
        else
            pThis->statDecrHit();
        pThis->m_retcode = UPDRET_NONE;
        pThis->doDataUpdate((uint8_t *)numBuf, pConn);
        pThis->respond(strcat(numBuf, "\r\n"), pConn);
    }
    else if (updOpt.m_iRetcode == UPDRET_NOTFOUND)
    {
        if (arg == MC_BINCMD_INCREMENT)
            pThis->statIncrMiss();
        else
            pThis->statDecrMiss();
        pThis->unlock();
        pThis->respond("NOT_FOUND" "\r\n", pConn);
    }
    else
    {
        pThis->unlock();
        if (updOpt.m_iRetcode == UPDRET_NONNUMERIC)
            pThis->respond(
              "CLIENT_ERROR cannot increment or decrement non-numeric value" 
              "\r\n", pConn);
        else
            pThis->respond("SERVER_ERROR unable to update" "\r\n", pConn);
    }
    return 0;
}


int LsMemcache::doCmdDelete(LsMemcache *pThis, ls_strpair_t *pInput, int arg, 
                            MemcacheConn *pConn)
{
    char *tokPtr;
    size_t tokLen;
    LsShmHash::iteroffset iterOff;
    bool expired;
    LsMcHashSlice *pSlice;
    char *pStr = pInput->key.ptr;
    char *pStrEnd = pStr + pInput->key.len;
    char *pNxt = pInput->val.ptr;
    pStr = pThis->advToken(pStr, pStrEnd, &pThis->m_parms.key.ptr,
                           &pThis->m_parms.key.len);
    if ((pThis->m_parms.key.len <= 0)
        || (pThis->m_parms.key.len > KEY_MAXLEN))
    {
        pThis->respond(badCmdLineFmt, pConn);
        return 0;
    }
    if ((pSlice = pThis->canProcessNow(
        pThis->m_parms.key.ptr, pThis->m_parms.key.len, pConn)) == NULL)
    {
        return -1;  // cannot process now
    }

    pStr = pThis->advToken(pStr, pStrEnd, &tokPtr, &tokLen);     // optional
    if ((tokLen == 1) && (tokPtr[0] == '0'))            // hold_is_zero???
        pStr = pThis->advToken(pStr, pStrEnd, &tokPtr, &tokLen);
    if (tokLen > 0)
    {
        bool err = false;
        pThis->m_noreply = chkNoreply(tokPtr, tokLen);
        pStr = pThis->advToken(pStr, pStrEnd, &tokPtr, &tokLen);
        if (pThis->m_noreply == false)
        {
            err = true;
            if ((pThis->m_noreply = chkNoreply(tokPtr, tokLen)) == true)
                pThis->advToken(pStr, pStrEnd, &tokPtr, &tokLen);
        }
        if (tokLen > 0)
        {
            err = true;
            pThis->m_noreply = false;       // memcached compatibility
        }
        if (err)
        {
            pThis->respond("CLIENT_ERROR bad delete format" "\r\n", pConn);
            return 0;
        }
    }

    if (pThis->fwdToRemote(pSlice, pNxt, pConn))
        return 0;

    pThis->lock();
    if ((iterOff = pConn->getHash()->
        findIteratorWithKey(pThis->m_hkey, &pThis->m_parms)).m_iOffset != 0)
    {
        expired = pThis->isExpired((LsMcDataItem *)pConn->getHash()->
            offset2iteratorData(iterOff));
        pConn->getHash()->eraseIterator(iterOff);
    }
    if ((iterOff.m_iOffset != 0) && !expired)
    {
        pThis->statDeleteHit();
        pThis->unlock();
        pThis->notifyChange(pConn);
        pThis->respond("DELETED" "\r\n", pConn);
    }
    else
    {
        pThis->statDeleteMiss();
        pThis->unlock();
        pThis->respond("NOT_FOUND" "\r\n", pConn);
    }
    return 0;
}


int LsMemcache::doCmdTouch(LsMemcache *pThis, ls_strpair_t *pInput, int arg, 
                           MemcacheConn *pConn)
{
    char *tokPtr;
    size_t tokLen;
    char *pExptime;
    uint32_t exptime;
    LsMcHashSlice *pSlice;
    LsShmHash::iteroffset iterOff;
    char *pStr = pInput->key.ptr;
    char *pStrEnd = pStr + pInput->key.len;
    char *pNxt = pInput->val.ptr;
    pStr = pThis->advToken(pStr, pStrEnd, &pThis->m_parms.key.ptr,
                           &pThis->m_parms.key.len);
    pStr = pThis->advToken(pStr, pStrEnd, &pExptime, &tokLen);
    if ((tokLen <= 0) || (pThis->m_parms.key.len > KEY_MAXLEN))
    {
        pThis->respond(badCmdLineFmt, pConn);
        return 0;
    }
    if ((pSlice = pThis->canProcessNow(
        pThis->m_parms.key.ptr, pThis->m_parms.key.len, pConn)) == NULL)
    {
        return -1;  // cannot process now
    }

    if (!pThis->myStrtoul(pExptime, &exptime))
    {
        pThis->respond("CLIENT_ERROR invalid exptime argument" "\r\n", pConn);
        return 0;
    }
    pStr = pThis->advToken(pStr, pStrEnd, &tokPtr, &tokLen);    // opt noreply
    pThis->m_noreply = chkNoreply(tokPtr, tokLen);

    if (pThis->fwdToRemote(pSlice, pNxt, pConn))
        return 0;

    pThis->lock();
    if ((iterOff = pConn->getHash()->findIteratorWithKey(
        pThis->m_hkey, &pThis->m_parms)).m_iOffset != 0)
    {
        LsShmHElem *iter = pConn->getHash()->offset2iterator(iterOff);
        LsMcDataItem *pItem = (LsMcDataItem *)iter->getVal();
        if (pThis->isExpired(pItem))
            pConn->getHash()->eraseIterator(iterOff);
        else
        {
            setItemExptime(pItem, (uint32_t)exptime);
            pConn->getHash()->getTidMgr()->tidReplaceTid(iter, iterOff, 
                                                         (uint64_t *)NULL);
            pThis->statTouchHit();
            pThis->unlock();
            pThis->notifyChange(pConn);
            pThis->respond("TOUCHED" "\r\n", pConn);
            return 0;
        }
    }
    pThis->statTouchMiss();
    pThis->unlock();
    pThis->respond("NOT_FOUND" "\r\n", pConn);
    return 0;
}


int LsMemcache::doCmdStats(LsMemcache *pThis, ls_strpair_t *pInput, int arg, 
                           MemcacheConn *pConn)
{
    char *tokPtr;
    size_t tokLen;
    char *pStr = pInput->key.ptr;
    if (pThis->m_iHdrOff == 0)
    {
        pThis->respond("SERVER_ERROR no statistics" "\r\n", pConn);
        return 0;
    }
    pStr = pThis->advToken(pStr, pStr + pInput->key.len, &tokPtr, &tokLen);
    if (tokLen > 0)
    {
        if (strcmp(tokPtr, "detail") == 0)
            ;
        else if (strcmp(tokPtr, "settings") == 0)
            ;
        else if (strcmp(tokPtr, "cachedump") == 0)
            ;
        else if (strcmp(tokPtr, "conns") == 0)
            ;
        else
        {
            if (strcmp(tokPtr, "reset") == 0)
            {
                if (pThis->useMulti())
                {
                    pThis->m_pHashMulti->foreach(multiStatResetFunc, (void *)NULL);
                }
                else
                {
                    pThis->lock();
                    ::memset(&((LsMcHdr *)pConn->getHash()->
                                offset2ptr(pThis->m_iHdrOff))->x_stats, 
                             0, sizeof(LsMcStats));
                    pThis->unlock();
                }
                pThis->respond("RESET" "\r\n", pConn);
            }
            else
            {
                pThis->respond(badCmdLineFmt, pConn);
            }
            return 0;
        }
    }

    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    LsMcStats stats;
    if (pThis->useMulti())
    {
        ::memset((void *)&stats, 0, sizeof(stats));
        pThis->m_pHashMulti->foreach(multiStatFunc, (void *)&stats);
    }
    else
    {
        pThis->lock();
        ::memcpy((void *)&stats,
                 (void *)&((LsMcHdr *)pConn->getHash()->offset2ptr(
                     pThis->m_iHdrOff))->x_stats, 
                 sizeof(stats));
        pThis->unlock();
    }
    pThis->sendResult(pConn, "STAT pid %lu\r\n", (long)getpid());
    pThis->sendResult(pConn, "STAT version %s\r\n", VERSION);
    pThis->sendResult(pConn, "STAT pointer_size %d\r\n", (int)(8 * sizeof(void *)));
    pThis->sendResult(pConn, "STAT rusage_user %ld.%06ld\r\n",
                      (long)usage.ru_utime.tv_sec,
                      (long)usage.ru_utime.tv_usec);
    pThis->sendResult(pConn, "STAT rusage_system %ld.%06ld\r\n",
                      (long)usage.ru_stime.tv_sec,
                      (long)usage.ru_stime.tv_usec);

    pThis->sendResult(pConn, "STAT cmd_get %llu\r\n",
                      (unsigned long long)stats.get_hits
                      + (unsigned long long)stats.get_misses);
    pThis->sendResult(pConn, "STAT cmd_set %llu\r\n",
                      (unsigned long long)stats.set_cmds);
    pThis->sendResult(pConn, "STAT cmd_flush %llu\r\n",
                      (unsigned long long)stats.flush_cmds);
    pThis->sendResult(pConn, "STAT cmd_touch %llu\r\n",
                      (unsigned long long)stats.touch_hits
                      + (unsigned long long)stats.touch_misses);
    pThis->sendResult(pConn, "STAT get_hits %llu\r\n",
                      (unsigned long long)stats.get_hits);
    pThis->sendResult(pConn, "STAT get_misses %llu\r\n",
                      (unsigned long long)stats.get_misses);
    pThis->sendResult(pConn, "STAT delete_misses %llu\r\n",
                      (unsigned long long)stats.delete_misses);
    pThis->sendResult(pConn, "STAT delete_hits %llu\r\n",
                      (unsigned long long)stats.delete_hits);
    pThis->sendResult(pConn, "STAT incr_misses %llu\r\n",
                      (unsigned long long)stats.incr_misses);
    pThis->sendResult(pConn, "STAT incr_hits %llu\r\n",
                      (unsigned long long)stats.incr_hits);
    pThis->sendResult(pConn, "STAT decr_misses %llu\r\n",
                      (unsigned long long)stats.decr_misses);
    pThis->sendResult(pConn, "STAT decr_hits %llu\r\n",
                      (unsigned long long)stats.decr_hits);
    pThis->sendResult(pConn, "STAT cas_misses %llu\r\n",
                      (unsigned long long)stats.cas_misses);
    pThis->sendResult(pConn, "STAT cas_hits %llu\r\n",
                      (unsigned long long)stats.cas_hits);
    pThis->sendResult(pConn, "STAT cas_badval %llu\r\n",
                      (unsigned long long)stats.cas_badval);
    pThis->sendResult(pConn, "STAT touch_hits %llu\r\n",
                      (unsigned long long)stats.touch_hits);
    pThis->sendResult(pConn, "STAT touch_misses %llu\r\n",
                      (unsigned long long)stats.touch_misses);
    pThis->sendResult(pConn, "STAT auth_cmds %llu\r\n",
                      (unsigned long long)stats.auth_cmds);
    pThis->sendResult(pConn, "STAT auth_errors %llu\r\n",
                      (unsigned long long)stats.auth_errors);
    pThis->sendResult(pConn, "END\r\n");
    return 0;
}


int LsMemcache::multiStatFunc(LsMcHashSlice *pSlice, void *pArg)
{
    LsShmHash *pHash = pSlice->m_hashByUser.getHash(pSlice->m_hashByUser.
        getMemcache()->getUser());
    LsShmOffset_t iHdrOff = pSlice->m_iHdrOff;
    if (iHdrOff == 0)
        return LS_FAIL;
    LsMcStats *pTotal = (LsMcStats *)pArg;
    pHash->disableAutoLock();
    pHash->lockChkRehash();
    LsMcStats *pStats = &((LsMcHdr *)pHash->offset2ptr(iHdrOff))->x_stats;
    pTotal->get_cmds += pStats->get_cmds;
    pTotal->set_cmds += pStats->set_cmds;
    pTotal->touch_cmds += pStats->touch_cmds;
    pTotal->flush_cmds = pStats->flush_cmds;
    pTotal->get_hits += pStats->get_hits;
    pTotal->get_misses += pStats->get_misses;
    pTotal->touch_hits += pStats->touch_hits;
    pTotal->touch_misses += pStats->touch_misses;
    pTotal->delete_hits += pStats->delete_hits;
    pTotal->delete_misses += pStats->delete_misses;
    pTotal->incr_hits += pStats->incr_hits;
    pTotal->incr_misses += pStats->incr_misses;
    pTotal->decr_hits += pStats->decr_hits;
    pTotal->decr_misses += pStats->decr_misses;
    pTotal->cas_hits += pStats->cas_hits;
    pTotal->cas_misses += pStats->cas_misses;
    pTotal->cas_badval += pStats->cas_badval;
    pTotal->auth_cmds += pStats->auth_cmds;
    pTotal->auth_errors += pStats->auth_errors;
    pHash->unlock();
    pHash->enableAutoLock();
    return LS_OK;
}


int LsMemcache::multiStatResetFunc(LsMcHashSlice *pSlice, void *pArg)
{
    LsShmHash *pHash = pSlice->m_hashByUser.
        getHash(pSlice->m_hashByUser.getMemcache()->getUser());
    LsShmOffset_t iHdrOff = pSlice->m_iHdrOff;
    if (iHdrOff == 0)
        return LS_FAIL;
    pHash->disableAutoLock();
    pHash->lockChkRehash();
    ::memset(&((LsMcHdr *)pHash->offset2ptr(iHdrOff))->x_stats, 0,
        sizeof(LsMcStats));
    pHash->unlock();
    pHash->enableAutoLock();
    return LS_OK;
}


// TODO: currently does not handle future expiration times
int LsMemcache::doCmdFlush(LsMemcache *pThis, ls_strpair_t *pInput, int arg, 
                           MemcacheConn *pConn)
{
    char *tokPtr;
    size_t tokLen;
    char *pStr = pInput->key.ptr;
    char *pStrEnd = pStr + pInput->key.len;
    char *pNxt = pInput->val.ptr;
    pStr = pThis->advToken(pStr, pStrEnd, &tokPtr, &tokLen); 
    if (tokLen > 0)
    {
        if ((pThis->m_noreply = chkNoreply(tokPtr, tokLen)) == false)
        {
            if ((tokLen != 1) || (tokPtr[0] != '0'))
            {
                pThis->respond(badCmdLineFmt, pConn);
                return 0;
            }
            pStr = pThis->advToken(pStr, pStrEnd, &tokPtr, &tokLen);
            pThis->m_noreply = chkNoreply(tokPtr, tokLen);
        }
    }
    if (!pThis->chkEndToken(pStr, pStrEnd))
    {
        pThis->respond(badCmdLineFmt, pConn);
        return 0;
    }
    if (pThis->useMulti())
    {
        if (pThis->m_pHashMulti->foreach(multiFlushFunc, 
                                         (void *)pThis) == LS_FAIL)
        {
            pThis->respond("SERVER_ERROR unable to forward" "\r\n", pConn);
            return 0;
        }
    }
    else
    {
        LsMcHashSlice *pSlice = pThis->m_pHashMulti->indx2hashSlice(0);
        if ((!pSlice->m_hashByUser.getHash(pThis->getUser())->isTidMaster())
            && (pSlice->m_pConn->GetConnFlags() & CS_REMBUSY))
        {
            pThis->putWaitQ(pConn);
            return -1;  // cannot process now
        }
        if (pThis->fwdToRemote(pSlice, pNxt, pConn))
            return 0;

        pThis->lock();
        pConn->getHash()->clear();
        pThis->statFlushCmd();
        pThis->unlock();
        pThis->notifyChange(pConn);
    }
    pThis->respond("OK" "\r\n", pConn);
    return 0;
}


int LsMemcache::multiFlushFunc(LsMcHashSlice *pSlice, void *pArg)
{
    LsShmHash *pHash = pSlice->m_hashByUser.
        getHash(pSlice->m_hashByUser.getMemcache()->getUser());
    if (pHash->isTidMaster())
    {
        LsShmOffset_t iHdrOff = pSlice->m_iHdrOff;
        pHash->disableAutoLock();
        pHash->lockChkRehash();
        pHash->clear();
        if (iHdrOff != 0)
            ++((LsMcHdr *)pHash->offset2ptr(iHdrOff))->x_stats.flush_cmds;
        pHash->unlock();
        pHash->enableAutoLock();
        
        Lsmcd::getInstance().getUsockConn()->cachedNotifyData(
            Lsmcd::getInstance().getProcId(), pSlice->m_idx );
        
//         if (pSlice->m_pNotifier != NULL)
//             pSlice->m_pNotifier->notify();
    }
    else
    {
        LsMemcache *pThis = (LsMemcache *)pArg;
        char buf[64];
        int len = snprintf(buf, sizeof(buf), "clear %d\r\n",
            (int)(pSlice - pThis->m_pHashMulti->indx2hashSlice(0)));
        if (pThis->getVerbose() > 1)
        {
            LS_INFO(">%d (fwd) %.*s", pSlice->m_pConn->getfd(), len, buf);
        }
        if (!pThis->isSliceRemote(pSlice)
            || (pSlice->m_pConn->SendBuff(buf, len, 0) != len))
        {
            LS_ERROR("Unable to forward to remote\n");
            return LS_FAIL;
        }
    }
    return LS_OK;
}


int LsMemcache::doCmdVersion(LsMemcache *pThis, ls_strpair_t *pInput, int arg, 
                             MemcacheConn *pConn)
{
    if (!pThis->chkEndToken(pInput->key.ptr,
                            pInput->key.ptr + pInput->key.len))
        pThis->respond(badCmdLineFmt, pConn);
    else
        pThis->respond("VERSION " VERSION "\r\n", pConn);
    return 0;
}


int LsMemcache::doCmdQuit(LsMemcache *pThis, ls_strpair_t *pInput, int arg, 
                          MemcacheConn *pConn)
{
    if (!pThis->chkEndToken(pInput->key.ptr,
                            pInput->key.ptr + pInput->key.len))
    {
        pThis->respond(badCmdLineFmt, pConn);
        return 0;
    }
    return -2;  // special case for termination
}


int LsMemcache::doCmdVerbosity(LsMemcache *pThis, ls_strpair_t *pInput,
                               int arg, MemcacheConn *pConn)
{
    char *tokPtr;
    size_t tokLen;
    char *pVerbose;
    uint32_t verbose;
    char *pStr = pInput->key.ptr;
    char *pStrEnd = pStr + pInput->key.len;
    pStr = pThis->advToken(pStr, pStrEnd, &pVerbose, &tokLen);
    if (tokLen <= 0)
    {
        pThis->respond(badCmdLineFmt, pConn);
        return 0;
    }
    if (!pThis->myStrtoul(pVerbose, &verbose))
    {
        pThis->respond("CLIENT_ERROR invalid verbosity argument" "\r\n", pConn);
        return 0;
    }
    pStr = pThis->advToken(pStr, pStrEnd, &tokPtr, &tokLen); // optional noreply
    pThis->m_noreply = chkNoreply(tokPtr, tokLen);

    if (!pThis->chkEndToken(pStr, pStrEnd))
    {
        pThis->respond(badCmdLineFmt, pConn);
    }
    else
    {
        pThis->setVerbose((uint8_t)verbose);
        pThis->respond("OK" "\r\n", pConn);
    }
    return 0;
}


int LsMemcache::doCmdClear(LsMemcache *pThis, ls_strpair_t *pInput, int arg, 
                           MemcacheConn *pConn)
{
    size_t tokLen;
    char *pWhich;
    uint32_t which;
    pThis->advToken(pInput->key.ptr, pInput->key.ptr + pInput->key.len,
                    &pWhich, &tokLen);
    if ((tokLen <= 0) || !pThis->myStrtoul(pWhich, &which))
    {
        LS_ERROR("SERVER_ERROR Invalid clear argument!\n");
        return 0;
    }
    if (pThis->clearSlice((int)which) != LS_OK)
        LS_ERROR("SERVER_ERROR clearSlice(%d) FAILED!\n", which);
    // internal only: no response.
    return 0;
}


LsMcHashSlice *LsMemcache::setSlice(const void *pKey, int iLen, 
                                    MemcacheConn *pConn)
{
    m_hkey = m_pHashMulti->getHKey(pKey, iLen);
    m_pCurSlice = m_pHashMulti->key2hashSlice(m_hkey);
    m_iHdrOff = m_pCurSlice->m_iHdrOff;
#ifdef USE_SASL
    if (m_mcparms.m_usesasl) 
    {
        if (!pConn->GetSasl()->isAuthenticated())
        {
            LS_DBG_M("SetHash Using anonymous user\n");
            // set hash for anonymous user
            pConn->setHash(m_pCurSlice->m_hashByUser.getHash(NULL));
        }
        else
        {
            LS_DBG_M("SetHash using user: %s\n", getUser());
            pConn->setHash(m_pCurSlice->m_hashByUser.getHash(getUser()));
        }
    }
    else
#endif
    {
        LS_DBG_M("No SASL set hash no user\n");
        pConn->setHash(m_pCurSlice->m_hashByUser.getHash(getUser()));
    }
        
    char key[iLen + 1];
    memcpy(key, pKey, iLen);
    key[iLen] = 0;
    LS_DBG_M("setSlice: key: %s, hkey: 0x%x, pCurSlice: %p, HdrOff: %d\n", 
             key, m_hkey, m_pCurSlice, m_iHdrOff);
    return m_pCurSlice;
}

int LsMemcache::processBinCmd(uint8_t *pBinBuf, int iLen, MemcacheConn *pConn)
{
    McBinCmdHdr *pHdr;
    uint8_t cmd;
    int consumed;
    uint8_t *pVal;
    LsMcUpdOpt updOpt;
    bool doTouch;

    if (iLen < (int)sizeof(*pHdr))
        return -1;  // need more data
    pHdr = (McBinCmdHdr *)pBinBuf;
    if ((consumed = sizeof(*pHdr) + ntohl(pHdr->totbody)) > iLen)
        return -1;
    if (getVerbose() > 1)
    {
        /* Dump the packet before we convert it to host order */
        int fd = pConn->getfd();
        int i;
        uint8_t *p = pBinBuf;
        char outBuf[1024];  // big enough for a header's worth of dumped bytes
        char *pOut = outBuf;
        pOut += sprintf(pOut, "<%d Read binary protocol data:", fd);
        for (i = 0; i < (int)sizeof(McBinCmdHdr); ++i)
        {
            if ((i & 0x03) == 0)
                pOut += sprintf(pOut, "\n<%d   ", fd);
            pOut += sprintf(pOut, " 0x%02x", *p++);
        }
        LS_INFO("%s\n", outBuf);
    }

#ifdef USE_SASL
    LS_DBG_M("SASL test, parm for sasl: %s, isAuthenticated: %s, opcode: %d\n",
             m_mcparms.m_usesasl ? "YES" : "NO",
             (m_mcparms.m_usesasl 
                && pConn->GetSasl()->isAuthenticated()) ? "YES" : "NO",
             pHdr->opcode);
    // Do the set user and set hash when the slice is set.
    if ((m_mcparms.m_usesasl) && (!pConn->GetSasl()->isAuthenticated()) &&
        (!m_mcparms.m_anonymous) && (!pConn->GetSasl()->getUser()))
    {
        LS_ERROR("SASL: Response is AUTHERROR for code %d!\n", pHdr->opcode);
        binErrRespond(pHdr, MC_BINSTAT_AUTHERROR, pConn);
        return 0;   // close connection
    }
#endif
    cmd = setupNoreplyCmd(pHdr->opcode);
    // special case: handle set with cas the same as replace
    if ((cmd == MC_BINCMD_SET) && (pHdr->cas != 0))
        cmd = MC_BINCMD_REPLACE;
    if ((pVal = setupBinCmd(pHdr, cmd, &updOpt, pConn)) == NULL)
    {
        binErrRespond(pHdr, MC_BINSTAT_EINVAL, pConn);
        return 0;
    }
    else if (pVal == (uint8_t *)-1)    // queued for remote
    {
        return -1;
    }
    else if (pVal == (uint8_t *)-2)    // sent to remote
    {
        return consumed;
    }
    else if ((pVal == (uint8_t *)MC_BINSTAT_E2BIG)
        || (pVal == (uint8_t *)MC_BINSTAT_ENOMEM))
    {
        unlock();   // locked in setup
        binErrRespond(pHdr, (McBinStat)(long)pVal, pConn);
        return consumed;
    }
    m_retcode = UPDRET_DONE;
    m_rescas = 0;

    doTouch = false;
    switch (cmd)
    {
        case MC_BINCMD_TOUCH:
        case MC_BINCMD_GAT:
        case MC_BINCMD_GATK:
            doTouch = true;
            // no break
        case MC_BINCMD_GET:
        case MC_BINCMD_GETK:
            doBinGet(pHdr, cmd, doTouch, pConn);
            break;
        case MC_BINCMD_SET:
            // locked in setup
            statSetCmd();
            m_iterOff = pConn->getHash()->setIteratorWithKey(m_hkey, &m_parms);
            doBinDataUpdate(pVal, pHdr, pConn);
            break;
        case MC_BINCMD_ADD:
            // locked in setup
            statSetCmd();
            m_iterOff = doHashInsert(&m_parms, &updOpt, pConn);
            m_retcode = updOpt.m_iRetcode;
            doBinDataUpdate(pVal, pHdr, pConn);
            break;
        case MC_BINCMD_REPLACE:
            // locked in setup
            statSetCmd();
            m_iterOff = doHashUpdate(&m_parms, &updOpt, pConn);
            m_retcode = updOpt.m_iRetcode;
            if (pHdr->cas != 0)
            {
                if (m_retcode == UPDRET_NOTFOUND)
                    statCasMiss();
                else if (m_retcode == UPDRET_CASFAIL)
                    statCasBad();
                else
                    statCasHit();
            }
            doBinDataUpdate(pVal, pHdr, pConn);
            break;
        case MC_BINCMD_DELETE:
            doBinDelete(pHdr, pConn);
            break;
        case MC_BINCMD_INCREMENT:
        case MC_BINCMD_DECREMENT:
            doBinArithmetic(pHdr, cmd, &updOpt, pConn);
            break;
        case MC_BINCMD_QUIT:
            binOkRespond(pHdr, pConn);
            return 0;   // close connection
        case MC_BINCMD_FLUSH:
            doBinFlush(pHdr, pConn);
            break;
        case MC_BINCMD_NOOP:
            binOkRespond(pHdr, pConn);
            break;
        case MC_BINCMD_VERSION:
            doBinVersion(pHdr, pConn);
            break;
        case MC_BINCMD_APPEND:
        case MC_BINCMD_PREPEND:
            // locked in setup
            statSetCmd();
            m_iterOff = doHashUpdate(&m_parms, &updOpt, pConn);
            // memcached compatibility
            m_retcode = ((updOpt.m_iRetcode == UPDRET_NOTFOUND) ?
                UPDRET_DONE: updOpt.m_iRetcode);
            doBinDataUpdate(pVal, pHdr, pConn);
            break;
        case MC_BINCMD_STAT:
            doBinStats(pHdr, pConn);
            break;
        case MC_BINCMD_VERBOSITY:
        {
            McBinReqExtra *pReqX = (McBinReqExtra *)(pHdr + 1);
            setVerbose((uint8_t)ntohl(pReqX->verbosity.level));
        }
            binOkRespond(pHdr, pConn);
            break;
#ifdef USE_SASL
        case MC_BINCMD_SASL_LIST:
            LS_DBG_M("SASL_LIST command\n");
            doBinSaslList(pHdr, pConn);
            break;
        case MC_BINCMD_SASL_AUTH:
        case MC_BINCMD_SASL_STEP:
            LS_DBG_M("SASL_AUTH or STEP command\n");
            doBinSaslAuth(pHdr, pConn);
            break;
#endif
        default:
            LS_DBG_M("UNKNOWN command (SASL?): %d\n", cmd);
            binErrRespond(pHdr, MC_BINSTAT_UNKNOWNCMD, pConn);
            break;
    }
    return ((pConn->GetConnFlags() & CS_CMDWAIT) ? -1 : consumed);
}


uint8_t LsMemcache::setupNoreplyCmd(uint8_t cmd)
{
    m_noreply = true;
    switch (cmd)
    {
        case MC_BINCMD_SETQ:
            cmd = MC_BINCMD_SET;
            break;
        case MC_BINCMD_ADDQ:
            cmd = MC_BINCMD_ADD;
            break;
        case MC_BINCMD_REPLACEQ:
            cmd = MC_BINCMD_REPLACE;
            break;
        case MC_BINCMD_DELETEQ:
            cmd = MC_BINCMD_DELETE;
            break;
        case MC_BINCMD_INCREMENTQ:
            cmd = MC_BINCMD_INCREMENT;
            break;
        case MC_BINCMD_DECREMENTQ:
            cmd = MC_BINCMD_DECREMENT;
            break;
        case MC_BINCMD_QUITQ:
            cmd = MC_BINCMD_QUIT;
            break;
        case MC_BINCMD_FLUSHQ:
            cmd = MC_BINCMD_FLUSH;
            break;
        case MC_BINCMD_APPENDQ:
            cmd = MC_BINCMD_APPEND;
            break;
        case MC_BINCMD_PREPENDQ:
            cmd = MC_BINCMD_PREPEND;
            break;
        case MC_BINCMD_GETQ:
            cmd = MC_BINCMD_GET;
            m_noreply = false;
            break;
        case MC_BINCMD_GETKQ:
            cmd = MC_BINCMD_GETK;
            m_noreply = false;
            break;
        case MC_BINCMD_GATQ:
            cmd = MC_BINCMD_GAT;
            break;
        case MC_BINCMD_GATKQ:
            cmd = MC_BINCMD_GATK;
            break;
        default:
            m_noreply = false;
            break;
    }
    return cmd;
}


uint8_t *LsMemcache::setupBinCmd(
    McBinCmdHdr *pHdr, uint8_t cmd, LsMcUpdOpt *pOpt, MemcacheConn *pConn)
{
    uint8_t *pBody = (uint8_t *)(pHdr + 1);
    uint32_t bodyLen = (uint32_t)ntohl(pHdr->totbody);
    uint16_t keyLen = (uint16_t)ntohs(pHdr->keylen);
    LsMcHashSlice *pSlice;

    if (keyLen > KEY_MAXLEN)
        return NULL;
    if (bodyLen == keyLen)
    {
        m_parms.key.ptr = (char *)pBody;
        m_parms.key.len = keyLen;
        if (cmd == MC_BINCMD_DELETE)    // remote eligible
        {
            pSlice = canProcessNow(m_parms.key.ptr, m_parms.key.len, pConn);
            if (pSlice == NULL)
                return (uint8_t *)-1;   // queued for remote
            if (fwdBinToRemote(pSlice, pHdr, pConn))
                return (uint8_t *)-2;   // sent to remote
        }
        else
        {
            setSlice(m_parms.key.ptr, m_parms.key.len, pConn);
        }
        pHdr->cas = (uint64_t)ntohll(pHdr->cas);
    }
    else
    {
        McBinStat ret;
        McBinReqExtra *pReqX = (McBinReqExtra *)pBody;
        pBody += pHdr->extralen;

        m_parms.key.ptr = (char *)pBody;
        m_parms.key.len = keyLen;
        if (isRemoteEligible(cmd))
        {
            pSlice = canProcessNow(m_parms.key.ptr, m_parms.key.len, pConn);
            if (pSlice == NULL)
                return (uint8_t *)-1;   // queued for remote
            if (fwdBinToRemote(pSlice, pHdr, pConn))
                return (uint8_t *)-2;   // sent to remote
        }
        else
        {
            setSlice(m_parms.key.ptr, m_parms.key.len, pConn);
        }
        m_parms.val.ptr = NULL;
        m_parms.val.len = bodyLen - keyLen - pHdr->extralen;
        pHdr->cas = (uint64_t)ntohll(pHdr->cas);

        switch (cmd)
        {
            case MC_BINCMD_REPLACE:
                pOpt->m_iFlags = cmd;
                pOpt->m_cas = pHdr->cas;
                // no break
            case MC_BINCMD_ADD:
            case MC_BINCMD_SET:
                if (pHdr->extralen != sizeof(pReqX->value))
                    return NULL;
                // special case
                lock();
                if ((ret = chkMemSz(cmd)) != MC_BINSTAT_SUCCESS)
                    return (uint8_t *)ret;
                m_item.x_flags = (uint32_t)ntohl(pReqX->value.flags);
                m_item.x_exptime = (time_t)ntohl(pReqX->value.exptime);
                m_parms.val.len = parmAdjLen(m_parms.val.len);
                pOpt->m_iRetcode = UPDRET_DONE;
                break;

            case MC_BINCMD_TOUCH:
            case MC_BINCMD_GAT:
            case MC_BINCMD_GATK:
            case MC_BINCMD_FLUSH:
                if (pHdr->extralen != sizeof(pReqX->touch))
                    return NULL;
                m_item.x_exptime = (time_t)ntohl(pReqX->touch.exptime);
                break;

            case MC_BINCMD_INCREMENT:
            case MC_BINCMD_DECREMENT:
#ifdef notdef
                if (pHdr->extralen != sizeof(pReqX->incrdecr))
#endif
                if (pHdr->extralen != 20)	// ugly, but need for 64-bit compiler
                    return NULL;
                m_parms.val.len = parmAdjLen(0);
                pOpt->m_iFlags = cmd;
                if (m_mcparms.m_usecas)
                    pOpt->m_iFlags |= LSMC_USECAS;
                pOpt->m_value = (uint64_t)ntohll(pReqX->incrdecr.delta);
                pOpt->m_cas = pHdr->cas;
                pOpt->m_pRet = (void *)&m_item;
                break;

            case MC_BINCMD_APPEND:
                // special case
                lock();
                if ((ret = chkMemSz(cmd)) != MC_BINSTAT_SUCCESS)
                    return (uint8_t *)ret;
                pOpt->m_iFlags = MC_BINCMD_APPEND;
                if (m_mcparms.m_usecas)
                    pOpt->m_iFlags |= LSMC_USECAS;
                pOpt->m_cas = pHdr->cas;
                pOpt->m_iRetcode = UPDRET_APPEND;
                break;

            case MC_BINCMD_PREPEND:
                // special case
                lock();
                if ((ret = chkMemSz(cmd)) != MC_BINSTAT_SUCCESS)
                    return (uint8_t *)ret;
                pOpt->m_iFlags = MC_BINCMD_PREPEND;
                if (m_mcparms.m_usecas)
                    pOpt->m_iFlags |= LSMC_USECAS;
                pOpt->m_cas = pHdr->cas;
                pOpt->m_iRetcode = UPDRET_PREPEND;
                break;

            case MC_BINCMD_VERBOSITY:
                if (pHdr->extralen != sizeof(pReqX->verbosity))
                    return NULL;
                break;

            default:
                break;
        }
        pBody += keyLen;
    }
    return pBody;
}


bool LsMemcache::isRemoteEligible(uint8_t cmd)
{
    switch(cmd)
    {
        case MC_BINCMD_TOUCH:
        case MC_BINCMD_GAT:
        case MC_BINCMD_GATK:
        case MC_BINCMD_SET:
        case MC_BINCMD_ADD:
        case MC_BINCMD_REPLACE:
        case MC_BINCMD_DELETE:
        case MC_BINCMD_INCREMENT:
        case MC_BINCMD_DECREMENT:
        case MC_BINCMD_APPEND:
        case MC_BINCMD_PREPEND:
            return true;
        default:
            return false;
    }
}


void LsMemcache::doBinGet(McBinCmdHdr *pHdr, uint8_t cmd, bool doTouch, 
                          MemcacheConn *pConn)
{
    LS_DBG_M("doBinGet, pConn: %p, pHash: %p\n", pConn, pConn->getHash());
    uint8_t resBuf[sizeof(McBinCmdHdr)+sizeof(McBinResExtra)];
    int keyLen = ((cmd == MC_BINCMD_GATK) || (cmd == MC_BINCMD_GETK)) ?
        m_parms.key.len : 0;
    if (getVerbose() > 1)
    {
        LS_INFO("<%d %s %.*s\n", pConn->getfd(), (doTouch ? "TOUCH" : "GET"),
                (int)m_parms.key.len, m_parms.key.ptr);
    }
    lock();
    if ((m_iterOff = pConn->getHash()->findIteratorWithKey(
        m_hkey, &m_parms)).m_iOffset != 0)
    {
        LsShmHElem *iter;
        LsMcDataItem *pItem;
        int valLen;
        uint8_t *valPtr;
        iter = pConn->getHash()->offset2iterator(m_iterOff);
        pItem = mcIter2data(iter, m_mcparms.m_usecas, &valPtr, &valLen);
        if (isExpired(pItem))
        {
            pConn->getHash()->eraseIterator(m_iterOff);
            notifyChange(pConn);
            if (doTouch)
                statTouchMiss();
            else
                statGetMiss();
        }
        else
        {
            int extra = sizeof(((McBinResExtra *)0)->value.flags);
            McBinResExtra *pResX = (McBinResExtra *)(&resBuf[sizeof(McBinCmdHdr)]);
            pResX->value.flags = (uint32_t)htonl(pItem->x_flags);
            if (doTouch)
            {
                setItemExptime(pItem, (uint32_t)m_item.x_exptime);
                pConn->getHash()->getTidMgr()->tidReplaceTid(iter, m_iterOff, 
                                                             (uint64_t *)NULL);
                notifyChange(pConn);
                statTouchHit();
                pItem = mcIter2data(pConn->getHash()->offset2iterator(m_iterOff), 
                                    m_mcparms.m_usecas, &valPtr, &valLen);
            }
            else
                statGetHit();
            saveCas(pItem);
            if (cmd == MC_BINCMD_TOUCH)
                valLen = 0;     // return only flags
            setupBinResHdr(pHdr,
                           (uint8_t)extra, (uint16_t)keyLen, 
                           (uint32_t)extra + keyLen + valLen,
                           MC_BINSTAT_SUCCESS, resBuf, pConn);
            binRespond(resBuf, sizeof(McBinCmdHdr) + extra, pConn);
            if (keyLen != 0)
                binRespond((uint8_t *)m_parms.key.ptr, m_parms.key.len, pConn);
            if (valLen != 0)
                binRespond(valPtr, valLen, pConn);
            unlock();
            return;
        }
    }
    else if (doTouch)
        statTouchMiss();
    else
        statGetMiss();
    unlock();
    if ((m_noreply == false)
        && (pHdr->opcode != MC_BINCMD_GETQ) && (pHdr->opcode != MC_BINCMD_GETKQ))
    {
        if (keyLen != 0)
        {
            setupBinResHdr(pHdr,
                0, (uint16_t)keyLen, (uint32_t)keyLen,
                MC_BINSTAT_KEYENOENT, resBuf, pConn);
            binRespond(resBuf, sizeof(McBinCmdHdr), pConn);
            binRespond((uint8_t *)m_parms.key.ptr, m_parms.key.len, pConn);
        }
        else
        {
            binErrRespond(pHdr, MC_BINSTAT_KEYENOENT, pConn);
        }
    }
    return;
}


int LsMemcache::doBinDataUpdate(uint8_t *pBuf, McBinCmdHdr *pHdr, 
                                MemcacheConn *pConn)
{
    LS_DBG_M("doBinDataUpdate, pHash: %p, iterOff: %d\n", pConn->getHash(), 
             m_iterOff.m_iOffset);
    m_parms.key.len = 0;
    if (m_iterOff.m_iOffset != 0)
    {
        dataItemUpdate(pBuf, pConn);
        unlock();
        notifyChange(pConn);
        if (m_retcode != UPDRET_NONE)
            binOkRespond(pHdr, pConn);
        m_iterOff.m_iOffset = 0;
    }
    else
    {
        McBinStat stat;
        unlock();
        if (m_retcode == UPDRET_NOTFOUND)
            stat = MC_BINSTAT_KEYENOENT;
        else if (m_retcode == UPDRET_EEXISTS)   // also CASFAIL
            stat = MC_BINSTAT_KEYEEXISTS;
        else
            stat = MC_BINSTAT_NOTSTORED;
        binErrRespond(pHdr, stat, pConn);
    }
    return 0;
}


void LsMemcache::doBinDelete(McBinCmdHdr *pHdr, MemcacheConn *pConn)
{
    lock();
    if ((m_iterOff = pConn->getHash()->findIteratorWithKey(
        m_hkey, &m_parms)).m_iOffset != 0)
    {
        LsMcDataItem *pItem =
            (LsMcDataItem *)pConn->getHash()->offset2iteratorData(m_iterOff);
        if (isExpired(pItem))
        {
            pConn->getHash()->eraseIterator(m_iterOff);
            statDeleteMiss();
            unlock();
            notifyChange(pConn);
            binErrRespond(pHdr, MC_BINSTAT_KEYENOENT, pConn);
        }
        else if (m_mcparms.m_usecas && (pHdr->cas != 0)
                 && (pHdr->cas != pItem->x_data->withcas.cas))
        {
            unlock();
            binErrRespond(pHdr, MC_BINSTAT_KEYEEXISTS, pConn);
        }
        else
        {
            pConn->getHash()->eraseIterator(m_iterOff);
            statDeleteHit();
            unlock();
            notifyChange(pConn);
            binOkRespond(pHdr, pConn);
        }
    }
    else
    {
        statDeleteMiss();
        unlock();
        binErrRespond(pHdr, MC_BINSTAT_KEYENOENT, pConn);
    }
    return;
}


void LsMemcache::doBinArithmetic(
    McBinCmdHdr *pHdr, uint8_t cmd, LsMcUpdOpt *pOpt, MemcacheConn *pConn)
{
    char numBuf[ULL_MAXLEN+1];
    uint8_t resBuf[sizeof(McBinCmdHdr)+sizeof(McBinResExtra)];

    // return value in `extra' field
    int extra = sizeof(((McBinResExtra *)0)->incrdecr.newval);
    McBinResExtra *pResX = (McBinResExtra *)(&resBuf[sizeof(McBinCmdHdr)]);

    pOpt->m_pMisc = (void *)numBuf;
    lock();
    if ((m_iterOff = doHashUpdate(&m_parms, pOpt, pConn)).m_iOffset != 0)
    {
        if (cmd == MC_BINCMD_INCREMENT)
            statIncrHit();
        else
            statDecrHit();
        m_retcode = UPDRET_NONE;
        doBinDataUpdate((uint8_t *)numBuf, pHdr, pConn);
        setupBinResHdr(pHdr,
            (uint8_t)0, (uint16_t)0, (uint32_t)extra,
            MC_BINSTAT_SUCCESS, resBuf, pConn);
        pResX->incrdecr.newval = (uint64_t)htonll(strtoull(numBuf, NULL, 10));
        binRespond(resBuf, sizeof(McBinCmdHdr) + extra, pConn);
    }
    else if (pOpt->m_iRetcode == UPDRET_NOTFOUND)
    {
        McBinReqExtra *pReqX = (McBinReqExtra *)(pHdr + 1);
        if (pReqX->incrdecr.exptime == 0xffffffff)
        {
            if (cmd == MC_BINCMD_INCREMENT)
                statIncrMiss();
            else
                statDecrMiss();
            unlock();
            binErrRespond(pHdr, MC_BINSTAT_KEYENOENT, pConn);
        }
        else
        {
            m_item.x_flags = 0;
            m_item.x_exptime = (time_t)ntohl(pReqX->incrdecr.exptime);
            m_parms.val.len = parmAdjLen(
                snprintf(numBuf, sizeof(numBuf), "%llu",
                  (unsigned long long)ntohll(pReqX->incrdecr.initval)));
            if ((m_iterOff = pConn->getHash()->insertIteratorWithKey(
                m_hkey, &m_parms)).m_iOffset != 0)
            {
                m_retcode = UPDRET_NONE;
                doBinDataUpdate((uint8_t *)numBuf, pHdr, pConn);
                setupBinResHdr(pHdr,
                    (uint8_t)0, (uint16_t)0, (uint32_t)extra,
                    MC_BINSTAT_SUCCESS, resBuf, pConn);
                pResX->incrdecr.newval = pReqX->incrdecr.initval;
                binRespond(resBuf, sizeof(McBinCmdHdr) + extra, pConn);
            }
            else
            {
                unlock();
                binErrRespond(pHdr, MC_BINSTAT_NOTSTORED, pConn);
            }
        }
    }
    else
    {
        McBinStat stat;
        unlock();
        if (pOpt->m_iRetcode == UPDRET_NONNUMERIC)
            stat = MC_BINSTAT_DELTABADVAL;
        else if (pOpt->m_iRetcode == UPDRET_CASFAIL)
            stat = MC_BINSTAT_KEYEEXISTS;
        else
            stat = MC_BINSTAT_NOTSTORED;
        binErrRespond(pHdr, stat, pConn);
    }
    return;
}


// TODO: currently does not handle future expiration times
void LsMemcache::doBinFlush(McBinCmdHdr *pHdr, MemcacheConn *pConn)
{
    if ((pHdr->extralen > 0) && (m_item.x_exptime != 0))
    {
        binErrRespond(pHdr, MC_BINSTAT_EINVAL, pConn);
        return;
    }
    if (useMulti())
    {
        if (m_pHashMulti->foreach(multiFlushFunc, (void *)this) == LS_FAIL)
        {
            binErrRespond(pHdr, MC_BINSTAT_REMOTEERROR, pConn);
            return;
        }
    }
    else
    {
        LsMcHashSlice *pSlice = m_pHashMulti->indx2hashSlice(0);
        if ((!pConn->getHash()->isTidMaster()) && 
            (pSlice->m_pConn->GetConnFlags() & CS_REMBUSY))
        {
            putWaitQ(pConn);
            return;  // cannot process now
        }
        if (fwdBinToRemote(pSlice, pHdr, pConn))
            return;

        lock();
        pConn->getHash()->clear();
        statFlushCmd();
        unlock();
        notifyChange(pConn);
    }
    binOkRespond(pHdr, pConn);
    return;
}


void LsMemcache::doBinVersion(McBinCmdHdr *pHdr, MemcacheConn *pConn)
{
    uint8_t resBuf[sizeof(McBinCmdHdr)];
    int len = strlen(VERSION);
    setupBinResHdr(pHdr, (uint8_t)0, (uint16_t)0, (uint32_t)len, 
                   MC_BINSTAT_SUCCESS, resBuf, pConn);
    binRespond(resBuf, sizeof(McBinCmdHdr), pConn);
    binRespond((uint8_t *)VERSION, len, pConn);
}


void LsMemcache::doBinStats(McBinCmdHdr *pHdr, MemcacheConn *pConn)
{
    if (m_iHdrOff == 0)
    {
        binErrRespond(pHdr, MC_BINSTAT_EINVAL, pConn);
        return;
    }
    uint16_t keyLen = (uint16_t)ntohs(pHdr->keylen);
    if ((keyLen == 5) && (memcmp((void *)(pHdr + 1), "reset", 5)  == 0))
    {
        if (useMulti())
        {
            m_pHashMulti->foreach(multiStatResetFunc, (void *)NULL);
        }
        else
        {
            lock();
            ::memset(&((LsMcHdr *)pConn->getHash()->
                        offset2ptr(m_iHdrOff))->x_stats, 0, sizeof(LsMcStats));
            unlock();
        }
        binOkRespond(pHdr, pConn);
        return;
    }

    uint8_t resBuf[sizeof(McBinCmdHdr) + STATITEM_MAXLEN];
    McBinCmdHdr *pResHdr = (McBinCmdHdr *)resBuf;
    pResHdr->magic = MC_BINARY_RES;
    pResHdr->opcode = pHdr->opcode;
    pResHdr->keylen = 0;
    pResHdr->extralen = 0;
    pResHdr->datatype = 0;
    pResHdr->status = (uint16_t)htons(MC_BINSTAT_SUCCESS);
    pResHdr->totbody = 0;
    pResHdr->opaque = pHdr->opaque;
    pResHdr->cas = (uint64_t)0;

    uint64_t pid = (uint64_t)getpid();
    uint64_t ptrsz = (uint64_t)(8 * sizeof(void *));
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);

    doBinStat1p64(pResHdr, "pid", &pid, pConn);
    doBinStat1str(pResHdr, "version", (char *)VERSION, pConn);
    doBinStat1p64(pResHdr, "pointer_size", &ptrsz, pConn);
    doBinStat1dot6(pResHdr, "rusage_user",
        (long)usage.ru_utime.tv_sec, (long)usage.ru_utime.tv_usec, pConn);
    doBinStat1dot6(pResHdr, "rusage_system",
        (long)usage.ru_stime.tv_sec, (long)usage.ru_stime.tv_usec, pConn);
    LsMcStats stats;
    if (useMulti())
    {
        ::memset((void *)&stats, 0, sizeof(stats));
        m_pHashMulti->foreach(multiStatFunc, (void *)&stats);
    }
    else
    {
        lock();
        ::memcpy((void *)&stats,
            (void *)&((LsMcHdr *)m_pCurSlice->m_hashByUser.getHash(getUser())->
                offset2ptr(m_iHdrOff))->x_stats, sizeof(stats));
        unlock();
    }
    uint64_t getcmds = stats.get_hits + stats.get_misses;
    uint64_t touchcmds = stats.touch_hits + stats.touch_misses;
    doBinStat1p64(pResHdr, "cmd_get", &getcmds, pConn);
    doBinStat1p64(pResHdr, "cmd_set", &stats.set_cmds, pConn);
    doBinStat1p64(pResHdr, "cmd_flush", &stats.flush_cmds, pConn);
    doBinStat1p64(pResHdr, "cmd_touch", &touchcmds, pConn);
    doBinStat1p64(pResHdr, "get_hits", &stats.get_hits, pConn);
    doBinStat1p64(pResHdr, "get_misses", &stats.get_misses, pConn);
    doBinStat1p64(pResHdr, "delete_misses", &stats.delete_misses, pConn);
    doBinStat1p64(pResHdr, "delete_hits", &stats.delete_hits, pConn);
    doBinStat1p64(pResHdr, "incr_misses", &stats.incr_misses, pConn);
    doBinStat1p64(pResHdr, "incr_hits", &stats.incr_hits, pConn);
    doBinStat1p64(pResHdr, "decr_misses", &stats.decr_misses, pConn);
    doBinStat1p64(pResHdr, "decr_hits", &stats.decr_hits, pConn);
    doBinStat1p64(pResHdr, "cas_misses", &stats.cas_misses, pConn);
    doBinStat1p64(pResHdr, "cas_hits", &stats.cas_hits, pConn);
    doBinStat1p64(pResHdr, "cas_badval", &stats.cas_badval, pConn);
    doBinStat1p64(pResHdr, "touch_hits", &stats.touch_hits, pConn);
    doBinStat1p64(pResHdr, "touch_misses", &stats.touch_misses, pConn);
    doBinStat1p64(pResHdr, "auth_cmds", &stats.auth_cmds, pConn);
    doBinStat1p64(pResHdr, "auth_errors", &stats.auth_errors, pConn);
    binOkRespond(pHdr, pConn);
}


void LsMemcache::doBinStat1str(
    McBinCmdHdr *pResHdr, const char *pkey, char *pval, MemcacheConn *pConn)
{
    doBinStat1Send(pResHdr, pkey, snprintf((char *)(pResHdr + 1),
        STATITEM_MAXLEN, "%s%s", pkey, pval), pConn);
}


void LsMemcache::doBinStat1p64(
    McBinCmdHdr *pResHdr, const char *pkey, uint64_t *pval, MemcacheConn *pConn)
{
    doBinStat1Send(pResHdr, pkey, snprintf((char *)(pResHdr + 1),
        STATITEM_MAXLEN, "%s%llu", pkey, (unsigned long long)*pval), pConn);
}


void LsMemcache::doBinStat1dot6(
    McBinCmdHdr *pResHdr, const char *pkey, long val1, long val2, 
    MemcacheConn *pConn)
{
    doBinStat1Send(pResHdr, pkey, snprintf((char *)(pResHdr + 1),
        STATITEM_MAXLEN, "%s%ld.%06ld", pkey, val1, val2), pConn);
}


void LsMemcache::doBinStat1Send(
    McBinCmdHdr *pResHdr, const char *pkey, int bodylen, MemcacheConn *pConn)
{
    uint16_t keylen = (uint16_t)strlen(pkey);
    pResHdr->keylen = (uint16_t)htons(keylen);
    pResHdr->totbody = (uint32_t)htonl((long)bodylen);
    binRespond((uint8_t *)pResHdr, sizeof(*pResHdr) + bodylen, pConn);
}


#ifdef USE_SASL
void LsMemcache::doBinSaslList(McBinCmdHdr *pHdr, MemcacheConn *pConn)
{
    const char *result;
    int len;
    uint8_t resBuf[sizeof(McBinCmdHdr)];
    LS_DBG_M("doBinSaslList (hdr size: %ld)\n",sizeof(McBinCmdHdr));
    if (!m_mcparms.m_usesasl)
    {
        LS_DBG_M("SASL turned off\n");
        binErrRespond(pHdr, MC_BINSTAT_UNKNOWNCMD, pConn);
        return;
    }
    if ((len = pConn->GetSasl()->listMechs(&result)) < 0)
    {
        if (getVerbose() > 0)
        {
            LS_INFO("Failed to list SASL mechanisms.\n");
        }
        LS_DBG_M("No SASL mechanism\n");
        binErrRespond(pHdr, MC_BINSTAT_AUTHERROR, pConn);
    }
    else
    {
        setupBinResHdr(pHdr,
            (uint8_t)0, (uint16_t)0, (uint32_t)len, MC_BINSTAT_SUCCESS, resBuf, pConn);
        binRespond(resBuf, sizeof(McBinCmdHdr), pConn);
        binRespond((uint8_t *)result, len, pConn);
        LS_DBG_M("SASL enabled and on\n");
    }
    return;
}


void LsMemcache::doBinSaslAuth(McBinCmdHdr *pHdr, MemcacheConn *pConn)
{
    int ret;
    const char *result;
    unsigned int len;
    uint8_t resBuf[sizeof(McBinCmdHdr)];
    LS_DBG_M("doBinSaslAuth\n");
    if (!m_mcparms.m_usesasl)
    {
        LS_DBG_M("SASL off\n");
        binErrRespond(pHdr, MC_BINSTAT_UNKNOWNCMD, pConn);
        return;
    }
    unsigned int mechLen = (unsigned int)ntohs(pHdr->keylen);
    unsigned int valLen = (unsigned int)ntohl(pHdr->totbody) - mechLen;
    if (mechLen > SASLMECH_MAXLEN)
    {
        LS_DBG_M("SASL mech not right\n");
        binErrRespond(pHdr, MC_BINSTAT_EINVAL, pConn);
        return;
    }
    if (pHdr->opcode == MC_BINCMD_SASL_AUTH)
    {
        ret = pConn->GetSasl()->chkAuth(
            (char *)(pHdr + 1), mechLen, valLen, &result, &len);
    }
    else /* if (pHdr->opcode == MC_BINCMD_SASL_STEP) */
    {
        ret = pConn->GetSasl()->chkAuthStep(
            ((char *)(pHdr + 1)) + mechLen, valLen, &result, &len);
    }
    if (ret == 0)
    {
        static const char authok[] = "Authenticated";
        int userLen = valLen;
        char user[userLen + 1];
        memcpy(user, (char *)(pHdr + 1) + mechLen, userLen);
        user[userLen] = 0;
        LS_DBG_M("SASL worked, user: %s! mechLen: %d, userLen: %d, usesasl: %s, byUser: %s\n", 
                 user, mechLen, userLen, m_mcparms.m_usesasl ? "YES":"NO", m_mcparms.m_byUser ? "YES" : "NO");
        if (m_mcparms.m_usesasl && m_mcparms.m_byUser)
            setUser(user);
        setupBinResHdr(pHdr,
            (uint8_t)0, (uint16_t)0, (uint32_t)(sizeof(authok) - 1),
            MC_BINSTAT_SUCCESS, resBuf, pConn);
        binRespond(resBuf, sizeof(McBinCmdHdr), pConn);
        binRespond((uint8_t *)authok, sizeof(authok) - 1, pConn);
        lock();
        statAuthCmd();
        unlock();
    }
    else if (ret > 0)
    {
        LS_DBG_M("NO SASL for YOU!\n");
        setupBinResHdr(pHdr,
            (uint8_t)0, (uint16_t)0, (uint32_t)len,
            MC_BINSTAT_AUTHCONTINUE, resBuf, pConn);
        binRespond(resBuf, sizeof(McBinCmdHdr), pConn);
        if (len > 0)
            binRespond((uint8_t *)result, len, pConn);
    }
    else /* if (ret < 0) */
    {
        LS_DBG_M("SASL really off!\n");
        binErrRespond(pHdr, MC_BINSTAT_AUTHERROR, pConn);
        lock();
        statAuthCmd();
        statAuthErr();
        unlock();
    }
    return;
}
#endif


void LsMemcache::setupBinResHdr(McBinCmdHdr *pHdr,
    uint8_t extralen, uint16_t keylen, uint32_t totbody,
    uint16_t status, uint8_t *pBinBuf, MemcacheConn *pConn)
{
    if (m_noreply)
        return;

    McBinCmdHdr *pResHdr = (McBinCmdHdr *)pBinBuf;
    pResHdr->magic = MC_BINARY_RES;
    pResHdr->opcode = pHdr->opcode;
    pResHdr->keylen = (uint16_t)htons(keylen);
    pResHdr->extralen = extralen;
    pResHdr->datatype = 0;
    pResHdr->status = (uint16_t)htons(status);
    pResHdr->totbody = (uint32_t)htonl(totbody);
    pResHdr->opaque = pHdr->opaque;
    pResHdr->cas = (uint64_t)htonll(m_rescas);

    if (getVerbose() > 1)
    {
        int fd = pConn->getfd();
        int i;
        uint8_t *p = pBinBuf;
        char outBuf[1024];  // big enough for a header's worth of dumped bytes
        char *pOut = outBuf;
        pOut += sprintf(pOut, ">%d Writing bin response:", fd);
        for (i = 0; i < (int)sizeof(McBinCmdHdr); ++i)
        {
            if ((i & 0x03) == 0)
                pOut += sprintf(pOut, "\n>%d  ", fd);
            pOut += sprintf(pOut, " 0x%02x", *p++);
        }
        LS_INFO("%s\n", outBuf);
    }
    return;
}


void LsMemcache::binOkRespond(McBinCmdHdr *pHdr, MemcacheConn *pConn)
{
    uint8_t resBuf[sizeof(McBinCmdHdr)];
    setupBinResHdr(pHdr, (uint8_t)0, (uint16_t)0, (uint32_t)0,
                   MC_BINSTAT_SUCCESS, resBuf, pConn);
    binRespond(resBuf, sizeof(McBinCmdHdr), pConn);
    return;
}


void LsMemcache::binErrRespond(McBinCmdHdr *pHdr, McBinStat err, 
                               MemcacheConn *pConn)
{
    const char *text;
    uint8_t resBuf[sizeof(McBinCmdHdr)];
    switch (err)
    {
    case MC_BINSTAT_KEYENOENT:
        text = "Not found";
        break;
    case MC_BINSTAT_KEYEEXISTS:
        text = "Data exists for key.";
        break;
    case MC_BINSTAT_E2BIG:
        text = "Too large.";
        break;
    case MC_BINSTAT_EINVAL:
        text = "Invalid arguments";
        break;
    case MC_BINSTAT_NOTSTORED:
        text = "Not stored.";
        break;
    case MC_BINSTAT_DELTABADVAL:
        text = "Non-numeric server-side value for incr or decr";
        break;
    case MC_BINSTAT_AUTHERROR:
        text = "Auth failure.";
        break;
    case MC_BINSTAT_REMOTEERROR:
        text = "Remote error.";
        break;
    case MC_BINSTAT_UNKNOWNCMD:
        text = "Unknown command";
        break;
    case MC_BINSTAT_ENOMEM:
        text = "Out of memory allocating item";
        break;
    default:
        text = "UNHANDLED ERROR";
        LS_ERROR("UNHANDLED ERROR: %d\n", err);
    }

    if (getVerbose() > 1)
    {
        LS_INFO(">%d Writing an error: %s\n", pConn->getfd(), text);
    }

    int len = strlen(text);
    m_iterOff.m_iOffset = 0;
    m_rescas = 0;
    m_noreply = false;
    setupBinResHdr(pHdr, 0, (uint16_t)0, (uint32_t)len, err, resBuf, pConn);
    binRespond(resBuf, sizeof(McBinCmdHdr), pConn);
    binRespond((uint8_t *)text, len, pConn);
    return;
}


char *LsMemcache::advToken(char *pStr, char *pStrEnd, char **pTokPtr, 
                           size_t *pTokLen)
{
    while ((pStr < pStrEnd) && (*pStr ==  ' '))
       ++pStr;
    if (pStr >= pStrEnd)
    {
        *pTokPtr = pStrEnd;
        *pTokLen = 0;
        return pStrEnd;
    }
    *pTokPtr = pStr;
    pStr = (char *)memchr(pStr, ' ', pStrEnd - pStr);
    if (pStr == NULL)
        pStr = pStrEnd;
    *pTokLen = pStr - *pTokPtr;
    if (*pStr == ' ')
        ++pStr;
    return pStr;
}


bool LsMemcache::myStrtol(const char *pStr, int32_t *pVal)
{
    char *endptr;
    errno = 0;
    long val = strtol(pStr, &endptr, 10);
    if ((errno == ERANGE) || (endptr == pStr) || (*endptr && !isspace(*endptr)))
        return false;
    *pVal = (int32_t)val;
    return true;
}


bool LsMemcache::myStrtoul(const char *pStr, uint32_t *pVal)
{
    char *endptr;
    errno = 0;
    unsigned long val = strtoul(pStr, &endptr, 10);
    if ((errno == ERANGE) || (endptr == pStr) || (*endptr && !isspace(*endptr)))
        return false;
    *pVal = (uint32_t)val;
    return true;
}


bool LsMemcache::myStrtoll(const char *pStr, int64_t *pVal)
{
    char *endptr;
    errno = 0;
    long long val = strtoll(pStr, &endptr, 10);
    if ((errno == ERANGE) || (endptr == pStr) || (*endptr && !isspace(*endptr)))
        return false;
    *pVal = val;
    return true;
}


bool LsMemcache::myStrtoull(const char *pStr, uint64_t *pVal)
{
    char *endptr;
    errno = 0;
    unsigned long long val = strtoull(pStr, &endptr, 10);
    if ((errno == ERANGE) || (endptr == pStr) || (*endptr && !isspace(*endptr)))
        return false;
    *pVal = val;
    return true;
}

