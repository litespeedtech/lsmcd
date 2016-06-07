/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */

#include "gzipstream.h"
#include "lsdef.h"
#include <assert.h>

#define DEFAULT_FLUSH_WINDOW    2048
#define Z_INIT_ERROR            (-10)

GzipStream::GzipStream (int mode, LoopBuf* pLBuf, int level)
    : m_errno(0)
    , m_zLevel(level)
    , m_zipMode(mode)
    , m_pLoopBuf(pLBuf)     
    
{
    m_zstrm.zalloc = Z_NULL;
    m_zstrm.zfree = Z_NULL;
    m_zstrm.opaque = Z_NULL;
    m_zstrm.state = NULL;
    if (m_zipMode == ZIP_DEF) 
        initDef();
    else 
        initInf();
}


GzipStream::~GzipStream()
{
    release();
}


int GzipStream::release()
{
    if (m_zipMode == ZIP_DEF)
        return deflateEnd(&m_zstrm);
    else
        return inflateEnd(&m_zstrm);
}


int GzipStream::initDef()
{
    int ret;
    
    if (m_zipMode != ZIP_DEF)
        return Z_INIT_ERROR;
    
    if (!m_zstrm.state)
        ret = deflateInit(&m_zstrm, m_zLevel);
    else
        ret = deflateReset(&m_zstrm);
    
    if (ret != Z_OK)
        return Z_INIT_ERROR;
    return LS_OK;   
}

int GzipStream::initInf()
{
    int ret;
    if (m_zipMode != ZIP_INF)
        return Z_INIT_ERROR;
    
    if (!m_zstrm.state)
        ret = inflateInit(&m_zstrm);
    else
        ret = inflateReset(&m_zstrm);    
    
    if (ret != Z_OK)
        return Z_INIT_ERROR;
    return LS_OK;    
}


int GzipStream::pushOutGoingData(const char* pData, int dataLen)
{
    if ( m_zipMode != ZIP_DEF)
        return LS_FAIL;

    int oLen = compress(pData,dataLen);
    return oLen;
}


int GzipStream::popIncomingData(const char* pData, int dataLen)
{
    if ( m_zipMode != ZIP_INF)
        return LS_FAIL;
    
    int res = uncompress(pData, dataLen);
    if (res == LS_OK)
    {
        return m_pLoopBuf->size();
    }
    else
        return res;
}


int GzipStream::compress (const char *pInBuf, int dataLen)
{
    int ret;
            
    m_zstrm.avail_in = dataLen;
    m_zstrm.next_in = (Bytef*)(pInBuf);
    
    int currSize = m_pLoopBuf->size();
    do 
    {
        m_pLoopBuf->guarantee(2048);
        
        m_zstrm.avail_out = m_pLoopBuf->contiguous();//m_pLoopBuf->available();
        m_zstrm.next_out = (Bytef*)(m_pLoopBuf->end());
        ret = deflate(&m_zstrm, Z_SYNC_FLUSH);    /* no bad return value */
        
        assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
        m_pLoopBuf->used (m_zstrm.next_out - (Bytef*)(m_pLoopBuf->end()));
    } 
    while (m_zstrm.avail_in > 0);
    
    return m_pLoopBuf->size() - currSize; 
}


int GzipStream::uncompress (const char *pInBuf, int dataLen)
{
    int ret;
    
    int currSize = m_pLoopBuf->size();      
    m_zstrm.avail_in = dataLen;
    m_zstrm.next_in = (Bytef*)(pInBuf);
    do 
    {
        m_pLoopBuf->guarantee(2048);
        
        m_zstrm.avail_out = m_pLoopBuf->contiguous();
        m_zstrm.next_out = (Bytef*)(m_pLoopBuf->end());
        ret = inflate (&m_zstrm, Z_SYNC_FLUSH);
        
        assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
        switch (ret) {
        case Z_NEED_DICT:
            ret = Z_DATA_ERROR;
            return ret;
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
            return ret;
        }
        m_pLoopBuf->used(m_zstrm.next_out - (Bytef*)(m_pLoopBuf->end()));
        
    } 
    while (m_zstrm.avail_in > 0);
    return m_pLoopBuf->size() - currSize;
}


const char* GzipStream::getZipErr(int errno) const
{   
    switch (errno) {
    case Z_ERRNO:
        return "error reading or writing";
        break;
    case Z_STREAM_ERROR:
        return "invalid compression level";
        break;
    case Z_DATA_ERROR:
        return "invalid or incomplete deflate data";
        break;
    case Z_MEM_ERROR:
        return "out of memory";
        break;
    case Z_VERSION_ERROR:
        return "zlib version mismatch";
    case Z_INIT_ERROR:
        return "init failed";
    }
    return "unknown error";
}   

    
int GzipStream::getZipOutput(char **ptr)
{
    *ptr = m_pLoopBuf->begin();
    return m_pLoopBuf->size();
}

