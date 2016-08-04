/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include <util/vmembuf.h>
#include <util/blockbuf.h>
#include <util/gpath.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "vmembuf.h"

#define _RELEASE_MMAP
//#define _NO_ANON_POOL

int VMemBuf::s_iBlockSize = 8192;
int VMemBuf::s_iMinMmapSize = 8192;

long VMemBuf::s_iMaxAnonMapBlocks = 1024 * 1024 * 10 / 8192;
int  VMemBuf::s_iCurAnonMapBlocks = 0;
int  VMemBuf::s_iKeepOpened = 0;
int  VMemBuf::s_fdSpare = -1; //open( "/dev/null", O_RDWR );
char VMemBuf::s_sTmpFileTemplate[256] = "/tmp/tmp-XXXXXX";
#ifndef _NO_ANON_POOL
BufList *s_pAnonPool = NULL;
#endif

void VMemBuf::initAnonPool()
{
#ifndef _NO_ANON_POOL
    s_pAnonPool = new BufList();
#endif

}

void VMemBuf::releaseAnonPool()
{
#ifndef _NO_ANON_POOL
    if (!s_pAnonPool)
        return;
    while (s_pAnonPool->size() > 20)
    {
        BlockBuf *pBlock;
        pBlock = s_pAnonPool->pop_back();
        delete pBlock;
    }
#endif
}


void VMemBuf::setMaxAnonMapSize(long sz)
{
    if (sz >= 0)
        s_iMaxAnonMapBlocks = sz / s_iBlockSize;
}

void VMemBuf::setTempFileTemplate(const char *pTemp)
{
    if (pTemp != NULL)
    {
        strcpy(s_sTmpFileTemplate, pTemp);
        int len = strlen(pTemp);
        if ((len < 6) ||
            (strcmp(pTemp + len - 6, "XXXXXX") != 0))
            strcat(s_sTmpFileTemplate, "XXXXXX");
    }
}

VMemBuf::VMemBuf(long target_size)
    : m_bufList(4)
{
    reset();
}

VMemBuf::~VMemBuf()
{
    deallocate();
}

void VMemBuf::releaseBlocks()
{
    if (m_type == VMBUF_ANON_MAP)
    {
        s_iCurAnonMapBlocks -= m_curTotalSize / s_iBlockSize;
        if (m_noRecycle)
            m_bufList.release_objects();
        else
        {
#ifndef _NO_ANON_POOL
            s_pAnonPool->push_back(m_bufList);
            m_bufList.clear();
#else
            m_bufList.release_objects();
#endif
        }
    }
    else
        m_bufList.release_objects();
    memset(&m_curWBlkPos, 0,
           (char *)(&m_pCurRPos + 1) - (char *)&m_curWBlkPos);
    m_curTotalSize = 0;
}

void VMemBuf::deallocate()
{
    if (VMBUF_FILE_MAP == m_type)
    {
        if (m_fd != -1)
        {
            ::close(m_fd);
            m_fd = -1;
        }
        if (m_sFileName.c_str() && *m_sFileName.c_str())
            unlink(m_sFileName.c_str());
    }
    releaseBlocks();
}

void VMemBuf::recycle(BlockBuf *pBuf)
{
    if (m_type == VMBUF_ANON_MAP)
    {
        if ((int)pBuf->getBlockSize() == s_iBlockSize)
        {
            if (m_noRecycle)
                delete pBuf;
            else
            {
#ifndef _NO_ANON_POOL
                s_pAnonPool->push_back(pBuf);
#else
                delete pBuf;
#endif

            }
            --s_iCurAnonMapBlocks;
            return;
        }
        s_iCurAnonMapBlocks -= pBuf->getBlockSize() / s_iBlockSize;
    }
    delete pBuf;
}


