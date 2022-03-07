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
#ifndef LSSHMMEMCACHE_H
#define LSSHMMEMCACHE_H

#include <lsdef.h>
#include <log4cxx/logger.h>
#include "memcacheconn.h"
#include <shm/lsshmhash.h>
#include "lsmchashmulti.h"
#include "lsmcsasl.h"
#include <util/tsingleton.h>
#include <lsr/ls_hash.h>
#include <lsr/ls_pool.h>
#include <lsr/ls_str.h>
#include <util/gpointerlist.h>
#include <util/objpool.h>

#define VERSION_TO_LOG  "1.4.31"
#define VERSION         "1.0.0"

#define ULL_MAXLEN      24      // maximum buffer size of unsigned long long
#define STATITEM_MAXLEN 1024    // maximum buffer size for a status item (ascii)
#define KEY_MAXLEN      250     // maximum key length from memcached
#define VAL_MAXSIZE    (1024*1024)      // maximum size of value
#define MEM_MAXSIZE    (64*1024*1024)   // default maximum size of hash memory
#define USER_SIZE       1000    // Should be juse a default.
#define HASH_SIZE       500000  // Should be juse a default.

#define DSTADDR_MAXLEN (46+6)   // max dest addr + port, see INET6_ADDRSTRLEN

#define LSMC_MAXDELTATIME   60*60*24*30     // seconds in 30 days

typedef struct
{
    uint16_t len;
    char buf[1024];
} lenNbuf;

class ReplServerImpl;
class Multiplexer;
typedef struct LsMcUpdOpt LsMcUpdOpt;
typedef struct LsMcTidPkt LsMcTidPkt;
typedef struct
{
    uint64_t    get_cmds;
    uint64_t    set_cmds;
    uint64_t    touch_cmds;
    uint64_t    flush_cmds;
    uint64_t    get_hits;
    uint64_t    get_misses;
    uint64_t    touch_hits;
    uint64_t    touch_misses;
    uint64_t    delete_hits;
    uint64_t    delete_misses;
    uint64_t    incr_hits;
    uint64_t    incr_misses;
    uint64_t    decr_hits;
    uint64_t    decr_misses;
    uint64_t    cas_hits;
    uint64_t    cas_misses;
    uint64_t    cas_badval;
    uint64_t    auth_cmds;
    uint64_t    auth_errors;
} LsMcStats;

typedef struct
{
    uint8_t     x_verbose;
    uint8_t     x_withcas;
    uint8_t     x_withsasl;
    uint8_t     x_notused[1];
    uint8_t     x_dstaddr[DSTADDR_MAXLEN];
    LsMcStats   x_stats;
    union
    {
        uint64_t cas;
        uint8_t  val[1];
    } x_data[0];
} LsMcHdr;

typedef struct
{
    time_t   x_exptime;
    uint32_t x_flags;
    union
    {
        struct
        {
            uint64_t cas;
            uint8_t  val[1];
        } withcas;
        uint8_t val[1];
    } x_data[0];
} LsMcDataItem;

typedef enum
{
    MC_BINARY_REQ = 0x80,
    MC_BINARY_RES = 0x81,
    MC_INTERNAL_REQ = 0x8f
} McBinMagic;


typedef enum
{
    MC_BINCMD_GET           = 0x00,
    MC_BINCMD_SET           = 0x01,
    MC_BINCMD_ADD           = 0x02,
    MC_BINCMD_REPLACE       = 0x03,
    MC_BINCMD_DELETE        = 0x04,
    MC_BINCMD_INCREMENT     = 0x05,
    MC_BINCMD_DECREMENT     = 0x06,
    MC_BINCMD_QUIT          = 0x07,
    MC_BINCMD_FLUSH         = 0x08,
    MC_BINCMD_GETQ          = 0x09,
    MC_BINCMD_NOOP          = 0x0a,
    MC_BINCMD_VERSION       = 0x0b,
    MC_BINCMD_GETK          = 0x0c,
    MC_BINCMD_GETKQ         = 0x0d,
    MC_BINCMD_APPEND        = 0x0e,
    MC_BINCMD_PREPEND       = 0x0f,
    MC_BINCMD_STAT          = 0x10,
    MC_BINCMD_SETQ          = 0x11,
    MC_BINCMD_ADDQ          = 0x12,
    MC_BINCMD_REPLACEQ      = 0x13,
    MC_BINCMD_DELETEQ       = 0x14,
    MC_BINCMD_INCREMENTQ    = 0x15,
    MC_BINCMD_DECREMENTQ    = 0x16,
    MC_BINCMD_QUITQ         = 0x17,
    MC_BINCMD_FLUSHQ        = 0x18,
    MC_BINCMD_APPENDQ       = 0x19,
    MC_BINCMD_PREPENDQ      = 0x1a,
    MC_BINCMD_VERBOSITY     = 0x1b,
    MC_BINCMD_TOUCH         = 0x1c,
    MC_BINCMD_GAT           = 0x1d,
    MC_BINCMD_GATQ          = 0x1e,
    MC_BINCMD_SASL_LIST     = 0x20,
    MC_BINCMD_SASL_AUTH     = 0x21,
    MC_BINCMD_SASL_STEP     = 0x22,
    MC_BINCMD_GATK          = 0x23,
    MC_BINCMD_GATKQ         = 0x24
} McBinCmd;

