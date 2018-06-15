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
#include "memcacheconn.h"
#include <shm/lsshmhash.h>
#include "lsmchashmulti.h"
#include "lsmcsasl.h"
#include <util/tsingleton.h>
#include <lsr/ls_str.h>
#include <util/gpointerlist.h>
#include <util/objpool.h>

#define VERSION         "1.0.0"

#define ULL_MAXLEN      24      // maximum buffer size of unsigned long long
#define STATITEM_MAXLEN 1024    // maximum buffer size for a status item (ascii)
#define KEY_MAXLEN      246     // maximum key length from memcached (- 4 bytes overhead)
#define VAL_MAXSIZE    (1024*1024)      // maximum size of value
#define MEM_MAXSIZE    (64*1024*1024)   // default maximum size of hash memory

#define DSTADDR_MAXLEN (46+6)   // max dest addr + port, see INET6_ADDRSTRLEN

#define LSMC_MAXDELTATIME   60*60*24*30     // seconds in 30 days

extern uint8_t s_user_in_key;

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

#define LSMEMCACHE_WITH_SASL_USER   3

typedef struct
{
    uint8_t     x_verbose;
    uint8_t     x_withcas;
    uint8_t     x_withsasl; // 1 is just sasl 3 is sasl + user
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
    MC_BINSTAT_SUCCESS      = 0x00,
    MC_BINSTAT_KEYENOENT    = 0x01,
    MC_BINSTAT_KEYEEXISTS   = 0x02,
    MC_BINSTAT_E2BIG        = 0x03,
    MC_BINSTAT_EINVAL       = 0x04,
    MC_BINSTAT_NOTSTORED    = 0x05,
    MC_BINSTAT_DELTABADVAL  = 0x06,
    MC_BINSTAT_AUTHERROR    = 0x20,
    MC_BINSTAT_AUTHCONTINUE = 0x21,
    MC_BINSTAT_REMOTEERROR  = 0x30,
    MC_BINSTAT_UNKNOWNCMD   = 0x81,
    MC_BINSTAT_ENOMEM       = 0x82
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

#define LSMEMCACHE_SASL_WITH_USER 3

typedef struct LsMcParms_s
{
    bool            m_usecas;
    uint8_t         m_usesasl;
    bool            m_nomemfail;    // fail if nomem (rather than purge)
    uint32_t        m_iValMaxSz;
    LsShmXSize_t    m_iMemMaxSz;
} LsMcParms;

class LsMemcache;
typedef struct
{
    const char *cmd;
    int len;
    int arg;
    int (*func)(LsMemcache *pThis, int arg);
} LsMcCmdFunc;


typedef ObjPool<ls_str_t> LsMcStrPool;
typedef TPointerList<ls_str_t> LsMcStrList;
class LsMemcache : public TSingleton<LsMemcache>
{
    friend class TSingleton<LsMemcache>;
public:
    ~LsMemcache();

    void freeKey();
    int  initMcShm(int iCnt, const char **ppPathName,
        const char *pHashName, LsMcParms *pParms);
    int  initMcShm(const char *pDirName, const char *pShmName,
        const char *pHashName, LsMcParms *pParms);
    int  initMcEvents();
    int  reinitMcConn(Multiplexer *pMultiplexer);

    LsShmHash *getHash() const
    {   return m_pHash;  }

    LsShmHash *getHash(int indx) const
    {
        return ((m_pHashMulti != NULL) ?
            m_pHashMulti->indx2hashSlice(indx)->m_pHash : NULL);
    }
    void DEBUG();
    void setConn(MemcacheConn *pConn)
    {   m_pConn = pConn;  }

    bool setDiskKey();
    bool resetDiskKey();
    int  processCmd(char *pStr, int iLen);
    int  doDataUpdate(uint8_t *pBuf);
    McBinStat  chkMemSz(int arg);
    int  processBinCmd(uint8_t *pBinBuf, int iLen);
    int  processInternal(uint8_t *pBuf, int iLen);
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
    static int tidSetItems(LsMemcache *pThis, LsShmHash *pHash, uint8_t *pBuf, 
                           int iBufsz);

#ifdef USE_SASL
    inline bool isAuthenticated(uint8_t cmd)
    {
        return ((cmd == MC_BINCMD_SASL_LIST)
            || (cmd == MC_BINCMD_SASL_AUTH)
            || (cmd == MC_BINCMD_SASL_STEP)
            || (cmd == MC_BINCMD_VERSION)
            || ((m_pConn) && (m_pConn->GetSasl()) && m_pConn->GetSasl()->isAuthenticated()));
    }
#endif

    inline bool chkItemSize(uint32_t size)
    {   return (size <= m_mcparms.m_iValMaxSz);   }

    uint8_t  getVerbose()
    {
        return ((m_iHdrOff != 0) ?
            ((LsMcHdr *)m_pHash->offset2ptr(m_iHdrOff))->x_verbose: 0);
    }

    void     setVerbose(uint8_t iLevel)
    {
        if (m_pHashMulti != NULL)
            m_pHashMulti->foreach(multiVerboseFunc, (void *)(long)iLevel);
        else if (m_iHdrOff != 0)
            ((LsMcHdr *)m_pHash->offset2ptr(m_iHdrOff))->x_verbose = iLevel;
#ifdef USE_SASL
        LsMcSasl::verbose = iLevel;
#endif
        return;
    }

    void lock()
    {
        m_pHash->disableAutoLock();
        m_pHash->lockChkRehash();
    }

    void unlock()
    {
        m_pHash->unlock();
        m_pHash->enableAutoLock();
    }

    bool useMulti()
    {   return ((m_pHashMulti != NULL) && (m_pHashMulti->getMultiCnt() > 1));  }

    void setMultiplexer(Multiplexer *pMultiplexer)
    {   if (m_pHashMulti != NULL) m_pHashMulti->setMultiplexer(pMultiplexer);  }

    int setSliceDst(int which, char *pAddr);

    int clearSlice(int which)
    {
        if ((m_pHashMulti == NULL) || (which >= m_pHashMulti->getMultiCnt()))
            return LS_FAIL;
        multiFlushFunc(m_pHashMulti->indx2hashSlice(which), NULL);
        return LS_OK;
    }

//     LOG4CXX_NS::Logger *getLogger()
//     {   return m_pHash->getLogger();  }

private:
    LsMemcache();
    LsMemcache(const LsMemcache &other);
    LsMemcache &operator=(const LsMemcache &other);

private:
     
    static int  multiInitFunc(LsMcHashSlice *pSlice, void *pArg);
    static int  multiConnFunc(LsMcHashSlice *pSlice, void *pArg);
    static int  multiMultiplexerFunc(LsMcHashSlice *pSlice, void *pArg);
    static int  multiStatFunc(LsMcHashSlice *pSlice, void *pArg);
    static int  multiStatResetFunc(LsMcHashSlice *pSlice, void *pArg);
    static int  multiFlushFunc(LsMcHashSlice *pSlice, void *pArg);
    static int  multiVerboseFunc(LsMcHashSlice *pSlice, void *pArg)
    {
        LsShmHash *pHash = pSlice->m_pHash;
        LsShmOffset_t iHdrOff = pSlice->m_iHdrOff;
        if (iHdrOff != 0)
            ((LsMcHdr *)pHash->offset2ptr(iHdrOff))->x_verbose = (uint8_t)(long)pArg;
        return LS_OK;
    }

    static int  getNxtTidItem(LsShmHash *pHash, uint64_t *pTidLast,
                              void **ppBlk, LsMcTidPkt *pPkt, int iBufSz);
    static void iter2tidPack(LsShmHElem *pElem, uint64_t *pTid,
                             LsMcTidPkt *pPkt, int totSz);
    static void del2tidPack(uint64_t *pDel, uint64_t *pTid, LsMcTidPkt *pPkt,
                            int totSz);
    static int  setTidItem(LsMemcache *pThis, LsShmHash *pHash, LsMcTidPkt *pPkt);
    static int  delTidItem(LsMemcache *pThis, LsShmHash *pHash, LsMcTidPkt *pPkt, 
                           uint8_t *pBuf, int iBufsz);
    static int tidAddPktSize(LsShmHElem *pElem);
    static int tidDelPktSize();

    bool     keyDiskWire()
    { return ((m_parmsKeyDisk.ptr) && (m_parmsKeyDisk.ptr != m_parmsKeyWire.ptr)); }
    
    void     notifyChange();
    void     ackNoreply();
    void     respond(const char *str);
    void     sendResult(const char *fmt, ...);
    void     binRespond(uint8_t *buf, int cnt);
    lenNbuf *buf2lenNbuf(char *buf, int *pLen, lenNbuf *pRet)
    {
        ::memcpy((void *)pRet->buf, buf, *pLen);
        *pLen += sizeof(pRet->len);
        pRet->len = (uint16_t)htons(*pLen);
        return pRet;
    }

    uint64_t getCas()
    {
        return ((m_iHdrOff != 0) ?
            ++((LsMcHdr *)m_pHash->offset2ptr(m_iHdrOff))->x_data->cas : 0);
    }

    void     saveCas(LsMcDataItem *pItem)
    {
        m_rescas = (m_mcparms.m_usecas ? pItem->x_data->withcas.cas : 0);
    }

    LsMcCmdFunc *getCmdFunction(const char *pCmd, int len);
    void         dataItemUpdate(uint8_t *pBuf);

    static int   doCmdTest1(LsMemcache *pThis, int arg);
    static int   doCmdTest2(LsMemcache *pThis, int arg);
    static int   doCmdPrintTids(LsMemcache *pThis, int arg);
    static int   doCmdGet(LsMemcache *pThis, int arg);
    static int   doCmdUpdate(LsMemcache *pThis, int arg);
    static int   doCmdArithmetic(LsMemcache *pThis, int arg);
    static int   doCmdDelete(LsMemcache *pThis, int arg);
    static int   doCmdTouch(LsMemcache *pThis, int arg);
    static int   doCmdStats(LsMemcache *pThis, int arg);
    static int   doCmdFlush(LsMemcache *pThis, int arg);
    static int   doCmdVersion(LsMemcache *pThis, int arg);
    static int   doCmdQuit(LsMemcache *pThis, int arg);
    static int   doCmdVerbosity(LsMemcache *pThis, int arg);
    static int   doCmdClear(LsMemcache *pThis, int arg);
    static int   notImplemented(LsMemcache *pThis, int arg);

    uint8_t  setupNoreplyCmd(uint8_t cmd);
    uint8_t *setupBinCmd(McBinCmdHdr *pHdr, uint8_t cmd, LsMcUpdOpt *pOpt);
    bool     isRemoteEligible(uint8_t cmd);

    void     doBinGet(McBinCmdHdr *pHdr, uint8_t cmd, bool doTouch);
    int      doBinDataUpdate(uint8_t *pBuf, McBinCmdHdr *pHdr);
    void     doBinDelete(McBinCmdHdr *pHdr);
    void     doBinArithmetic(McBinCmdHdr *pHdr, uint8_t cmd, LsMcUpdOpt *pOpt);
    void     doBinFlush(McBinCmdHdr *pHdr);
    void     doBinVersion(McBinCmdHdr *pHdr);
    void     doBinStats(McBinCmdHdr *pHdr);
    void     doBinStat1str(McBinCmdHdr *pResHdr,
                const char *pkey, char *pval);
    void     doBinStat1p64(McBinCmdHdr *pResHdr,
                const char *pkey, uint64_t *pval);
    void     doBinStat1dot6(McBinCmdHdr *pResHdr,
                const char *pkey, long val1, long val2);
    void     doBinStat1Send(McBinCmdHdr *pResHdr, const char *pkey, int bodylen);
    void     doBinSaslList(McBinCmdHdr *pHdr);
    void     doBinSaslAuth(McBinCmdHdr *pHdr);
    void     doBinSaslStep(McBinCmdHdr *pHdr);

    void     setupBinResHdr(McBinCmdHdr *pHdr,
                uint8_t extralen, uint16_t keylen, uint32_t totbody,
                uint16_t status, uint8_t *pBinBuf);
    void     binOkRespond(McBinCmdHdr *pHdr);
    void     binErrRespond(McBinCmdHdr *pHdr, McBinStat err);

    char    *advToken(char *pStr, char *pStrEnd, char **pTokPtr, size_t *pTokLen);
    bool     myStrtol(const char *pStr, int32_t *pVal);
    bool     myStrtoul(const char *pStr, uint32_t *pVal);
    bool     myStrtoll(const char *pStr, int64_t *pVal);
    static bool  myStrtoull(const char *pStr, uint64_t *pVal);
    void     clearKeyList();

    bool     chkEndToken(char *pStr, char *pStrEnd);

    static void  setItemExptime(LsMcDataItem *pItem, uint32_t exptime)
    {
        if ((exptime != 0) && (exptime <= LSMC_MAXDELTATIME))
            exptime += time((time_t *)NULL);
        pItem->x_exptime = (time_t)exptime;
    }

    int   parmAdjLen(int valLen);

    LsMcHashSlice *setSlice();

    LsMcHashSlice *canProcessNow(const void *pKey, int iLen);

    bool     isSliceRemote(LsMcHashSlice *pSlice);
    bool     fwdToRemote(LsMcHashSlice *pSlice, char *pNxt);
    bool     fwdBinToRemote(LsMcHashSlice *pSlice, McBinCmdHdr *pHdr);
    void     fwdCommand(LsMcHashSlice *pSlice, const char *buf, int len);

    void statSetCmd()
    {
        if (m_iHdrOff != 0)
            ++((LsMcHdr *)m_pHash->offset2ptr(m_iHdrOff))->x_stats.set_cmds;
    }

    void statFlushCmd()
    {
        if (m_iHdrOff != 0)
            ++((LsMcHdr *)m_pHash->offset2ptr(m_iHdrOff))->x_stats.flush_cmds;
    }

    void statGetHit()
    {
        if (m_iHdrOff != 0)
            ++((LsMcHdr *)m_pHash->offset2ptr(m_iHdrOff))->x_stats.get_hits;
    }

    void statGetMiss()
    {
        if (m_iHdrOff != 0)
            ++((LsMcHdr *)m_pHash->offset2ptr(m_iHdrOff))->x_stats.get_misses;
    }

    void statTouchHit()
    {
        if (m_iHdrOff != 0)
            ++((LsMcHdr *)m_pHash->offset2ptr(m_iHdrOff))->x_stats.touch_hits;
    }

    void statTouchMiss()
    {
        if (m_iHdrOff != 0)
            ++((LsMcHdr *)m_pHash->offset2ptr(m_iHdrOff))->x_stats.touch_misses;
    }

    void statDeleteHit()
    {
        if (m_iHdrOff != 0)
            ++((LsMcHdr *)m_pHash->offset2ptr(m_iHdrOff))->x_stats.delete_hits;
    }

    void statDeleteMiss()
    {
        if (m_iHdrOff != 0)
            ++((LsMcHdr *)m_pHash->offset2ptr(m_iHdrOff))->x_stats.delete_misses;
    }

    void statIncrHit()
    {
        if (m_iHdrOff != 0)
            ++((LsMcHdr *)m_pHash->offset2ptr(m_iHdrOff))->x_stats.incr_hits;
    }

    void statIncrMiss()
    {
        if (m_iHdrOff != 0)
            ++((LsMcHdr *)m_pHash->offset2ptr(m_iHdrOff))->x_stats.incr_misses;
    }

    void statDecrHit()
    {
        if (m_iHdrOff != 0)
            ++((LsMcHdr *)m_pHash->offset2ptr(m_iHdrOff))->x_stats.decr_hits;
    }

    void statDecrMiss()
    {
        if (m_iHdrOff != 0)
            ++((LsMcHdr *)m_pHash->offset2ptr(m_iHdrOff))->x_stats.decr_misses;
    }

    void statCasHit()
    {
        if (m_iHdrOff != 0)
            ++((LsMcHdr *)m_pHash->offset2ptr(m_iHdrOff))->x_stats.cas_hits;
    }

    void statCasMiss()
    {
        if (m_iHdrOff != 0)
            ++((LsMcHdr *)m_pHash->offset2ptr(m_iHdrOff))->x_stats.cas_misses;
    }

    void statCasBad()
    {
        if (m_iHdrOff != 0)
            ++((LsMcHdr *)m_pHash->offset2ptr(m_iHdrOff))->x_stats.cas_badval;
    }

    void statAuthCmd()
    {
        if (m_iHdrOff != 0)
            ++((LsMcHdr *)m_pHash->offset2ptr(m_iHdrOff))->x_stats.auth_cmds;
    }

    void statAuthErr()
    {
        if (m_iHdrOff != 0)
            ++((LsMcHdr *)m_pHash->offset2ptr(m_iHdrOff))->x_stats.auth_errors;
    }
    LsShmHash::iteroffset doHashUpdate(LsMcUpdOpt *updOpt);
    LsShmHash::iteroffset doHashInsert(LsMcUpdOpt *updOpt);

    static LsMcCmdFunc s_LsMcCmdFuncs[];

    ls_strpair_t *mkDiskStrPair(ls_strpair_t *pair)
    {
        pair->key = m_parmsKeyDisk;
        pair->val = m_parmsData;
        return pair;
    }
    
    LsShmHash              *m_pHash;
    LsMcHashSlice          *m_pCurSlice;
    LsMcHashMulti          *m_pHashMulti;
    MemcacheConn           *m_pConn;
    MemcacheConn           *m_pWaitQ;
    MemcacheConn           *m_pWaitTail;
    char                   *m_pStrt;    // start of ascii command buffer
    LsShmOffset_t           m_iHdrOff;
    ls_str_t                m_parmsKeyWire;
    ls_str_t                m_parmsKeyDisk;
    ls_str_t                m_parmsData;
    LsShmHKey               m_KeyDiskHash;     
    bool                    m_KeyDiskMalloc;
    LsMcDataItem            m_item;     // cmd in progress
    LsShmHash::iteroffset   m_iterOff;  // cmd in progress
    uint64_t                m_rescas;   // cmd in progress
    int                     m_needed;
    uint8_t                 m_retcode;
    bool                    m_noreply;
    LsMcParms               m_mcparms;
    LsMcStrPool             m_keyPool;
    LsMcStrList             m_keyList;
};

#endif // LSSHMMEMCACHE_H

