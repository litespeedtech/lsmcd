/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include <util/gzipbuf.h>
#include <util/vmembuf.h>
#include <util/ni_fio.h>

#include <assert.h>
//#include <netinet/in.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define DEFAULT_FLUSH_WINDOW    2048

static unsigned char s_gzipHeader[10] = 
{   0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03   };

GzipBuf::GzipBuf()
{
    memset(this, 0, sizeof(GzipBuf));
    m_flushWindowSize = DEFAULT_FLUSH_WINDOW;
}

GzipBuf::GzipBuf(int type, int level)
{
    memset(this, 0, sizeof(GzipBuf));
    m_flushWindowSize = DEFAULT_FLUSH_WINDOW;
    init(type, level);
}

GzipBuf::~GzipBuf()
{
    release();
}

int GzipBuf::release()
{
    if (m_type == GZIP_INFLATE)
        return inflateEnd(&m_zstr);
    else
        return deflateEnd(&m_zstr);
}


int GzipBuf::init(int type, int level)
{
    int ret;
    if (type == GZIP_INFLATE)
        m_type = GZIP_INFLATE;
    else
        m_type = type;
    if (m_type != GZIP_INFLATE)
    {
        if (!m_zstr.state)
            ret = deflateInit2(&m_zstr, level, Z_DEFLATED, -15, 8,
                               Z_DEFAULT_STRATEGY);
        else
            ret = deflateReset(&m_zstr);
        m_zstr.adler = 0;
        m_lastFlush = 0;
        m_needFullFlush = 0;
    }
    else
    {
        if (!m_zstr.state)
            ret = inflateInit2(&m_zstr, 15 + 16);
        else
            ret = inflateReset(&m_zstr);
    }
    return ret;
}

int GzipBuf::reinit()
{
    int ret;
    if (m_type != GZIP_INFLATE)
    {
        ret = deflateReset(&m_zstr);
        m_zstr.adler = 0;
        m_crc = 0;
    }
    else
        ret = inflateReset(&m_zstr);
    m_streamStarted = 1;
    m_needFullFlush = 0;
    m_lastFlush = 0;
    return ret;
}


int GzipBuf::beginStream()
{
    if (!m_pCompressCache)
        return -1;
    size_t size;

    m_zstr.next_in = NULL;
    m_zstr.avail_in = 0;

    if (m_type == GZIP_DEFLATE || m_type == GZIP_DEFLATE_CHUNK)
        addGzipHeader();

    m_zstr.next_out = (unsigned char *)
                      m_pCompressCache->getWriteBuffer(size);
    m_zstr.avail_out = size;

    if (!m_zstr.next_out)
        return -1;
    m_streamStarted = 1;
    return 0;
}


int GzipBuf::addGzipHeader()
{
    return (m_pCompressCache->write((char *)s_gzipHeader, 10) == 10);
}


int GzipBuf::addGzipFooter()
{
    unsigned char footer[8];
    unsigned char *p = footer;
    uint32_t val = m_zstr.adler;
    *p++ = val & 0xff;
    val >>= 8;
    *p++ = val & 0xff;
    val >>= 8;
    *p++ = val & 0xff;
    val >>= 8;
    *p++ = val & 0xff;
    val = m_zstr.total_in;
    *p++ = val & 0xff;
    val >>= 8;
    *p++ = val & 0xff;
    val >>= 8;
    *p++ = val & 0xff;
    val >>= 8;
    *p++ = val & 0xff;
    return (m_pCompressCache->write((char *)footer, 8) == 8);
}


int GzipBuf::compress(const char *pBuf, int len)
{
    if (!m_streamStarted)
        return -1;
    m_zstr.next_in = (unsigned char *)pBuf;
    m_zstr.avail_in = len;
    if (m_type == GZIP_DEFLATE)
        m_zstr.adler = crc32(m_zstr.adler, m_zstr.next_in, len);
    else
        m_crc = crc32(m_crc, m_zstr.next_in, len);
    m_needFullFlush = 1;
    return process(0);
}