#define LSMC_USECAS     0x0200      // cas field present in db
#define LSMC_WITHCAS    0x0100      // command with cas
#define LSMC_CMDMASK    0x00ff

typedef enum
{
    MC_BINSTAT_SUCCESS              = 0x00,
    MC_BINSTAT_KEYENOENT            = 0x01,
    MC_BINSTAT_KEYEEXISTS           = 0x02,
    MC_BINSTAT_E2BIG                = 0x03,
    MC_BINSTAT_EINVAL               = 0x04,
    MC_BINSTAT_NOTSTORED            = 0x05,
    MC_BINSTAT_DELTABADVAL          = 0x06,
    MC_BINSTAT_AUTHERROR            = 0x20,
    MC_BINSTAT_AUTHCONTINUE         = 0x21,
    MC_BINSTAT_REMOTEERROR          = 0x30,
    MC_BINSTAT_UNKNOWNCMD           = 0x81,
    MC_BINSTAT_ENOMEM               = 0x82,
    MC_BINSTAT_NOT_SUPPORTED        = 0x83,
    MC_BINSTAT_INTERNAL_ERROR       = 0x84,
    MC_BINSTAT_BUSY                 = 0x85,
    MC_BINSTAT_TEMPORARY_FAILURE    = 0x86,
    
} McBinStat;

typedef struct
{
    uint8_t     magic;
    uint8_t     opcode;
    uint16_t    keylen;
    uint8_t     extralen;
    uint8_t     datatype;
    uint16_t    status;
    uint32_t    totbody;
    uint32_t    opaque;
    uint64_t    cas;
} McBinCmdHdr;

typedef union
{
    struct
    {
        uint64_t    delta;
        uint64_t    initval;
        uint32_t    exptime;
    } incrdecr;
    struct
    {
        uint32_t    flags;
        uint32_t    exptime;
    } value;
    struct
    {
        uint32_t    level;
    } verbosity;
    struct
    {
        uint32_t    exptime;
    } touch;
} McBinReqExtra;

typedef union
{
    struct
    {
        uint64_t    newval;
    } incrdecr;
    struct
    {
        uint32_t    flags;
    } value;
} McBinResExtra;

typedef struct LsMcParms_s
{
    bool            m_usecas;
    bool            m_usesasl;
    bool            m_anonymous;
    bool            m_byUser;
    bool            m_nomemfail;    // fail if nomem (rather than purge)
    uint32_t        m_iValMaxSz;
    LsShmXSize_t    m_iMemMaxSz;
    uint32_t        m_userSize;
    uint32_t        m_hashSize;
    bool            m_dbgValidate;
} LsMcParms;

class LsMemcache;
typedef struct
{
    const char *cmd;
    int len;
    int arg;
    int (*func)(LsMemcache *pThis, ls_strpair_t *pInput, int arg, 
                MemcacheConn *pConn);
} LsMcCmdFunc;


typedef ObjPool<ls_str_t> LsMcStrPool;
typedef TPointerList<ls_str_t> LsMcStrList;
class LsMemcache : public TSingleton<LsMemcache>
{
    friend class TSingleton<LsMemcache>;
private:
    static bool multiValidate(LsMcHashSlice *pSlice, MemcacheConn *pConn);
     
