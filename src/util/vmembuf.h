/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef VMEMBUF_H
#define VMEMBUF_H



#include <util/autostr.h>
#include <util/gpointerlist.h>
#include <util/blockbuf.h>

#include <stddef.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/stat.h>

#define VMBUF_MALLOC    0
#define VMBUF_ANON_MAP  1
#define VMBUF_FILE_MAP  2

typedef TPointerList<BlockBuf> BufList;

class VMemBuf
{
protected:
    static int      s_iBlockSize;
    static int      s_iMinMmapSize;
    static int      s_iKeepOpened;
    static int      s_fdSpare;
    static char     s_sTmpFileTemplate[256];
    static long     s_iMaxAnonMapBlocks;
    static int      s_iCurAnonMapBlocks;

private:
    BufList     m_bufList;
    AutoStr2    m_sFileName;
    int         m_fd;
    off_t       m_curTotalSize;

    char        m_type;
    char        m_mapMode;
    char        m_autoGrow;
    char        m_noRecycle;
    off_t       m_curWBlkPos;
    BlockBuf **m_pCurWBlock;
    char       *m_pCurWPos;

    off_t       m_curRBlkPos;
    BlockBuf **m_pCurRBlock;
    char       *m_pCurRPos;


    int mapNextWBlock();
    int mapNextRBlock();
    int appendBlock(BlockBuf *pBlock);
    int grow();
    void releaseBlocks();
    void reset();
    BlockBuf *getAnonMapBlock(int size);
    void recycle(BlockBuf *pBuf);

    int  remapBlock(BlockBuf *pBlock, off_t pos);

public:
    void deallocate();

    static size_t getBlockSize()    {   return s_iBlockSize;    }
    static int lowOnAnonMem()
    {   return s_iMaxAnonMapBlocks - s_iCurAnonMapBlocks < s_iMaxAnonMapBlocks / 4; }
    static size_t getMinMmapSize()  {   return s_iMinMmapSize;  }
    static void setMaxAnonMapSize(long sz);
    static void setTempFileTemplate(const char *pTemp);
    static char *mapTmpBlock(int fd, BlockBuf &buf, off_t offset,
                             int write = 0);
    static void releaseBlock(BlockBuf *pBlock);

    explicit VMemBuf(long TargetSize = 0);
    ~VMemBuf();
    int set(int type, long size);
    int set(int type, BlockBuf *pBlock);
    int set(const char *pFileName, long size);
    int setfd(const char *pFileName, int fd);
    char *getReadBuffer(size_t &size);
    char *getWriteBuffer(size_t &size);
    void readUsed(off_t len)     {   m_pCurRPos += len;      }
    void writeUsed(off_t len)    {   m_pCurWPos += len;      }
    char *getCurRPos() const       {   return m_pCurRPos;      }
    off_t getCurROffset() const
    {   return (m_pCurRBlock) ? (m_curRBlkPos - ((*m_pCurRBlock)->getBufEnd() - m_pCurRPos)) : 0;   }
    char *getCurWPos() const       {   return m_pCurWPos;      }
    off_t getCurWOffset() const
    {   return m_pCurWBlock ? (m_curWBlkPos - ((*m_pCurWBlock)->getBufEnd() - m_pCurWPos)) : 0;   }
    int write(const char *pBuf, int size);
    int write(VMemBuf *pBuf, off_t offset, int size);
    bool isMmaped() const {   return m_type >= VMBUF_ANON_MAP;  }
    //int  seekRPos( size_t pos );
    //int  seekWPos( size_t pos );
    void rewindWriteBuf();
    void rewindReadBuf();
    int  seekWritePos(off_t offset);
    void rewindWOff(off_t rewind);
    int setROffset(off_t offset);
    int getfd() const               {   return m_fd;            }
    off_t getCurFileSize() const   {   return m_curTotalSize;  }
    off_t getCurRBlkPos() const    {   return m_curRBlkPos;    }
    off_t getCurWBlkPos() const    {   return m_curWBlkPos;    }
    bool empty() const;
    off_t writeBufSize() const;
    int  reinit(off_t TargetSize = -1);
    int  exactSize(off_t *pSize = NULL);
    int  shrinkBuf(off_t size);
    int  shrinkAnonBuf(off_t size)
    {
        if (m_type == VMBUF_ANON_MAP)
            return shrinkBuf(size);
        return 0;
    }
    int  close();
    int  copyToFile(off_t startOff, off_t len,
                    int fd, off_t destStartOff);

    int convertFileBackedToInMemory();
    static void initAnonPool();
    static void releaseAnonPool();
    int copyToBuf(char *buf, int offset, int len);

};

class MMapVMemBuf : public VMemBuf
{
public:
    explicit MMapVMemBuf(int TargetSize = 0);
};


#endif