int GzipBuf::process(int flush_mode)
{
    do
    {
        int ret;
        size_t size;
        if (!m_zstr.avail_out)
        {
            m_zstr.next_out = (unsigned char *)m_pCompressCache->getWriteBuffer(size);
            m_zstr.avail_out = size;
            assert(m_zstr.avail_out);
        }
        if (m_type != GZIP_INFLATE)
            ret = ::deflate(&m_zstr, flush_mode);
        else
            ret = ::inflate(&m_zstr, flush_mode);
        if (ret == Z_STREAM_ERROR)
            return -1;
        if (ret == Z_BUF_ERROR)
            ret = 0;
        m_pCompressCache->writeUsed(m_zstr.next_out -
                                    (unsigned char *)m_pCompressCache->getCurWPos());
        if ((m_zstr.avail_out) || (ret == Z_STREAM_END))
            return ret;
        m_zstr.next_out = (unsigned char *)m_pCompressCache->getWriteBuffer(size);
        m_zstr.avail_out = size;
        if (!m_zstr.next_out)
            return -1;
    }
    while (true);
}


int GzipBuf::endStream()
{
    int ret = process(Z_FINISH);
    m_streamStarted = 0;
    m_needFullFlush = 0;
    if (ret != Z_STREAM_END)
        return -1;
    if (m_type == GZIP_DEFLATE || m_type == GZIP_DEFLATE_CHUNK)
    {
        addGzipFooter();
    }
    return 0;
}

int GzipBuf::resetCompressCache()
{
    m_pCompressCache->rewindReadBuf();
    m_pCompressCache->rewindWriteBuf();
    size_t size;
    m_zstr.next_out = (unsigned char *)
                      m_pCompressCache->getWriteBuffer(size);
    m_zstr.avail_out = size;
    return 0;
}

void GzipBuf::updateNextOut()
{
    size_t size;
    m_zstr.next_out = (unsigned char *)
                      m_pCompressCache->getWriteBuffer(size);
    m_zstr.avail_out = size;
}


int GzipBuf::appendPreCompressed(const char *pCompressed, int comp_len, 
                            int org_len, uint32_t crc32)
{
    if (fullFlush() == -1)
        return -1;
    if (m_pCompressCache->write(pCompressed, comp_len) < comp_len)
        return -1;
    updateZstreamPreCompressed(org_len, crc32);
    return 0;
}


void GzipBuf::updateZstreamPreCompressed(int org_len, uint32_t crc32)
{
    updateNextOut();
    combineChecksum(crc32, org_len);
    m_zstr.total_in += org_len;
    m_lastFlush = m_zstr.total_in;
}


int GzipBuf::processFile(int type, const char *pFileName,
                         const char *pCompressFileName)
{
    int fd;
    int ret = 0;
    fd = open(pFileName, O_RDONLY);
    if (fd == -1)
        return -1;
    VMemBuf gzFile;
    ret = gzFile.set(pCompressFileName, -1);
    if (!ret)
    {
        setCompressCache(&gzFile);
        if (((ret = init(type, 6)) == 0) && ((ret = beginStream()) == 0))
        {
            int len;
            char achBuf[16384];
            while (true)
            {
                len = nio_read(fd, achBuf, sizeof(achBuf));
                if (len <= 0)
                    break;
                if (this->write(achBuf, len) != len)
                {
                    ret = -1;
                    break;
                }
            }
            if (!ret)
            {
                ret = endStream();
                off_t size;
                if (!ret)
                    ret = gzFile.exactSize(&size);
            }
            gzFile.close();
        }
    }
    ::close(fd);
    return ret;
}


int DeflateContext::init(VMemBuf *pBuf, int mode)
{
    m_gzip.init(GzipBuf::GZIP_DEFLATE_CHUNK, getGzipLevel(mode));
    m_gzip.setCompressCache(pBuf);
    m_gzip.beginStream();
    newChunk(mode);
    return 0;
}