void VMemBuf::rewindWOff(off_t rewind)
{
    while(rewind > 0)
    {
        if (m_pCurWPos - (*m_pCurWBlock)->getBuf() < (int)rewind)
        {
            rewind -= m_pCurWPos - (*m_pCurWBlock)->getBuf();
            m_pCurWPos = (*m_pCurWBlock)->getBuf();
            
            if ( m_pCurWBlock > m_bufList.begin())
            {
                m_curWBlkPos -= (*m_pCurWBlock)->getBlockSize();
                m_pCurWBlock--;
                if (!(*m_pCurWBlock)->getBuf())
                {
                    if ( remapBlock(*m_pCurWBlock, m_curWBlkPos - s_iBlockSize) == 0)
                        m_pCurWPos = (*m_pCurWBlock)->getBufEnd();
                    else
                    {
                        //cannot map previous block, out of memory?
                        assert("failed to map previous block" == NULL);
                    }
                }
                else
                    m_pCurWPos = (*m_pCurWBlock)->getBufEnd();
            }
            else
                return;
            
        }
        else
        {
            m_pCurWPos -= rewind;
            return;
        }
    }
}


void VMemBuf::seekRwOff(off_t rewind)
{
    seekWritePos(rewind);
    m_pCurRBlock = m_pCurWBlock;
    m_pCurRPos = m_pCurWPos;
    m_pCurRBlock = m_pCurWBlock;
    m_curRBlkPos = m_curWBlkPos;
}



int VMemBuf::shrinkBuf(off_t size)
{
    /*
        if ( m_type == VMBUF_FILE_MAP )
        {
            if ( s_iMaxAnonMapBlocks - s_iCurAnonMapBlocks > s_iMaxAnonMapBlocks / 10 )
            {
                deallocate();
                if ( set( VMBUF_ANON_MAP , getBlockSize() ) == -1 )
                    return -1;
                return 0;
            }

        }
    */
    if (size < 0)
        size = 0;
    if ((m_type == VMBUF_FILE_MAP) && (m_fd != -1))
    {
        if (m_curTotalSize > size)
            ftruncate(m_fd, size);
    }
    BlockBuf *pBuf;
    while (m_curTotalSize > size)
    {
        pBuf = m_bufList.pop_back();
        m_curTotalSize -= pBuf->getBlockSize();
        recycle(pBuf);
    }
    if (!m_bufList.empty())
        m_pCurWBlock = m_pCurRBlock = m_bufList.begin();
    return 0;
}

void VMemBuf::releaseBlock(BlockBuf *pBlock)
{
    if (pBlock->getBuf())
    {
        munmap(pBlock->getBuf(), pBlock->getBlockSize());
        pBlock->setBlockBuf(NULL, pBlock->getBlockSize());
    }

}

int VMemBuf::remapBlock(BlockBuf *pBlock, off_t pos)
{
    char *pBuf = (char *) mmap(NULL, s_iBlockSize, PROT_READ | PROT_WRITE,
                               MAP_SHARED | MAP_FILE, m_fd, pos);
    if (pBuf == MAP_FAILED)
    {
        perror("mmap() failed in remapBlock");
        return -1;
    }
    pBlock->setBlockBuf(pBuf, s_iBlockSize);
    return 0;
}