    static int  multiInitFunc(LsMcHashSlice *pSlice, MemcacheConn *pConn, 
                              void *pArg);
    static int  multiConnFunc(LsMcHashSlice *pSlice, MemcacheConn *pConn, 
                              void *pArg);
    static int  multiMultiplexerFunc(LsMcHashSlice *pSlice, MemcacheConn *pConn, 
                                     void *pArg);
    static int  multiStatFunc(LsMcHashSlice *pSlice, MemcacheConn *pConn, 
                              void *pArg);
    static int  multiStatResetFunc(LsMcHashSlice *pSlice, MemcacheConn *pConn, 
                                   void *pArg);
    static int  multiFlushFunc(LsMcHashSlice *pSlice, MemcacheConn *pConn, 
                               void *pArg);
    static int  multiVerboseFunc(LsMcHashSlice *pSlice, MemcacheConn *pConn, 
                                 void *pArg)
    {
        if (!pConn)
            pConn = pSlice->m_pConnSlaveToMaster;
        if (!multiValidate(pSlice, pConn))
            return LS_OK; // Deal with it later
            
        LsShmOffset_t iHdrOff = pConn->getHdrOff();
        if (iHdrOff != 0)
            ((LsMcHdr *)pConn->getHash()->offset2ptr(iHdrOff))->x_verbose = 
                (uint8_t)(long)pArg;
        return LS_OK;
    }
public:
    ~LsMemcache() { }

    static void setConfigMultiUser(bool multiUser) 
    { m_bConfigMultiUser = multiUser; }
    static bool getConfigMultiUser(void)    { return m_bConfigMultiUser; }
    static void setConfigReplication(bool replication)
    { m_bConfigReplication = replication; }
    static bool getConfigReplication(void)  { return m_bConfigReplication; }
    
    int  initMcShm(int iCnt, const char **ppPathName,
        const char *pHashName, LsMcParms *pParms);
    int  initMcShm(const char *pDirName, const char *pShmName,
        const char *pHashName, LsMcParms *pParms);
    int  initMcEvents();
    int  reinitMcConn(Multiplexer *pMultiplexer);

    LsShmHash *getReplHash(int indx) 
    {
        return ((m_pHashMulti != NULL) ?
            m_pHashMulti->indx2hashSlice(indx)->m_hashByUser.getHash(NULL) 
            : NULL);
    }
    
    void DEBUG();

    int  processCmd(char *pStr, int iLen, MemcacheConn *pConn);
    int  doDataUpdate(uint8_t *pBuf, MemcacheConn *pConn);
    McBinStat  chkMemSz(MemcacheConn *pConn, int arg);
    int  processBinCmd(uint8_t *pBinBuf, int iLen, MemcacheConn *pConn);
    int  processInternal(uint8_t *pBuf, int iLen, MemcacheConn *pConn);
    void putWaitQ(MemcacheConn *pConn);
    void processWaitQ();
    void onTimer();

    static inline LsMcDataItem *mcIter2data(
        LsShmHElem *iter, int useCas, uint8_t **pValPtr, int *pValLen)
    {
        LsMcDataItem *pItem;
        pItem = (LsMcDataItem *)iter->getVal();
        *pValLen = iter->getValLen() - sizeof(*pItem);
        if (useCas)
        {
            *pValLen -= sizeof(pItem->x_data->withcas.cas);
            *pValPtr = pItem->x_data->withcas.val;
        }
        else
        {
            *pValPtr = pItem->x_data->val;
        }
        LS_DBG_M("iter2data, valLen: %d, pData: %p\n", *pValLen, *pValPtr);
        return pItem;
    }

    static inline LsMcDataItem *mcIter2num(
        LsShmHElem * iter, int useCas, char *pNumBuf, uint64_t *pNumVal)
    {
        uint64_t ullnum;
        int valLen;
        uint8_t *valPtr;
        LsMcDataItem *pItem = mcIter2data(iter, useCas, &valPtr, &valLen);
        if ((valLen <= 0) || (valLen > ULL_MAXLEN))
            return NULL;
        memcpy(pNumBuf, (char *)valPtr, valLen);
        pNumBuf[valLen] = '\0';
        if (!myStrtoull(pNumBuf, &ullnum))
            return NULL;
        *pNumVal = (uint64_t)ullnum;
        return pItem;
    }

    static inline bool isExpired(LsMcDataItem *pItem)
    {
        return ((pItem->x_exptime != 0)
            && (pItem->x_exptime <= time((time_t *)NULL)));
    }

