/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef LINKOBJPOOL_H
#define LINKOBJPOOL_H


#include <util/dlinkqueue.h>

template <class _Obj >
class LinkObjPool
{
    LinkQueue   m_pool;
    int         m_chunk;
    _Obj *newObj()
    {   return new _Obj();  }

    void releaseObj(void *pObj)
    {   delete(_Obj *)pObj; }

    int allocate(int size)
    {
        while (size-- > 0)
            m_pool.push(newObj());
        return 0;
    }

    void release()
    {
        _Obj *pObj;
        while ((pObj = (_Obj *)m_pool.pop()))
            delete pObj;
    }

public:
    explicit LinkObjPool(int initSize = 10, int chunkSize = 10)
        : m_chunk(chunkSize)
    {
        if (initSize)
            allocate(initSize);
    }

    ~LinkObjPool()
    {   release();  }

    _Obj *get()
    {
        _Obj *pObj = (_Obj *)m_pool.pop();
        if (!pObj)
        {
            allocate(m_chunk);
            pObj = (_Obj *)m_pool.pop();
        }
        return pObj;
    }

    int get(_Obj **pObj, int n)
    {
        int i;
        for (i = 0; i < n; ++i)
        {
            *pObj = get();
            if (!*pObj)
                break;
            ++pObj;
        }
        return i;
    }

    void recycle(LinkedObj *pObj)
    {
        if (pObj)
            m_pool.push(pObj);
    }

    void recycle(LinkedObj **pObj, int n)
    {
        LinkedObj **pEnd = pObj + n;
        while (pObj < pEnd)
        {
            recycle(*pObj);
            ++pObj;
        }
    }

};


#endif
