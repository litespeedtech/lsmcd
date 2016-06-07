/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef GZIPBUF_H
#define GZIPBUF_H


/**
  *@author George Wang
  */
#include <lsdef.h>
#include <inttypes.h>
#include <zlib.h>

#define DEFAULT_GZIP_HEADER_SIZE 10
#define DEFAULT_GZIP_TAILER_SIZE 8

class VMemBuf;

class GzipBuf
{
    z_stream        m_zstr;
    uint32_t        m_crc;
    uint32_t        m_lastFlush;
    uint32_t        m_flushWindowSize;
    short           m_type;
    char            m_streamStarted;
    char            m_needFullFlush;
    VMemBuf        *m_pCompressCache;

    int process(int flush_mode);
    int compress(const char *pBuf, int len);
    int decompress(const char *pBuf, int len);
public:
    enum
    {
        GZIP_UNKNOWN,
        GZIP_DEFLATE,
        GZIP_INFLATE,
        GZIP_DEFLATE_CHUNK,
        GZIP_DEFLATE_RAW
    };
    GzipBuf();
    ~GzipBuf();

    explicit GzipBuf(int type, int level);

    int getType() const {   return m_type;   }

    int init(int type, int level);
    int reinit();
    int beginStream();
    int write(const char *pBuf, int len)
    {   return (compress(pBuf, len) < 0) ? -1 : len;   }
    int shouldFlush()
    {   return m_zstr.total_in - m_lastFlush > m_flushWindowSize;       }
    int flush(int flush_mode = Z_SYNC_FLUSH)
    {
        if (m_lastFlush != m_zstr.total_in)
        {
            m_lastFlush = m_zstr.total_in;
            return process(flush_mode);
        }
        return 0;
    }
    int endStream();

    void setFlushWindowSize(unsigned long size)
    {   m_flushWindowSize = size;       }

    int release();

    void setCompressCache(VMemBuf *pCache)
    {   m_pCompressCache = pCache;  }
    VMemBuf *getCompressCache() const
    {   return m_pCompressCache;    }
    int resetCompressCache();
    const char *getLastError() const
    {   return m_zstr.msg;          }

    int processFile(int type, const char *pFileName,
                    const char *pCompressFileName);

    int compressFile(const char *pFileName, const char *pCompressFileName)
    {   return processFile(GZIP_DEFLATE, pFileName, pCompressFileName);       }
    int decompressFile(const char *pFileName, const char *pDecompressFileName)
    {   return processFile(GZIP_INFLATE, pFileName, pDecompressFileName);       }
    char isStreamStarted() const {   return m_streamStarted;     }
    void combineChecksum(uLong crc32, int len)
    {
        if (m_zstr.adler)
            m_zstr.adler = crc32_combine(m_zstr.adler, crc32, len);
        else
            m_zstr.adler = crc32;
    }
    uLong getChunkChecksum() const  {   return m_crc;           }
    uLong getTotalIn() const        {   return m_zstr.total_in; }
    void updateNextOut();
    
    int addGzipHeader();
    int addGzipFooter();
    
    int appendPreCompressed(const char *pCompressed, int comp_len, 
                            int org_len, uint32_t crc32);
    int fullFlush()
    {
        if (m_needFullFlush)
        {
            if (process(Z_FULL_FLUSH) == -1)
                return -1;
            m_needFullFlush = 0;
        }
        return 0;
    }
    
    void updateZstreamPreCompressed(int org_len, uint32_t crc32);
    
    void setLevel(int level)
    {
        deflateParams(&m_zstr, level, Z_DEFAULT_STRATEGY);
    }
    void resetChecksum()            {   m_crc = 0;   }
    
    char isNeedFullFlush() const    {   return m_needFullFlush;   }

};


#include <util/objarray.h>

#define DC_MODE_COPY        0 
#define DC_MODE_COMPRESS    1

struct DeflateChunkInfo
{
    int      mode;
    uint32_t offset;
    uint32_t gzip_len;
    uint32_t crc32;
    uint32_t org_len;
};


class DeflateChunks : public TObjArray<DeflateChunkInfo>
{
public:
    DeflateChunks()
    {}
    ~DeflateChunks() {}
    
    int save(VMemBuf *pBuf);
    int save(int fd, off_t offset);
    int load(VMemBuf *pBuf, off_t offset);
    int load(int fd, off_t offset);
    
    LS_NO_COPY_ASSIGN(DeflateChunks);
};


class DeflateContext
{
public:
    enum 
    {
    };

    DeflateContext()
        : m_curMode(DC_MODE_COPY)
        , m_pCurChunk(NULL)
    {}    
    
    ~DeflateContext()
    {}
    
    int init(VMemBuf *pBuf, int mode);
    int newChunk(int mode);
    int append(const char *pBuf, int len);
    int endChunk();
    DeflateChunkInfo *getCurChunk()     {   return m_pCurChunk;     }
    int getCurChunkIndex()     {   return m_pCurChunk - m_chunks.begin();     }
    int getChunkCount() const           {   return m_chunks.size(); }
    const DeflateChunkInfo *getChunk(int n) const;
    DeflateChunks *getChunks()    
    {   return &m_chunks;       }
    void swapChunks(DeflateChunks &rhs)
    {   m_chunks.swap(rhs);     }
    void done();
    
private:
    
    int getGzipLevel(int mode)  {   return (mode == DC_MODE_COPY) ? 0 : 9;     }
    
    GzipBuf             m_gzip;
    int                 m_curMode;
    DeflateChunkInfo *  m_pCurChunk;
    DeflateChunks       m_chunks;

    LS_NO_COPY_ASSIGN(DeflateContext);
};


#endif