    static int tidGetNxtItems(LsShmHash *pHash, uint64_t *pTidLast,
                              uint8_t *pBuf, int iBufSz);
    static int tidSetItems(LsShmHash *pHash, uint8_t *pBuf, int iBufsz);

#ifdef USE_SASL
    inline bool isAuthenticated(uint8_t cmd, MemcacheConn *pConn)
    {
        return ((cmd == MC_BINCMD_SASL_LIST)
            || (cmd == MC_BINCMD_SASL_AUTH)
            || (cmd == MC_BINCMD_SASL_STEP)
            || (cmd == MC_BINCMD_VERSION)
            || pConn->GetSasl()->isAuthenticated());
    }
#endif

    inline bool chkItemSize(uint32_t size)
    {   return (!m_mcparms.m_iValMaxSz || size <= m_mcparms.m_iValMaxSz);   }

    uint8_t  getVerbose(MemcacheConn *pConn)
    {
        /* Can be called REALLY early where nothing in the connection is setup 
         * yet.  Just be fault tolerant. */
        if ((!pConn) || (!pConn->getSlice()) || (!pConn->getHash()))
            return 0;
        if (pConn->getHdrOff() != 0)
        {
            uint8_t verbose = ((LsMcHdr *)pConn->getHash()->offset2ptr(
                        pConn->getHdrOff()))->x_verbose;
            if (verbose)
                LS_DBG_M("getVerbose: %d\n", verbose);
            return verbose;
        }
        return 0;
    }

    void     setVerbose(MemcacheConn *pConn, uint8_t iLevel)
    {
        if (m_pHashMulti != NULL)
            m_pHashMulti->foreach(multiVerboseFunc, pConn, (void *)(long)iLevel);
        else if (pConn->getHdrOff() != 0)
            ((LsMcHdr *)pConn->getHash()->offset2ptr(pConn->getHdrOff()))->x_verbose = iLevel;
#ifdef USE_SASL
        LsMcSasl::verbose = iLevel;
#endif
        return;
    }

    void lock(MemcacheConn *pConn)
    {
        pConn->getHash()->disableAutoLock();
        pConn->getHash()->lockChkRehash();
    }

    void unlock(MemcacheConn *pConn)
    {
        pConn->getHash()->unlock();
        pConn->getHash()->enableAutoLock();
    }

    bool useMulti()
    {   return ((m_pHashMulti != NULL) && (m_pHashMulti->getMultiCnt() > 1));  }

    void setMultiplexer(Multiplexer *pMultiplexer)
    {   if (m_pHashMulti != NULL) m_pHashMulti->setMultiplexer(pMultiplexer);  }

    int setSliceDst(int which, char *pAddr, MemcacheConn *pConn);

    int clearSlice(int which, MemcacheConn *pConn)
    {
        if ((m_pHashMulti == NULL) || (which >= m_pHashMulti->getMultiCnt()))
            return LS_FAIL;
        multiFlushFunc(m_pHashMulti->indx2hashSlice(which), pConn, NULL);
        return LS_OK;
    }
    void m_pHash();
    static void setDbgValidate(bool dbgValidate) { m_dbgValidate = dbgValidate; }

private:
    LsMemcache();
    LsMemcache(const LsMemcache &other);
    LsMemcache &operator=(const LsMemcache &other);

private:
    
    static int  getNxtTidItem(LsShmHash *pHash, uint64_t *pTidLast,
                              void **ppBlk, LsMcTidPkt *pPkt, int iBufSz);
    static void iter2tidPack(LsShmHElem *pElem, uint64_t *pTid,
                             LsMcTidPkt *pPkt, int totSz);
    static void del2tidPack(uint64_t *pDel, uint64_t *pTid, LsMcTidPkt *pPkt,
                            int totSz);
    static int  setTidItem(LsShmHash *pHash, LsMcTidPkt *pPkt);
    static int  delTidItem(LsShmHash *pHash, LsMcTidPkt *pPkt, uint8_t *pBuf,
                           int iBufsz);
    static int tidAddPktSize(LsShmHElem *pElem);
    static int tidDelPktSize();

    void     notifyChange(MemcacheConn *pConn);
    void     ackNoreply(MemcacheConn *pConn);
    void     respond(const char *str, MemcacheConn *pConn);
    void     sendResult(MemcacheConn *pConn, const char *fmt, ...);
    void     binRespond(uint8_t *buf, int cnt, MemcacheConn *pConn);
    lenNbuf *buf2lenNbuf(char *buf, int *pLen, lenNbuf *pRet)
    {
        ::memcpy((void *)pRet->buf, buf, *pLen);
        *pLen += sizeof(pRet->len);
        pRet->len = (uint16_t)htons(*pLen);
        return pRet;
    }