int DeflateContext::newChunk(int mode)
{
    if (m_pCurChunk != NULL)
        return -1;
    if (m_curMode != mode)
    {
        m_curMode = mode;
        m_gzip.setLevel(getGzipLevel(mode));
    }
    m_pCurChunk = m_chunks.newObj();
    m_pCurChunk->mode = mode;
    m_pCurChunk->offset = m_gzip.getCompressCache()->getCurWOffset();
    m_pCurChunk->org_len = m_gzip.getTotalIn();
    m_pCurChunk->crc32 = 0;
    m_pCurChunk->gzip_len = 0;
    return 0;
}


int DeflateContext::append(const char *pBuf, int len)
{
    return m_gzip.write(pBuf, len);
}


int DeflateContext::endChunk()
{
    if (m_pCurChunk)
    {
        m_pCurChunk->org_len = m_gzip.getTotalIn() - m_pCurChunk->org_len;
        if (m_pCurChunk->org_len > 0)
        {
            m_gzip.flush(Z_FULL_FLUSH);
            m_pCurChunk->gzip_len = m_gzip.getCompressCache()->getCurWOffset() - m_pCurChunk->offset;
            m_pCurChunk->crc32 = m_gzip.getChunkChecksum();
            m_gzip.combineChecksum(m_pCurChunk->crc32, m_pCurChunk->org_len);
            m_gzip.resetChecksum();
        }
        else
        {
            m_chunks.pop();
        }
        m_pCurChunk = NULL;
    }
    return 0;
}


const DeflateChunkInfo *DeflateContext::getChunk(int n) const
{
    if (n >= m_chunks.size())
        return NULL;
    return m_chunks.get(n);
}


void DeflateContext::done()
{
    endChunk();
    m_gzip.endStream();
        
}


int DeflateChunks::save(VMemBuf *pBuf)
{
    int32_t s[2];
    s[1] = size();
    s[0] = sizeof(DeflateChunkInfo) * s[1] + 4;
    pBuf->write((char *)s, sizeof(s));
    pBuf->write((char *)begin(), sizeof(DeflateChunkInfo)*s[1]);
    return 0;
}


int DeflateChunks::save(int fd, off_t offset)
{
    int32_t s[2];
    s[1] = size();
    s[0] = sizeof(DeflateChunkInfo) * s[1] + 4;
    if (pwrite(fd, (char *)s, sizeof(s), offset) == (ssize_t)sizeof(s))
    {
        offset += sizeof(s);
        if (pwrite(fd, (char *)begin(), sizeof(DeflateChunkInfo) * s[1], offset)
            == (ssize_t)sizeof(DeflateChunkInfo) * s[1])
            return 0;
    }
    return -1;
}


int DeflateChunks::load(int fd, off_t offset)
{
    int32_t s[2];
    if (pread(fd, (char *)s, 8, offset) == 8)
    {
        offset += 8;
        if (s[0] != (ssize_t)sizeof(DeflateChunkInfo) * s[1] + 4)
            return -1;
        alloc(s[1]);
        if (pread(fd, (char *)begin(), sizeof(DeflateChunkInfo) * s[1], offset)
            == (ssize_t)sizeof(DeflateChunkInfo) * s[1])
        {
            setSize(s[1]);
            return 0;
        }
    }
    return -1;
}


int DeflateChunks::load(VMemBuf *pBuf, off_t offset)
{
    int32_t s[2];
    if (pBuf->copyToBuf((char *)s, offset, 8) == 8)
    {
        offset += 8;
        if (s[0] != (ssize_t)sizeof(DeflateChunkInfo) * s[1] + 4)
            return -1;
        alloc(s[1]);
        if (pBuf->copyToBuf((char *)begin(), offset, sizeof(DeflateChunkInfo) * s[1])
            == (ssize_t)sizeof(DeflateChunkInfo) * s[1])
        {
            setSize(s[1]);
            return 0;
        }
    }
    return -1;
}


