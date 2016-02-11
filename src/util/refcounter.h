/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef REFCOUNTER_H
#define REFCOUNTER_H


/**
  *@author George Wang
  */

class RefCounter
{
    int     m_iRef;

public:
    RefCounter() : m_iRef(0) {};
    ~RefCounter() {};
    int incRef()            {   return ++m_iRef;    }
    int decRef()            {   return --m_iRef;    }
    int incRef(int n)     {   return m_iRef += n; }
    int decRef(int n)     {   return m_iRef -= n; }
    int getRef() const      {   return m_iRef;      }
};

#endif