int VMemBuf::reinit(off_t TargetSize)
{
    if (m_type == VMBUF_ANON_MAP)
    {
        if ((TargetSize >=
             (long)((s_iMaxAnonMapBlocks - s_iCurAnonMapBlocks) * s_iBlockSize)) ||
            (TargetSize > 1024 * 1024))
        {
            releaseBlocks();
            if (set(VMBUF_FILE_MAP , getBlockSize()) == -1)
                return -1;
        }
    }
    else if (m_type == VMBUF_FILE_MAP)
    {
        if ((TargetSize < 1024 * 1024) &&
            (s_iMaxAnonMapBlocks - s_iCurAnonMapBlocks > s_iMaxAnonMapBlocks / 5) &&
            (TargetSize < (s_iMaxAnonMapBlocks - s_iCurAnonMapBlocks) *
             (int)s_iBlockSize))
        {
            deallocate();
            if (set(VMBUF_ANON_MAP , getBlockSize()) == -1)
                return -1;
        }

#ifdef _RELEASE_MMAP
        if (!m_bufList.empty())
        {
            if (!(*m_bufList.begin())->getBuf())
            {
                if (remapBlock(*m_bufList.begin(), 0) == -1)
                    return -1;
            }
            if ((m_pCurWBlock) &&
                (m_pCurWBlock != m_bufList.begin()) &&
                (m_pCurWBlock != m_pCurRBlock))
                releaseBlock(*m_pCurWBlock);
            if ((m_pCurRBlock) && (m_pCurRBlock != m_bufList.begin()))
                releaseBlock(*m_pCurRBlock);
        }
#endif

    }

    if (!m_bufList.empty())
    {

        m_pCurWBlock = m_pCurRBlock = m_bufList.begin();
        if (*m_pCurWBlock)
        {
            m_curWBlkPos = m_curRBlkPos = (*m_pCurWBlock)->getBlockSize();
            m_pCurRPos = m_pCurWPos = (*m_pCurWBlock)->getBuf();
        }
        else
        {
            m_curWBlkPos = m_curRBlkPos = 0;
            m_pCurRPos = m_pCurWPos = NULL;
        }
    }
    return 0;
}

void VMemBuf::reset()
{
    m_fd = -1;
    memset(&m_curTotalSize, 0,
           (char *)(&m_pCurRPos + 1) - (char *)&m_curTotalSize);
    m_mapMode = PROT_READ | PROT_WRITE;
    if (m_sFileName.buf())
    {
        *m_sFileName.buf() = 0;
        m_sFileName.setLen(0);
    }
}

BlockBuf *VMemBuf::getAnonMapBlock(int size)
{
    BlockBuf *pBlock;
#ifndef _NO_ANON_POOL
    if (size == (int)s_iBlockSize)
    {
        if (!s_pAnonPool->empty())
        {
            pBlock = s_pAnonPool->pop_back();
            ++s_iCurAnonMapBlocks;
            return pBlock;
        }
    }
#endif
    int blocks = (size + s_iBlockSize - 1) / s_iBlockSize;
    size = blocks * s_iBlockSize;
    char *pBuf = (char *) mmap(NULL, size, m_mapMode,
                               MAP_ANON | MAP_SHARED, -1, 0);
    if (pBuf == MAP_FAILED)
    {
        perror("Anonymous mmap() failed");
        return NULL;
    }
    pBlock = new MmapBlockBuf(pBuf, size);
    if (!pBlock)
    {
        perror("new MmapBlockBuf failed in getAnonMapBlock()");
        munmap(pBuf, size);
        return NULL;
    }
    s_iCurAnonMapBlocks += blocks;
    return pBlock;
}


int VMemBuf::set(int type, long size)
{
    if ((type > VMBUF_FILE_MAP) || (type < VMBUF_MALLOC))
        return -1;

    char *pBuf;
    BlockBuf *pBlock = NULL;
    m_type = type;
    switch (type)
    {
    case VMBUF_MALLOC:
        if (size > 0)
        {
            size = ((size + 511) / 512) * 512;
            pBuf = (char *)malloc(size);
            if (!pBuf)
                return -1;
            pBlock = new MallocBlockBuf(pBuf, size);
            if (!pBlock)
            {
                free(pBuf);
                return -1;
            }
        }
        break;
    case VMBUF_ANON_MAP:
        m_autoGrow = 1;
        if (size > 0)
        {
            if ((pBlock = getAnonMapBlock(size)) == NULL)
                return -1;
        }
        break;
    case VMBUF_FILE_MAP:
        m_autoGrow = 1;
        m_sFileName.setStr(s_sTmpFileTemplate);
        m_fd = mkstemp(m_sFileName.buf());
        if (m_fd == -1)
        {
            if (errno == ENOENT)
            {
                GPath::createMissingPath(m_sFileName.buf(), 0700, -1, -1);
                m_sFileName.setStr(s_sTmpFileTemplate);
                m_fd = mkstemp(m_sFileName.buf());
            }
            if (m_fd == -1)
            {
                char achBuf[1024];
                snprintf(achBuf, 1024,
                         "Failed to create swap file with mkstemp( %s ), please check 'Swap Directory' and permission",
                         m_sFileName.c_str());
                perror(achBuf);
                *m_sFileName.buf() = 0;
                m_sFileName.setLen(0);
                return -1;
            }
        }
        unlink(m_sFileName.c_str());
        *m_sFileName.buf() = 0;
        m_sFileName.setLen(0);

        fcntl(m_fd, F_SETFD, FD_CLOEXEC);
        break;
    }
    if (pBlock)
    {
        appendBlock(pBlock);
        m_pCurRPos = m_pCurWPos = pBlock->getBuf();
        m_curTotalSize = m_curRBlkPos = m_curWBlkPos = pBlock->getBlockSize();
        m_pCurRBlock = m_pCurWBlock = m_bufList.begin();
    }
    return 0;
}

