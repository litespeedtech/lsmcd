/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include <util/objpool.h>

GObjPool::GObjPool(int chunkSize)
    : m_chunkSize(chunkSize)
    , m_poolSize(0)
{
}

int GObjPool::allocate(int size)
{
    if ((int)m_freeList.capacity() < m_poolSize + size)
        if (m_freeList.reserve(m_poolSize + size))
            return -1;
    int i = 0;
    try
    {
        for (; i < size; ++i)
        {
            void *pObj = newObj();
            if (pObj)
            {
                m_freeList.safe_push_back(pObj);
                ++m_poolSize;
            }
            else
                return -1;
        }
    }
    catch (...)
    {
        return -1;
    }
    return 0;
}


