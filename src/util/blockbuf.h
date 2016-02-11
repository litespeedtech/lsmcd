/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef BLOCKBUF_H
#define BLOCKBUF_H


/**
  *@author George Wang
  */
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <util/gpointerlist.h>
//#include <util/linkedobj.h>

#ifndef MAP_FILE
#define MAP_FILE 0
#endif //MAP_FILE

class BlockBuf //: public LinkedObj
{
    char   *m_pBuf;
    char   *m_pBufEnd;
public:
    BlockBuf()
        : m_pBuf(NULL)
        , m_pBufEnd(NULL)
    {}

    BlockBuf(char *pBuf, size_t size)
        : m_pBuf(pBuf)
        , m_pBufEnd(pBuf + size)
    {}
    virtual ~BlockBuf() {}
    virtual void deallocate()
    {}
    void setBlockBuf(char *pBuf, size_t size)
    {   m_pBuf = pBuf; m_pBufEnd = pBuf + size;       }

    char *getBuf() const           {   return m_pBuf;              }
    char *getBufEnd() const        {   return m_pBufEnd;           }
    size_t  getBlockSize() const    {   return m_pBufEnd - m_pBuf;  }

};

class MallocBlockBuf : public BlockBuf
{
public:
    MallocBlockBuf() {}
    MallocBlockBuf(char *pBuf, size_t size)
        : BlockBuf(pBuf, size)
    {}
    ~MallocBlockBuf()   {   if (getBuf()) free(getBuf());    }
    void deallocate()
    {   free(getBuf());     }
};

class MmapBlockBuf : public BlockBuf
{
public:
    MmapBlockBuf() {}
    MmapBlockBuf(char *pBuf, size_t size)
        : BlockBuf(pBuf, size)
    {}
    ~MmapBlockBuf()     {   if (getBuf()) munmap(getBuf(), getBlockSize()); }
    void deallocate()
    {
        munmap(getBuf(), getBlockSize());
        setBlockBuf(NULL, getBlockSize());
    }
};


#endif