int VMemBuf::appendBlock(BlockBuf *pBlock)
{
    if (m_bufList.full())
    {
        BlockBuf **pOld = m_bufList.begin();
        m_bufList.push_back(pBlock);
        if (m_pCurWBlock)
            m_pCurWBlock = m_pCurWBlock - pOld + m_bufList.begin();
        if (m_pCurRBlock)
            m_pCurRBlock = m_pCurRBlock - pOld + m_bufList.begin();
        return 0;
    }
    else
    {
        m_bufList.safe_push_back(pBlock);
        return 0;
    }

}

int VMemBuf::convertFileBackedToInMemory()
{
    BlockBuf *pBlock;
    if (m_type != VMBUF_FILE_MAP)
        return -1;
    if (m_pCurWPos == (*m_pCurWBlock)->getBuf())
    {
        if (*m_pCurWBlock == m_bufList.back())
        {
            m_bufList.pop_back();
            recycle(*m_pCurWBlock);
            if (!m_bufList.empty())
            {
                m_noRecycle = 1;
                s_iCurAnonMapBlocks += m_bufList.size();
                int i = 0;
                while (i < m_bufList.size())
                {
                    pBlock = m_bufList[i];
                    if (!pBlock->getBuf())
                    {
                        if (remapBlock(pBlock, i * s_iBlockSize) == -1)
                            return -1;
                    }
                    ++i;
                }
            }
            if (m_sFileName.c_str() && *m_sFileName.c_str())
                unlink(m_sFileName.c_str());
            ::close(m_fd);
            m_fd = -1;
            m_type = VMBUF_ANON_MAP;
            if (!(pBlock = getAnonMapBlock(s_iBlockSize)))
                return -1;
            appendBlock(pBlock);
            if (m_pCurRPos == m_pCurWPos)
                m_pCurRPos = (*m_pCurWBlock)->getBuf();
            m_pCurWPos = (*m_pCurWBlock)->getBuf();
            return 0;
        }

    }
    return -1;
}

int VMemBuf::set(int type, BlockBuf *pBlock)
{
    assert(pBlock);
    m_type = type;
    appendBlock(pBlock);
    m_pCurRBlock = m_pCurWBlock = m_bufList.begin();
    m_pCurRPos = m_pCurWPos = pBlock->getBuf();
    m_curTotalSize = m_curRBlkPos = m_curWBlkPos = pBlock->getBlockSize();
    return 0;
}

int VMemBuf::set(const char *pFileName, long size)
{
    if (size < 0)
        size = s_iMinMmapSize;
    m_type = VMBUF_FILE_MAP;
    m_autoGrow = 1;
    if (pFileName)
    {
        m_sFileName = pFileName;
        m_fd = ::open(pFileName, O_RDWR | O_CREAT | O_TRUNC, 0600);
        if (m_fd < 0)
        {
            perror("Failed to open temp file for swapping");
            return -1;
        }
        fcntl(m_fd, F_SETFD, FD_CLOEXEC);

    }
    return 0;
}