    uint64_t getCas(MemcacheConn *pConn)
    {
        return ((pConn->getHdrOff() != 0) ?
            ++((LsMcHdr *)pConn->getHash()->offset2ptr(pConn->getHdrOff()))->x_data->cas : 0);
    }

    void     saveCas(LsMcDataItem *pItem)
    {
        m_rescas = (m_mcparms.m_usecas ? pItem->x_data->withcas.cas : 0);
    }

    LsMcCmdFunc *getCmdFunction(const char *pCmd, int len);
    void         dataItemUpdate(uint8_t *pBuf, MemcacheConn *pConn);

    static int   doCmdTest1(LsMemcache *pThis,
                            ls_strpair_t *pInput, int arg, MemcacheConn *pConn);
    static int   doCmdTest2(LsMemcache *pThis,
                            ls_strpair_t *pInput, int arg, MemcacheConn *pConn);
    static int   doCmdPrintTids(LsMemcache *pThis,
                            ls_strpair_t *pInput, int arg, MemcacheConn *pConn);
    static int   doCmdGet(LsMemcache *pThis,
                            ls_strpair_t *pInput, int arg, MemcacheConn *pConn);
    static int   doCmdUpdate(LsMemcache *pThis,
                            ls_strpair_t *pInput, int arg, MemcacheConn *pConn);
    static int   doCmdArithmetic(LsMemcache *pThis,
                            ls_strpair_t *pInput, int arg, MemcacheConn *pConn);
    static int   doCmdDelete(LsMemcache *pThis,
                            ls_strpair_t *pInput, int arg, MemcacheConn *pConn);
    static int   doCmdTouch(LsMemcache *pThis,
                            ls_strpair_t *pInput, int arg, MemcacheConn *pConn);
    static int   doCmdStats(LsMemcache *pThis,
                            ls_strpair_t *pInput, int arg, MemcacheConn *pConn);
    static int   doCmdFlush(LsMemcache *pThis,
                            ls_strpair_t *pInput, int arg, MemcacheConn *pConn);
    static int   doCmdVersion(LsMemcache *pThis,
                            ls_strpair_t *pInput, int arg, MemcacheConn *pConn);
    static int   doCmdQuit(LsMemcache *pThis,
                            ls_strpair_t *pInput, int arg, MemcacheConn *pConn);
    static int   doCmdVerbosity(LsMemcache *pThis,
                            ls_strpair_t *pInput, int arg, MemcacheConn *pConn);
    static int   doCmdClear(LsMemcache *pThis,
                            ls_strpair_t *pInput, int arg, MemcacheConn *pConn);
    static int   notImplemented(LsMemcache *pThis,
                            ls_strpair_t *pInput, int arg, MemcacheConn *pConn);

    uint8_t  setupNoreplyCmd(uint8_t cmd);
    uint8_t *setupBinCmd(McBinCmdHdr *pHdr, uint8_t cmd, LsMcUpdOpt *pOpt, 
                         MemcacheConn *pConn);
    bool     isRemoteEligible(uint8_t cmd);

    void     doBinGet(McBinCmdHdr *pHdr, uint8_t cmd, bool doTouch, 
                      MemcacheConn *pConn);
    int      doBinDataUpdate(uint8_t *pBuf, McBinCmdHdr *pHdr, 
                             MemcacheConn *pConn);
    void     doBinDelete(McBinCmdHdr *pHdr, MemcacheConn *pConn);
    void     doBinArithmetic(McBinCmdHdr *pHdr, uint8_t cmd, LsMcUpdOpt *pOpt,
                             MemcacheConn *pConn);
    void     doBinFlush(McBinCmdHdr *pHdr, MemcacheConn *pConn);
    void     doBinVersion(McBinCmdHdr *pHdr, MemcacheConn *pConn);
    void     doBinStats(McBinCmdHdr *pHdr, MemcacheConn *pConn);
    void     doBinStat1str(McBinCmdHdr *pResHdr,
                const char *pkey, char *pval, MemcacheConn *pConn);
    void     doBinStat1p64(McBinCmdHdr *pResHdr,
                const char *pkey, uint64_t *pval, MemcacheConn *pConn);
    void     doBinStat1dot6(McBinCmdHdr *pResHdr,
                const char *pkey, long val1, long val2, MemcacheConn *pConn);
    void     doBinStat1Send(McBinCmdHdr *pResHdr, const char *pkey, int bodylen, 
                            MemcacheConn *pConn);
    void     doBinSaslList(McBinCmdHdr *pHdr, MemcacheConn *pConn);
    void     doBinSaslAuth(McBinCmdHdr *pHdr, MemcacheConn *pConn);
    void     doBinSaslStep(McBinCmdHdr *pHdr, MemcacheConn *pConn);

