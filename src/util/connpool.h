/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef CONNPOOL_H
#define CONNPOOL_H


/**
  *@author George Wang
  */

#include <util/gpointerlist.h>
#include <util/iconnection.h>
#include <assert.h>

class ConnPool
{
    TPointerList<IConnection>   m_connList;
    TPointerList<IConnection>   m_freeList;
    TPointerList<IConnection>   m_badList;
    int                         m_iMaxConns;
public:
    ConnPool() : m_iMaxConns(0) {};
    ~ConnPool();

    int setMaxConns(int max);
    void decMaxConn()           {   --m_iMaxConns;      }
    int getMaxConns() const     {   return m_iMaxConns; }
    void adjustMaxConns()       {   m_iMaxConns = m_connList.size();    }

    int getTotalConns() const   {   return m_connList.size();   }

    int getFreeConns() const    {   return m_freeList.size();   }

    int getUsedConns() const    {   return m_connList.size() - m_freeList.size();   }

    int regConn(IConnection *pConn);

    IConnection *getFreeConn()
    {
        if (m_freeList.empty())
            return NULL;
        else
            return m_freeList.pop_back();
    }

    IConnection *getBadConn()
    {
        if (m_badList.empty())
            return NULL;
        else
            return m_badList.pop_back();
    }

    void for_each_conn(gpl_for_each_fn2 fn, void *param)
    {   m_connList.for_each2(m_connList.begin(), m_connList.end(), fn, param);    }

    void reuse(IConnection *pConn)
    {
        assert(pConn);
        m_freeList.safe_push_back(pConn);
    }

    int  inFreeList(IConnection *pConn);
    void removeConn(IConnection *pConn);
    int canAddMore() const
    {   return m_iMaxConns > (int)m_connList.size(); }
};


#endif