int VMemBuf::setfd(const char *pFileName, int fd)
{
    if (pFileName)
        m_sFileName = pFileName;
    m_fd = fd;
    fcntl(m_fd, F_SETFD, FD_CLOEXEC);
    m_type = VMBUF_FILE_MAP;
    m_autoGrow = 1;
    return 0;
}


void VMemBuf::rewindWriteBuf()
{
    if (m_pCurRBlock)
    {
        if (m_pCurWBlock != m_pCurRBlock)
        {
            m_pCurWBlock = m_pCurRBlock;
            m_curWBlkPos = m_curRBlkPos;
        }
        m_pCurWPos = m_pCurRPos;
    }
    else
    {
        m_pCurRBlock = m_pCurWBlock;
        m_curRBlkPos = m_curWBlkPos;
        m_pCurRPos = m_pCurWPos;
    }

}

void VMemBuf::rewindReadBuf()
{
#ifdef _RELEASE_MMAP
    if (m_type == VMBUF_FILE_MAP)
    {
        if (!(*m_bufList.begin())->getBuf())
        {
            if (remapBlock(*m_bufList.begin(), 0) == -1)
                return;
        }
        if ((m_pCurRBlock) &&
            (m_pCurRBlock != m_bufList.begin()) &&
            (m_pCurRBlock != m_pCurWBlock))
            releaseBlock(*m_pCurRBlock);
    }
#endif
    m_pCurRBlock = m_bufList.begin();
    m_curRBlkPos = (*m_pCurRBlock)->getBlockSize();
    m_pCurRPos = (*m_pCurRBlock)->getBuf();
}

int VMemBuf::setROffset(off_t offset)
{
    if (offset > m_curTotalSize)
        return -1;
    off_t roffset = getCurROffset();
    if (offset < roffset)
        rewindReadBuf();
    while (offset >= m_curRBlkPos)
        mapNextRBlock();
    m_pCurRPos = (*m_pCurRBlock)->getBuf() + offset - (m_curRBlkPos
                 - (*m_pCurRBlock)->getBlockSize());
    return 0;
}


int VMemBuf::seekWritePos(off_t offset)
{
    struct stat st;
    fstat(getfd(), &st);
    m_curTotalSize = st.st_size;
    off_t target_size = (offset + s_iBlockSize - 1) & ~(s_iBlockSize - 1);
    if (target_size > st.st_size)
    {
        if (ftruncate(getfd(), target_size) == -1)
            return -1;
        m_curTotalSize = target_size;
    }
    off_t block_pos = offset & ~(s_iBlockSize - 1);
    while(m_bufList.size() < m_curTotalSize / s_iBlockSize)
    {
        m_bufList.push_back(new MmapBlockBuf(NULL, s_iBlockSize));
    }
    m_pCurWBlock = &m_bufList[block_pos / s_iBlockSize];
    if (remapBlock(*m_pCurWBlock, block_pos) == -1)
        return -1;
    
    m_curWBlkPos = block_pos + s_iBlockSize;
    m_pCurWPos = (*m_pCurWBlock)->getBuf() + (offset - block_pos);
    return 0;
}

int VMemBuf::mapNextWBlock()
{
    if (!m_pCurWBlock || m_pCurWBlock + 1 >= m_bufList.end())
    {
        if (!m_autoGrow || grow())
            return -1;
    }

    if (m_pCurWBlock)
    {
#ifdef _RELEASE_MMAP
        if (m_type == VMBUF_FILE_MAP)
        {
            if (!(*(m_pCurWBlock + 1))->getBuf())
            {
                if (remapBlock(*(m_pCurWBlock + 1), m_curWBlkPos) == -1)
                    return -1;
            }
            if (m_pCurWBlock != m_pCurRBlock)
                releaseBlock(*m_pCurWBlock);
        }
#endif
        ++m_pCurWBlock;
    }
    else
    {
#ifdef _RELEASE_MMAP
        if ((m_type == VMBUF_FILE_MAP) && (!(*m_bufList.begin())->getBuf()))
        {
            if (remapBlock(*m_bufList.begin(), 0) == -1)
                return -1;
        }
#endif
        m_pCurWBlock = m_pCurRBlock = m_bufList.begin();
        m_curWBlkPos = 0;
        m_curRBlkPos = (*m_pCurWBlock)->getBlockSize();
        m_pCurRPos = (*m_pCurWBlock)->getBuf();
    }
    m_curWBlkPos += (*m_pCurWBlock)->getBlockSize();
    m_pCurWPos = (*m_pCurWBlock)->getBuf();
    return 0;
}

