/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include "bufferedos.h"
#include "util/iovec.h"
//#include <http/httplog.h>

#include <stdio.h>



BufferedOS::BufferedOS(OutputStream *pOS, int initSize)
    : m_pOS(pOS)
    , m_buf(initSize)
{}

BufferedOS::~BufferedOS()
{}


int BufferedOS::write(const char *pBuf, int size)
{
    return writeEx(pBuf, size, 1);
}

int BufferedOS::writev(const struct iovec *vector, int len)
{
    IOVec iov(vector, len);
    return writevEx(iov, 1);
}

int BufferedOS::writeEx(const char *pBuf, int size, int avoidCache)
{
    assert(m_pOS != NULL);
    int ret = 0;
    if (m_buf.empty())
    {
        ret = m_pOS->write(pBuf, size);
        if (ret >= avoidCache)
            ret = m_buf.cache(pBuf, size, ret);
//        LS_DBG_H( "bufferedOS::write() return %d, %d bytes in cache\n", ret, m_buf.size() ));
        return ret;
    }
    else
    {
        IOVec iov(pBuf, size);
        return writevEx(iov, avoidCache);
    }

}


int BufferedOS::cacheWritev(IOVec &vector, int total)
{
    if (m_buf.size() + total < 16384)
    {
        m_buf.cache(vector.get(), vector.len(), 0);
        return total;
    }
    else
        return writevEx(vector, 0);
}


int BufferedOS::writev(IOVec &vec)
{
    return writevEx(vec, 1);
}


int BufferedOS::writevEx(IOVec &vec, int avoidCache)
{
    assert(m_pOS != NULL);
    int ret;
    if (!m_buf.empty())
    {
        int oldLen = vec.len();
        m_buf.iov_insert(vec);
        //FIXME: DEBUG Code
        //ret = 0;
        ret = m_pOS->writev(vec);
        vec.pop_front(vec.len() - oldLen);
        if (ret > 0)
        {
            int pop = ret;
            if (pop > m_buf.size())
                pop = m_buf.size();
            m_buf.pop_front(pop);
            ret -= pop;
        }
    }
    else
        //FIXME: DEBUG Code
        //ret = 0;
        ret = m_pOS->writev(vec);
    if (ret >= avoidCache)
        ret = m_buf.cache(vec.get(), vec.len(), ret);
    //LS_DBG_H( "bufferedOS::writev() return %d, %d bytes in cache\n", ret, m_buf.size() ));
    return ret;
}

int BufferedOS::flush()
{
    assert(m_pOS != NULL);
    int ret = 0;
    if (!m_buf.empty())
    {
        IOVec iovector;
        m_buf.getIOvec(iovector);
        ret = m_pOS->writev(iovector);
        if (ret >= 0)
        {
            if (m_buf.size() <= ret)
                m_buf.clear();
            else
                m_buf.pop_front(ret);
//            LS_DBG_H( "bufferedOS::flush() writen %d, %d bytes in cache\n",
//                        ret, m_buf.size() ));
            ret = m_buf.size();
        }
    }
    return ret;
}

int BufferedOS::close()
{
    assert(m_pOS != NULL);
    flush();
    return m_pOS->close();
}


