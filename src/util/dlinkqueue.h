/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef DLINKQUEUE_H
#define DLINKQUEUE_H


/**
  *@author George Wang
  */
#include <util/linkedobj.h>

class LinkQueue
{
    LinkedObj   m_head;
    int         m_iTotal;

    LinkQueue(const LinkQueue &rhs);
    LinkQueue &operator=(const LinkQueue &rhs);

public:
    LinkQueue()
        : m_iTotal(0)
    {}
    ~LinkQueue() {}

    void clear()
    {
        m_head.setNext(NULL);
        m_iTotal = 0;
    }

    int size() const            {   return m_iTotal;        }
    LinkedObj *begin() const   {   return m_head.next();   }
    LinkedObj *end() const     {   return NULL;            }
    LinkedObj *head()          {   return  &m_head;        }

    void push(LinkedObj *pObj)
    {   m_head.addNext(pObj);     ++m_iTotal;     }

    LinkedObj *pop()
    {
        if (m_iTotal)
        {   --m_iTotal; return m_head.removeNext(); }
        return NULL;
    }

    LinkedObj *removeNext(LinkedObj *pObj)
    {   --m_iTotal; return pObj->removeNext();  }

};

class DLinkQueue
{
    DLinkedObj      m_head;
    int             m_iTotal;

    DLinkQueue(const DLinkQueue &rhs);
    DLinkQueue &operator=(const DLinkQueue &rhs);

public:
    DLinkQueue()
        : m_head(&m_head, &m_head)
        , m_iTotal(0)
    {}
    ~DLinkQueue()
    {}

    int size() const        {   return m_iTotal;    }
    bool empty() const      {   return m_head.next() == &m_head;   }
    DLinkedObj *begin()    {   return m_head.next();    }
    DLinkedObj *end()      {   return &m_head;          }

    void clear()
    {
        m_head.setNext(&m_head);
        m_head.setPrev(&m_head);
        m_iTotal = 0;
    }

    void append(DLinkedObj *pReq)
    {
        m_head.prev()->addNext(pReq);
        ++m_iTotal;
    }

    void push_front(DLinkedObj *pReq)
    {
        m_head.next()->addPrev(pReq);
        ++m_iTotal;
    }

    void insert_after(DLinkedObj *pReq, DLinkedObj *pReqToAppend)
    {
        pReq->addNext(pReqToAppend);
        ++m_iTotal;
    }

    void remove(DLinkedObj *pReq)
    {
        assert(pReq != &m_head);
        if (pReq->next())
        {
            pReq->remove();
            --m_iTotal;
        }
    }

    DLinkedObj *pop_front()
    {
        if (m_head.next() != &m_head)
        {
            --m_iTotal;
            return m_head.removeNext();
        }
        assert(m_iTotal = 0);
        return NULL;
    }



    void takeover(DLinkQueue &rhs)
    {
        if (rhs.empty())
            return;
        m_head.setNext(rhs.begin());
        m_head.next()->setPrev(&m_head);
        m_head.setPrev(rhs.end()->prev());
        m_head.prev()->setNext(&m_head);
        m_iTotal = rhs.m_iTotal;
        rhs.clear();
    }

};

template< typename T>
class TDLinkQueue : public DLinkQueue
{
public:
    TDLinkQueue()   {}
    ~TDLinkQueue()  {}

    void release_objects()
    {
        T *p;
        while ((p = pop_front()) != NULL)
            delete p;
    }

    void append(T *pReq)
    {   DLinkQueue::append(pReq);     }

    void push_front(T *pReq)
    {   DLinkQueue::push_front(pReq); }

    void remove(T *pReq)
    {   DLinkQueue::remove(pReq);     }

    T *next(const T *pObj)
    {   return (T *)pObj->next();       }

    T *pop_front()
    {   return (T *)DLinkQueue::pop_front();  }

    T *begin()
    {   return (T *)DLinkQueue::begin();      }

    T *end()
    {   return (T *)DLinkQueue::end();    }

};

#endif
