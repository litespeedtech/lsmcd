/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef LINKEDOBJ_H
#define LINKEDOBJ_H


#include <assert.h>
#include <stddef.h>
#include <string.h>

class LinkedObj
{
    LinkedObj *m_pNext;

public:
    LinkedObj() : m_pNext(NULL) {}
    explicit LinkedObj(LinkedObj *next)
        : m_pNext(next)
    {}

    LinkedObj *next() const
    {   return m_pNext;     }

    void setNext(LinkedObj *pNext)
    {   m_pNext = pNext;        }

    void addNext(LinkedObj *pNext)
    {
        LinkedObj *pTemp = m_pNext;
        setNext(pNext);
        pNext->setNext(pTemp);
    }

    LinkedObj *removeNext()
    {
        LinkedObj *pNext = m_pNext;
        if (pNext)
        {
            setNext(pNext->m_pNext);
            pNext->setNext(NULL);
        }
        return pNext;
    }

    void swap(LinkedObj &rhs)
    {
        LinkedObj *pObj = m_pNext;
        m_pNext = rhs.m_pNext;
        rhs.m_pNext = pObj;
    }

};


class DLinkedObj : public LinkedObj
{
    DLinkedObj *m_pPrev;

public:

    DLinkedObj(): m_pPrev(NULL) {}
    DLinkedObj(DLinkedObj *prev, DLinkedObj *next)
        : LinkedObj(next), m_pPrev(prev)
    {}

    DLinkedObj *prev() const
    {   return m_pPrev;     }

    void setPrev(DLinkedObj *p)
    {   m_pPrev = p;        }

    DLinkedObj *next() const
    {   return (DLinkedObj *)LinkedObj::next();     }

    void addNext(DLinkedObj *pNext)
    {
        assert(pNext);
        DLinkedObj *pTemp = next();
        LinkedObj::addNext(pNext);
        pNext->m_pPrev = this;
        if (pTemp)
            pTemp->m_pPrev = pNext;
    }

    DLinkedObj *removeNext()
    {
        DLinkedObj *pNext = next();
        if (pNext)
        {
            setNext(pNext->next());
            if (next())
                next()->m_pPrev = this;
            memset(pNext, 0, sizeof(DLinkedObj));
        }
        return pNext;
    }

    void addPrev(DLinkedObj *pPrev)
    {
        assert(pPrev);
        DLinkedObj *pTemp;
        pTemp = m_pPrev;
        m_pPrev = pPrev;
        pPrev->m_pPrev = pTemp;
        pPrev->setNext(this);
        if (pTemp)
            pTemp->setNext(pPrev);
    }

    DLinkedObj *removePrev()
    {
        DLinkedObj *pPrev = m_pPrev;
        if (m_pPrev)
        {
            m_pPrev = pPrev->m_pPrev;
            if (m_pPrev)
                m_pPrev->setNext(this);
            memset(pPrev, 0, sizeof(DLinkedObj));
        }
        return pPrev;
    }

    DLinkedObj *remove()
    {
        DLinkedObj *pNext = next();
        if (pNext)
            pNext->m_pPrev = m_pPrev;
        if (m_pPrev)
            m_pPrev->setNext(next());
        memset(this, 0, sizeof(DLinkedObj));
        return pNext;
    }

    void swap(DLinkedObj &rhs)
    {
        DLinkedObj *pObj = m_pPrev;
        m_pPrev = rhs.m_pPrev;
        rhs.m_pPrev = pObj;
        LinkedObj::swap(rhs);
    }

};


class LinkObjHolder : public LinkedObj
{
public:
    LinkObjHolder()
        : m_pObj(NULL)
    {}

    explicit LinkObjHolder(void *p)
        : m_pObj(p)
    {}

    void *get()                {   return m_pObj;  }
    const void *get() const    {   return m_pObj;  }

    void set(void *p)        {   m_pObj = p;     }

private:
    void *m_pObj;
};


template< typename T >
class TLinkObjHolder : public LinkObjHolder
{
public:
    TLinkObjHolder()
    {}

    explicit TLinkObjHolder(T *p)
        : LinkObjHolder(p)
    {}

    T *get()                   {   return (T *)LinkObjHolder::get();  }
    const T *get() const       {   return (const T *)LinkObjHolder::get();  }

    void set(T *p)           {   LinkObjHolder::set(p);     }

};


class DLinkObjHolder : public DLinkedObj
{
public:
    DLinkObjHolder()
        : m_pObj(NULL)
    {}

    explicit DLinkObjHolder(void *p)
        : m_pObj(p)
    {}

    void *get()                {   return m_pObj;  }
    const void *get() const    {   return m_pObj;  }

    void set(void *p)        {   m_pObj = p;     }

private:
    void *m_pObj;
};


template< typename T >
class TDLinkObjHolder : public DLinkObjHolder
{
public:
    TDLinkObjHolder()
    {}

    explicit TDLinkObjHolder(T *p)
        : DLinkObjHolder(p)
    {}

    T *get()                   {   return (T *)DLinkObjHolder::get();  }
    const T *get() const       {   return (const T *)DLinkObjHolder::get();  }

    void set(T *p)           {   DLinkObjHolder::set(p);     }

};



#endif