int VMemBuf::grow()
{
    char *pBuf;
    BlockBuf *pBlock;
    off_t oldPos = m_curTotalSize;
    switch (m_type)
    {
    case VMBUF_MALLOC:
        pBuf = (char *)malloc(s_iBlockSize);
        if (!pBuf)
            return -1;
        pBlock = new MallocBlockBuf(pBuf, s_iBlockSize);
        if (!pBlock)
        {
            perror("new MallocBlockBuf failed in grow()");
            free(pBuf);
            return -1;
        }
        break;
    case VMBUF_FILE_MAP:
        if (ftruncate(m_fd, m_curTotalSize + s_iBlockSize) == -1)
        {
            perror("Failed to increase temp file size with ftrancate()");
            return -1;
        }
        pBuf = (char *) mmap(NULL, s_iBlockSize, m_mapMode,
                             MAP_SHARED | MAP_FILE, m_fd, oldPos);
        if (pBuf == MAP_FAILED)
        {
            perror("FS backed mmap() failed");
            return -1;
        }
        pBlock = new MmapBlockBuf(pBuf, s_iBlockSize);
        if (!pBlock)
        {
            perror("new MmapBlockBuf failed in grow()");
            munmap(pBuf, s_iBlockSize);
            return -1;
        }
        break;
    case VMBUF_ANON_MAP:
        if ((pBlock = getAnonMapBlock(s_iBlockSize)))
            break;
    default:
        return -1;
    }
    appendBlock(pBlock);
    m_curTotalSize += s_iBlockSize;
    return 0;

}


char *VMemBuf::getWriteBuffer(size_t &size)
{
    if ((!m_pCurWBlock) || (m_pCurWPos >= (*m_pCurWBlock)->getBufEnd()))
    {
        if (mapNextWBlock() != 0)
            return NULL;
    }
    size = (*m_pCurWBlock)->getBufEnd() - m_pCurWPos;
    return m_pCurWPos;
}

char *VMemBuf::getReadBuffer(size_t &size)
{
    if ((!m_pCurRBlock) || (m_pCurRPos >= (*m_pCurRBlock)->getBufEnd()))
    {
        size = 0;
        if (mapNextRBlock() != 0)
            return NULL;
    }
    if (m_curRBlkPos == m_curWBlkPos)
        size = m_pCurWPos - m_pCurRPos;
    else
        size = (*m_pCurRBlock)->getBufEnd() - m_pCurRPos;
    return m_pCurRPos;
}

int VMemBuf::mapNextRBlock()
{
    if (m_curRBlkPos >= m_curWBlkPos)
        return -1;
    if (m_pCurRBlock)
    {
#ifdef _RELEASE_MMAP
        if (m_type == VMBUF_FILE_MAP)
        {
            if (m_pCurWBlock != m_pCurRBlock)
                releaseBlock(*m_pCurRBlock);
        }
#endif
        ++m_pCurRBlock;
    }
    else
    {
        m_pCurRBlock = m_bufList.begin();
    }
    if (!(*m_pCurRBlock)->getBuf())
    {
        if (remapBlock(*m_pCurRBlock, m_curRBlkPos) == -1)
            return -1;
    }
    m_curRBlkPos += (*m_pCurRBlock)->getBlockSize();
    m_pCurRPos = (*m_pCurRBlock)->getBuf();
    return 0;

}


