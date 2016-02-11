/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef STATICOBJ_H
#define STATICOBJ_H


/**
  *@author George Wang
  */
#include <new>
template< class T>
class StaticObj
{
    char m_achObj[ sizeof(T) ];

public:
    StaticObj() {};
    ~StaticObj() {};
    T *operator()()
    {   return (T *)m_achObj;    }
    void construct()
    {   new(m_achObj) T(); }
    void destruct()
    {   operator()()->~T(); }
};

#endif
