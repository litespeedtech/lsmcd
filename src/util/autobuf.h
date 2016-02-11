/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef AUTOBUF_H
#define AUTOBUF_H


/**
  *@author George Wang
  */
#include <string.h>

class Buffer
{
public:
    Buffer()
        : m_pBuf(NULL)
        , m_pEnd(NULL)
    {}

    Buffer(char *pBegin, int len)
        : m_pBuf(pBegin)
        , m_pEnd(pBegin + len)
    {}
    Buffer(char *pBegin, char *pEnd)
        : m_pBuf(pBegin)
        , m_pEnd(pEnd)
    {}
    ~Buffer()
    {}

    void set(char *pBegin, int len)
    {   m_pBuf = pBegin; m_pEnd = pBegin + len; }

    void set(char *pBegin, char *pEnd)
    {   m_pBuf = pBegin; m_pEnd = pEnd;     }

    void set(const char *pBegin, int len)
    {   m_pBuf = (char *)pBegin; m_pEnd = m_pBuf + len;  }

    void set(const char *pBegin, const char *pEnd)
    {   m_pBuf = (char *)pBegin; m_pEnd = (char *)pEnd;   }


    void copy(const Buffer &rhs)
    {   m_pBuf = rhs.m_pBuf;    m_pEnd = rhs.m_pEnd;    }

    char *begin() const {   return m_pBuf;              }
    char *end() const   {   return m_pEnd;              }
    size_t len() const  {   return m_pEnd - m_pBuf;     }
    size_t size() const {   return m_pEnd - m_pBuf;     }

private:
    char *m_pBuf;
    char *m_pEnd;

    Buffer(const Buffer &rhs);
    void operator=(const Buffer &rhs);
};


class AutoBuf
{
    char   *m_pBuf;
    char   *m_pEnd;
    char   *m_pBufEnd;

    AutoBuf(const AutoBuf &rhs);
    void operator=(const AutoBuf &rhs);

    int     allocate(int size);
    void    deallocate();
public:
    explicit AutoBuf(int size = 1024);
    ~AutoBuf();
    int available() const { return m_pBufEnd - m_pEnd; }

    char *begin()      {   return m_pBuf;      }
    char *end()        {   return m_pEnd;      }
    char *getBufEnd()  {   return m_pBufEnd;   }

    const char *begin() const      {   return m_pBuf;      }
    const char *end() const        {   return m_pEnd;      }
    const char *getBufEnd() const  {   return m_pBufEnd;   }


    char *getPointer(int offset)             { return m_pBuf + offset; }
    const char *getPointer(int offset) const { return m_pBuf + offset; }

    void used(int size)   {   m_pEnd += size;             }
    void clear()            {   m_pEnd = m_pBuf;            }

    int capacity() const    {   return m_pBufEnd - m_pBuf;  }
    int size()   const      {   return m_pEnd - m_pBuf;     }

    char *inc(char *&pCh) const
    {   return ++pCh;       }
    const char *inc(const char *&pCh) const
    {   return ++pCh;       }

    int reserve(int size) {   return allocate(size);   }
    void resize(int size) {   m_pEnd = m_pBuf + size;    }

    int grow(int size);
    int append_unsafe(const char *pBuf, int size)
    {
        memmove(end(), pBuf, size);
        used(size);
        return size;
    }

    int appendAllocOnly(int size)
    {
        if (size == 0)
            return 0;
        if (size > available())
        {
            if (grow(size - available()) == -1)
                return -1;
        }
        used(size);
        return size;
    }

    void append(char ch)
    {   *m_pEnd++ = ch; }
    int append(const char *pBuf, int size);
    int append(const char *pBuf)
    {   return append(pBuf, strlen(pBuf)); }
    bool empty()  const     {   return (m_pBuf == m_pEnd);  }
    bool full() const       {   return (m_pEnd == m_pBufEnd); }
    int getOffset(const char *p) const
    {   return p - m_pBuf;    }

    int moveTo(char *pBuf, int size);
    int pop_front(int size);
    int pop_end(int size);
    void swap(AutoBuf &rhs);
    int push_front_deepcopy(const char *pBuf, int size)
    {
        return insert_deepcopy(0, pBuf, size);
    }
    int insert_deepcopy(int pos, const char *pBuf, int size);
    int make_room_deepcopy(int pos, int size);
};

#endif