bool VMemBuf::empty() const
{
    if (m_curRBlkPos < m_curWBlkPos)
        return false;
    if (!m_pCurWBlock)
        return true;
    return (m_pCurRPos >= m_pCurWPos);
}

off_t VMemBuf::writeBufSize() const
{
    if (m_pCurWBlock == m_pCurRBlock)
        return m_pCurWPos - m_pCurRPos;
    off_t diff = m_curWBlkPos - m_curRBlkPos;
    if (m_pCurWBlock)
        diff += m_pCurWPos - (*m_pCurWBlock)->getBuf();
    if (m_pCurRBlock)
        diff -= m_pCurRPos - (*m_pCurRBlock)->getBuf();
    return diff;
}

int VMemBuf::write(const char *pBuf, int size)
{
    const char *pCur = pBuf;
    if (m_pCurWPos)
    {
        int len = (*m_pCurWBlock)->getBufEnd() - m_pCurWPos;
        if (size <= len)
        {
            memmove(m_pCurWPos, pBuf, size);
            m_pCurWPos += size;
            return size;
        }
        else
        {
            memmove(m_pCurWPos, pBuf, len);
            pCur += len;
            size -= len;
        }
    }
    do
    {
        if (mapNextWBlock() != 0)
            return pCur - pBuf;
        int len  = (*m_pCurWBlock)->getBufEnd() - m_pCurWPos;
        if (size <= len)
        {
            memmove(m_pCurWPos, pCur, size);
            m_pCurWPos += size;
            return pCur - pBuf + size;
        }
        else
        {
            memmove(m_pCurWPos, pCur, len);
            pCur += len;
            size -= len;
        }
    }
    while (true);
}


int VMemBuf::write(VMemBuf *pBuf, off_t offset, int size)
{
    if (pBuf->setROffset(offset) == -1)
        return -1;
    int total = 0;
    while(total < size)
    {
        size_t len;
        const char * p = pBuf->getReadBuffer(len);
        if (len > size - total)
            len = size - total;
        if (!p)
            break;
        int ret = write(p, len);
        if (ret > 0)
        {
            total += ret;
            pBuf->readUsed(ret);
        }
        else
            break;
    }
    return total;
}



int VMemBuf::exactSize(off_t *pSize)
{
    off_t size = m_curTotalSize;
    if (m_pCurWBlock)
        size -= (*m_pCurWBlock)->getBufEnd() - m_pCurWPos;
    if (pSize)
        *pSize = size;
    if (m_fd != -1)
        return  ftruncate(m_fd, size);
    return 0;
}

int VMemBuf::close()
{
    if (m_fd != -1)
        ::close(m_fd);
    releaseBlocks();
    reset();
    return 0;
}

MMapVMemBuf::MMapVMemBuf(int TargetSize)
{
    int type = VMBUF_ANON_MAP;

    if ((lowOnAnonMem()) || (TargetSize > 1024 * 1024) ||
        (TargetSize >=
         (long)((s_iMaxAnonMapBlocks - s_iCurAnonMapBlocks) * s_iBlockSize)))
        type = VMBUF_FILE_MAP;
    if (set(type , getBlockSize()) == -1)
        abort(); //throw -1;
}

// int VMemBuf::setFd( int fd )
// {
//     struct stat st;
//     if ( fstat( fd, &st ) == -1 )
//         return -1;
//     m_fd = fd;
//     m_type = VMBUF_FILE_MAP;
//     m_curTotalSize = st.st_size;
//
//     return 0;
// }

char *VMemBuf::mapTmpBlock(int fd, BlockBuf &buf, off_t offset, int write)
{
    off_t blkBegin = offset - offset % s_iBlockSize;
    char *pBuf = (char *) mmap(NULL, s_iBlockSize, PROT_READ | write,
                               MAP_SHARED | MAP_FILE, fd, blkBegin);
    if (pBuf == MAP_FAILED)
    {
        perror("FS backed mmap() failed");
        return NULL;
    }
    buf.setBlockBuf(pBuf, s_iBlockSize);
    return pBuf + offset % s_iBlockSize;
}

