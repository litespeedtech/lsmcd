/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef TLINKLIST_H
#define TLINKLIST_H


#include <util/linkedobj.h>

template< typename T>
class TLinkList
{
    LinkedObj m_first;
    LinkedObj *m_pLast;
public:
    TLinkList() : m_pLast(NULL) {};
    ~TLinkList() {};
    TLinkList(const TLinkList &rhs)
    {
        T *p = rhs.begin();
        LinkedObj *pNext = &m_first;
        T *pObj;
        while (p)
        {
            pObj = new T(*p);
            if (!pObj)
                break;
            pNext->addNext(pObj);
            pNext = pObj;
            p = (T *)p->next();
        }
        if (pNext == &m_first)
            m_pLast = NULL;
        else
            m_pLast = pNext;
    }
    LinkedObj *head()                  {   return &m_first;    }
    const LinkedObj *head() const      {   return &m_first;    }
    T *begin() const          {   return (T *)(m_first.next()); }
    LinkedObj *tail()
    {
        if ((m_pLast) && (m_pLast->next() == NULL))
            return m_pLast;
        LinkedObj *p = (m_pLast) ? m_pLast : m_first.next();
        while (p && p->next())
            p = p->next();
        m_pLast = p;
        return p;
    }

    LinkedObj *prev(LinkedObj *obj)
    {
        LinkedObj *p = m_first.next();
        if (p == obj)
            p = NULL;
        while (p && p->next() != obj)
            p = p->next();
        return p;
    }

    LinkedObj *remove_back()
    {
        LinkedObj *p = &m_first;
        while ((p->next()) && (p->next()->next()))
            p = p->next();
        m_pLast = p;
        p = p->removeNext();
        return p;
    }
    void release_objects()
    {
        LinkedObj *pDel;
        while ((pDel = m_first.removeNext()) != NULL)
            delete(T *)pDel;
        m_pLast = NULL;
    }
    void clear()
    {
        m_first.setNext(NULL);
        m_pLast = NULL;
    }

    T *pop()
    {
        LinkedObj * p = m_first.removeNext();
        if (!p)
            return NULL;
        if (m_first.next() == NULL)
            m_pLast = NULL;
        return (T *)p;
    }
    
    void append(T *p)
    {
        LinkedObj *pNext = tail();
        if (!pNext)
            pNext = &m_first;
        pNext->addNext(p);
        m_pLast = p;
    }

    void insert_front(T *p)
    {
        m_first.addNext(p);
        if (m_pLast == NULL)
            m_pLast = p;
    }

    int insert_front(LinkedObj *pHead, LinkedObj *pTail)
    {
        if (!pHead || !pTail || pTail->next())
            return -1;
        LinkedObj *pNext = m_first.next();
        m_first.setNext(pHead);
        pTail->setNext(pNext);
        if (m_pLast == NULL)
            m_pLast = pTail;
        return 0;
    }
    void swap(TLinkList &rhs)
    {
        LinkedObj tmp;
        tmp = m_first;
        m_first = rhs.m_first;
        rhs.m_first = tmp;
    }
};


#endif