    void     setupBinResHdr(McBinCmdHdr *pHdr,
                uint8_t extralen, uint16_t keylen, uint32_t totbody,
                uint16_t status, uint8_t *pBinBuf, MemcacheConn *pConn);
    void     binOkRespond(McBinCmdHdr *pHdr, MemcacheConn *pConn);
    void     binErrRespond(McBinCmdHdr *pHdr, McBinStat err, MemcacheConn *pConn);

    char    *advToken(char *pStr, char *pStrEnd, char **pTokPtr, size_t *pTokLen);
    bool     myStrtol(const char *pStr, int32_t *pVal);
    bool     myStrtoul(const char *pStr, uint32_t *pVal);
    bool     myStrtoll(const char *pStr, int64_t *pVal);
    static bool  myStrtoull(const char *pStr, uint64_t *pVal);
    void     clearKeyList();

    bool     chkEndToken(char *pStr, char *pStrEnd)
    {
        char *tokPtr;
        size_t tokLen;
        advToken(pStr, pStrEnd, &tokPtr, &tokLen);
        return (tokLen <= 0);
    }

    static void  setItemExptime(LsMcDataItem *pItem, uint32_t exptime)
    {
        if ((exptime != 0) && (exptime <= LSMC_MAXDELTATIME))
            exptime += time((time_t *)NULL);
        pItem->x_exptime = (time_t)exptime;
    }

    int   parmAdjLen(int valLen)
    {
        return (m_mcparms.m_usecas ?
            (valLen + sizeof(m_item) + sizeof(m_item.x_data->withcas.cas)) :
            (valLen + sizeof(m_item)));
    }

    LsMcHashSlice *setSlice(const void *pKey, int iLen, MemcacheConn *pConn);
    LsMcHashSlice *canProcessNow(const void *pKey, int iLen, MemcacheConn *pConn)
    {
        LsMcHashSlice *pSlice = setSlice(pKey, iLen, pConn);
        if (!getConfigReplication())
            return pSlice;
        if ((!pSlice->m_hashByUser.getHash(NULL)->isTidMaster())
            && (pSlice->m_pConnSlaveToMaster->GetConnFlags() & CS_REMBUSY))
        {
            putWaitQ(pConn);
            return NULL;
        }
        return pSlice;
    }

    bool     isSliceRemote(LsMcHashSlice *pSlice);
    bool     fwdToRemote(LsMcHashSlice *pSlice, char *pNxt, MemcacheConn *pConn);
    bool     fwdBinToRemote(LsMcHashSlice *pSlice, McBinCmdHdr *pHdr, 
                            MemcacheConn *pConn);
    void     fwdCommand(LsMcHashSlice *pSlice, const char *buf, int len, 
                        MemcacheConn *pConn);

    bool statsAggregate(MemcacheConn *pConn)
    { 
        return ((useMulti()) && ((!getConfigMultiUser()) || 
                                 (!pConn->GetSasl()->isAuthenticated()))); 
    };

    void statSetCmd(MemcacheConn *pConn)
    {
        if (pConn->getHdrOff() != 0)
            ++((LsMcHdr *)pConn->getConnStats()->getHash()->
                offset2ptr(pConn->getHdrOff()))->x_stats.set_cmds;
    }

    void statFlushCmd(MemcacheConn *pConn)
    {
        if (pConn->getHdrOff() != 0)
            ++((LsMcHdr *)pConn->getConnStats()->getHash()->
                offset2ptr(pConn->getHdrOff()))->x_stats.flush_cmds;
    }

    void statGetHit(MemcacheConn *pConn)
    {
        if (pConn->getHdrOff() != 0)
            ++((LsMcHdr *)pConn->getConnStats()->getHash()->
                offset2ptr(pConn->getHdrOff()))->x_stats.get_hits;
    }

    void statGetMiss(MemcacheConn *pConn)
    {
        if (pConn->getHdrOff() != 0)
            ++((LsMcHdr *)pConn->getConnStats()->getHash()->
                offset2ptr(pConn->getHdrOff()))->x_stats.get_misses;
    }