int VMemBuf::copyToFile(off_t startOff, off_t len,
                        int fd, off_t destStartOff)
{
    BlockBuf *pSrcBlock = NULL;
    char *pSrcPos;
    int mapped = 0;
    int blk = startOff / s_iBlockSize;
    if (blk >= m_bufList.size())
        return -1;
    struct stat st;
    if (fstat(fd, &st) == -1)
        return -1;
    int destSize = destStartOff + len;
    //destSize -= destSize % s_iBlockSize;
    if (st.st_size < destSize)
    {
        if (ftruncate(fd, destSize) == -1)
            return -1;
    }
    BlockBuf destBlock;
    char *pPos = mapTmpBlock(fd, destBlock, destStartOff, PROT_WRITE);
    int ret = 0;
    if (!pPos)
        return -1;
    while (!ret && (len > 0))
    {
        if (blk >= m_bufList.size())
            break;
        pSrcBlock = m_bufList[blk];
        if (!pSrcBlock)
        {
            ret = -1;
            break;
        }
        if (!pSrcBlock->getBuf())
        {
            if (remapBlock(pSrcBlock, blk * s_iBlockSize) == -1)
                return -1;
            mapped = 1;
        }
        pSrcPos = pSrcBlock->getBuf() + startOff % s_iBlockSize;

        while ((len > 0) && (pSrcPos < pSrcBlock->getBufEnd()))
        {
            if (pPos >= destBlock.getBufEnd())
            {
                //msync(destBlock.getBuf(), destBlock.getBlockSize(), MS_SYNC);
                releaseBlock(&destBlock);
                pPos = mapTmpBlock(fd, destBlock, destStartOff, PROT_WRITE);
                if (!pPos)
                {
                    ret = -1;
                    break;
                }
            }
            int sz = len;
            if (sz > pSrcBlock->getBufEnd() - pSrcPos)
                sz = pSrcBlock->getBufEnd() - pSrcPos;
            if (sz > destBlock.getBufEnd() - pPos)
                sz = destBlock.getBufEnd() - pPos;
            memmove(pPos, pSrcPos, sz);
            pSrcPos += sz;
            pPos += sz;
            destStartOff += sz;
            len -= sz;
        }
        startOff = 0;
        if (mapped)
        {
            releaseBlock(pSrcBlock);
            mapped = 0;
        }
        ++blk;
    }
    releaseBlock(&destBlock);
    return ret;

}


int VMemBuf::copyToBuf(char *pBuf, int offset, int len)
{
    BlockBuf *pSrcBlock = NULL;
    char *pSrcPos;
    int mapped = 0;
    char *pPos = pBuf;
    int ret = 0;
    int blk = offset / s_iBlockSize;
    if (blk >= m_bufList.size())
        return -1;
    if (!pPos)
        return -1;
    while (!ret && (len > 0))
    {
        if (blk >= m_bufList.size())
            break;
        pSrcBlock = m_bufList[blk];
        if (!pSrcBlock)
        {
            ret = -1;
            break;
        }
        if (!pSrcBlock->getBuf())
        {
            if (remapBlock(pSrcBlock, blk * s_iBlockSize) == -1)
                return -1;
            mapped = 1;
        }
        pSrcPos = pSrcBlock->getBuf() + offset % s_iBlockSize;

        int sz = len;
        if (sz > pSrcBlock->getBufEnd() - pSrcPos)
            sz = pSrcBlock->getBufEnd() - pSrcPos;
        memmove(pPos, pSrcPos, sz);
        pPos += sz;
        len -= sz;

        offset = 0;
        if (mapped)
        {
            releaseBlock(pSrcBlock);
            mapped = 0;
        }
        ++blk;
    }
    return pPos - pBuf;

}


