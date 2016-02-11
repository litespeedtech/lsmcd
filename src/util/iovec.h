/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef IOVEC_H
#define IOVEC_H


#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>

#define MAX_VECTOR_LEN 15

class IOVec
{
    struct iovec *m_pBegin;
    struct iovec *m_pEnd;
    struct iovec  m_store[MAX_VECTOR_LEN];

public:
    IOVec()
        : m_pBegin((struct iovec *)m_store + 5)
        , m_pEnd((struct iovec *)m_store + 5)
    {
    };

    explicit IOVec(int begin)
        : m_pBegin((struct iovec *)m_store + begin)
        , m_pEnd((struct iovec *)m_store + begin)
    {};

    ~IOVec() {};

    IOVec(const IOVec &rhs)
        : m_pBegin(m_store + (rhs.m_pBegin - rhs.m_store))
        , m_pEnd(m_store + (rhs.m_pEnd - rhs.m_store))
    {
        memmove(m_pBegin, rhs.m_pBegin, rhs.len()* sizeof(struct iovec));
    }

    IOVec(const struct iovec *vector, int size)
        : m_pBegin((struct iovec *)m_store + 5)
        , m_pEnd((struct iovec *)m_store + 5)
    {   append(vector, size); }

    IOVec(const char *buf, size_t len)
        : m_pBegin((struct iovec *)m_store + 5)
        , m_pEnd((struct iovec *)m_store + 5)
    {
        append(buf, len);
    }

    const struct iovec *get() const
    {   return m_pBegin;   }

    int len() const
    {   return m_pEnd - m_pBegin;  }

    bool empty() const
    {   return m_pEnd == m_pBegin;  }

    void append(const void *pBuf, size_t size)
    {
        //assert(( pBuf )&&( size > 0 ));
        assert(m_pEnd < (struct iovec *)m_store + MAX_VECTOR_LEN);
        m_pEnd->iov_base = (char *)pBuf;
        m_pEnd->iov_len = size;
        ++m_pEnd;
    }

    void appendCombine(const void *pBuf, size_t size)
    {
        if (m_pBegin != m_pEnd &&
            pBuf == (char *)((m_pEnd - 1)->iov_base) + (m_pEnd - 1)->iov_len)
            (m_pEnd - 1)->iov_len += size;
        else
            append(pBuf, size);
    }

    void append(const struct iovec *vector, int len)
    {
        assert(len <= (struct iovec *)m_store + MAX_VECTOR_LEN - m_pEnd);
        memmove(m_pEnd, vector, sizeof(struct iovec) * len);
        m_pEnd += len;
    }

    void append(const IOVec &rhs)
    {
        append(rhs.get(), rhs.len());
    }

    void push_front(const void *pBuf, size_t size)
    {
        if ((pBuf) && (size > 0))
        {
            //assert( m_pBegin > m_store );
            --m_pBegin;
            m_pBegin->iov_base = (char *)pBuf;
            m_pBegin->iov_len = size;
        }
    }

    void push_front(const struct iovec *vector, int len)
    {
        //assert( len < m_pBegin - m_store );
        m_pBegin -= len;
        memmove(m_pBegin, vector, sizeof(struct iovec) * len);
    }

    void push_front(const IOVec &rhs)
    {
        push_front(rhs.get(), rhs.len());
    }

    void pop_front(int len)
    {
        //assert( len <= m_pEnd - m_pBegin );
        m_pBegin += len;
    }
    void pop_back(int len)
    {
        //assert( len <= m_pEnd - m_pBegin );
        m_pEnd -= len;
    }


    void clear()
    {
        m_pBegin = m_pEnd = (struct iovec *)m_store + 5;
    }

    int  bytes() const;
    int  shrinkTo(unsigned int newSize, unsigned int additional);
    int  finish(int &finishLen);
    int  avail() const
    {   return &m_store[MAX_VECTOR_LEN] - m_pEnd;  }

    typedef struct iovec *iterator;
    typedef const struct iovec *const_iterator;
    iterator begin() {  return m_pBegin;    }
    iterator end()   {  return m_pEnd;      }
    const_iterator begin() const {  return m_pBegin;    }
    const_iterator end() const   {  return m_pEnd;      }
    void setEnd(iterator end)         {   m_pEnd = end;   }
    void setBegin(iterator begin)     {   m_pBegin = begin;   }
    void adjust(const char *pOld, const char *pNew, int len);
};

#endif