    void statTouchHit(MemcacheConn *pConn)
    {
        if (pConn->getHdrOff() != 0)
            ++((LsMcHdr *)pConn->getConnStats()->getHash()->
                offset2ptr(pConn->getHdrOff()))->x_stats.touch_hits;
    }

    void statTouchMiss(MemcacheConn *pConn)
    {
        if (pConn->getHdrOff() != 0)
            ++((LsMcHdr *)pConn->getConnStats()->getHash()->
                offset2ptr(pConn->getHdrOff()))->x_stats.touch_misses;
    }

    void statDeleteHit(MemcacheConn *pConn)
    {
        if (pConn->getHdrOff() != 0)
            ++((LsMcHdr *)pConn->getConnStats()->getHash()->
                offset2ptr(pConn->getHdrOff()))->x_stats.delete_hits;
    }

    void statDeleteMiss(MemcacheConn *pConn)
    {
        if (pConn->getHdrOff() != 0)
            ++((LsMcHdr *)pConn->getConnStats()->getHash()->
                offset2ptr(pConn->getHdrOff()))->x_stats.delete_misses;
    }

    void statIncrHit(MemcacheConn *pConn)
    {
        if (pConn->getHdrOff() != 0)
            ++((LsMcHdr *)pConn->getConnStats()->getHash()->
                offset2ptr(pConn->getHdrOff()))->x_stats.incr_hits;
    }

    void statIncrMiss(MemcacheConn *pConn)
    {
        if (pConn->getHdrOff() != 0)
            ++((LsMcHdr *)pConn->getConnStats()->getHash()->
                offset2ptr(pConn->getHdrOff()))->x_stats.incr_misses;
    }

    void statDecrHit(MemcacheConn *pConn)
    {
        if (pConn->getHdrOff() != 0)
            ++((LsMcHdr *)pConn->getConnStats()->getHash()->
                offset2ptr(pConn->getHdrOff()))->x_stats.decr_hits;
    }

    void statDecrMiss(MemcacheConn *pConn)
    {
        if (pConn->getHdrOff() != 0)
            ++((LsMcHdr *)pConn->getConnStats()->getHash()->
                offset2ptr(pConn->getHdrOff()))->x_stats.decr_misses;
    }

    void statCasHit(MemcacheConn *pConn)
    {
        if (pConn->getHdrOff() != 0)
            ++((LsMcHdr *)pConn->getConnStats()->getHash()->
                offset2ptr(pConn->getHdrOff()))->x_stats.cas_hits;
    }

    void statCasMiss(MemcacheConn *pConn)
    {
        if (pConn->getHdrOff() != 0)
            ++((LsMcHdr *)pConn->getConnStats()->getHash()->
                offset2ptr(pConn->getHdrOff()))->x_stats.cas_misses;
    }

    void statCasBad(MemcacheConn *pConn)
    {
        if (pConn->getHdrOff() != 0)
            ++((LsMcHdr *)pConn->getConnStats()->getHash()->
                offset2ptr(pConn->getHdrOff()))->x_stats.cas_badval;
    }

    void statAuthCmd(MemcacheConn *pConn)
    {
        if (pConn->getHdrOff() != 0)
            ++((LsMcHdr *)pConn->getConnStats()->getHash()->
                offset2ptr(pConn->getHdrOff()))->x_stats.auth_cmds;
    }

    void statAuthErr(MemcacheConn *pConn)
    {
        if (pConn->getHdrOff() != 0)
            ++((LsMcHdr *)pConn->getConnStats()->getHash()->
                offset2ptr(pConn->getHdrOff()))->x_stats.auth_errors;
    }
    LsShmHash::iteroffset doHashUpdate(ls_strpair_s *m_parms, 
                                       LsMcUpdOpt *updOpt,
                                       MemcacheConn *pConn);
    LsShmHash::iteroffset doHashInsert(ls_strpair_s *m_parms, 
                                       LsMcUpdOpt *updOpt,
                                       MemcacheConn *pConn);
    
    void *dbgValidateKeyData(char *key, size_t keyLen, void *data, size_t dataLen,
                             char **keyOut)
    {
        size_t len = keyLen + 1 + sizeof(size_t) + dataLen;
        int pos;
        char *ptr = (char *)ls_palloc(len);
        if (!ptr)
            return NULL;
        memcpy(ptr, key, keyLen);
        *(ptr + keyLen) = 0;
        pos = keyLen + 1;
        memcpy(ptr + pos, &dataLen, sizeof(dataLen));
        pos += sizeof(dataLen);
        memcpy(ptr + pos, data, dataLen);
        *keyOut = ptr;
        return ptr;
    }
    void *dbgValidateParseData(void *data, char **key, size_t *dataLen)
    {
        char *ptr = (char *)data;
        int pos;
        *key = ptr;
        pos = strlen(ptr) + 1;
        *dataLen = *(size_t *)(ptr + pos);
        pos += sizeof(size_t);
        return (void *)(ptr + pos);
    }
    
    void dbgValidateAdd(char *key, size_t keyLen, void *data, size_t dataLen)
    {
        char *keyPtr;
        if (!m_dbgValidate || !m_dbgValidateHash)
            return;
        void *ptr = dbgValidateKeyData(key, keyLen, data, dataLen, &keyPtr);
        if (!ptr)
        {
            LS_ERROR("Debug Validation error - can't allocate data\n");
            return;
        }
        if (!ls_hash_update(m_dbgValidateHash, keyPtr, ptr))
        {
            LS_ERROR("Debug Validation error - can't allocate hash entry\n");
            ls_pfree(ptr);
            return;
        }
        LS_DBG_M("Debug Validation, added key: %s, len: %ld, datalen: %ld\n", keyPtr, keyLen, dataLen);
    }
    void dbgValidateGet(char *key, size_t keyLen, void *data, size_t dataLen)
    {
        if (!m_dbgValidate || !m_dbgValidateHash)
            return;
        char strKey[keyLen + 1];
        memcpy(strKey, key, keyLen);
        strKey[keyLen] = 0;
        ls_hashelem_t *elem = ls_hash_find(m_dbgValidateHash, strKey);
        if (!elem)
        {
            LS_NOTICE("Debug Validation error (get): expected to find: %s\n", strKey);
            return;
        }
        char *hashKey;
        size_t hashDataLen;
        void *hashData = dbgValidateParseData(ls_hash_getdata(elem), &hashKey,
                                              &hashDataLen);
        if (hashDataLen != dataLen)
        {
            LS_NOTICE("Debug Validation error: for key: %s:%s, expected len: %ld, got %ld\n",
                      strKey, hashKey, dataLen, hashDataLen);
            return;
        }
        if (memcmp(data, hashData, hashDataLen))
        {
            LS_NOTICE("Debug Validation error: for key: %s, got correct len: %ld, but data failure\n",
                      strKey, dataLen);
            return;
        }
        LS_DBG_M("Debug Validation for key: %s validated\n", strKey);
    }
    void dbgValidateDelete(char *key, size_t keyLen)
    {
        if (!m_dbgValidate || !m_dbgValidateHash)
            return;
        char strKey[keyLen + 1];
        memcpy(strKey, key, keyLen);
        strKey[keyLen] = 0;
        ls_hashelem_t *elem = ls_hash_find(m_dbgValidateHash, strKey);
        if (!elem)
        {
            LS_NOTICE("Debug Validation error (delete): expected to find: %s\n", strKey);
            return;
        }
        ls_hash_erase(m_dbgValidateHash, elem);
        LS_DBG_M("Debug Validation for key: %s deleted\n", strKey);
        ls_pfree(ls_hash_getdata(elem));
    }
        
        

    static LsMcCmdFunc s_LsMcCmdFuncs[];

    LsMcHashMulti          *m_pHashMulti;
    MemcacheConn           *m_pWaitQ;
    MemcacheConn           *m_pWaitTail;
    char                   *m_pStrt;    // start of ascii command buffer
    ls_strpair_t            m_parms;
    LsShmHKey               m_hkey;
    LsMcDataItem            m_item;     // cmd in progress
    LsShmHash::iteroffset   m_iterOff;  // cmd in progress
    uint64_t                m_rescas;   // cmd in progress
    int                     m_needed;
    uint8_t                 m_retcode;
    bool                    m_noreply;
    LsMcParms               m_mcparms;
    LsMcStrPool             m_keyPool;
    LsMcStrList             m_keyList;
    ls_str_t                m_key;
    
    static bool             m_bConfigMultiUser;
    static bool             m_bConfigReplication;
    static bool             m_dbgValidate;
    static ls_hash_t       *m_dbgValidateHash;
    static const char      *m_pchConfigUser;
    static const char      *m_pchConfigGroup;
};

#endif // LSSHMMEMCACHE_H

