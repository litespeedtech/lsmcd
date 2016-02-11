/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */

#ifndef DUPLICABLE_H
#define DUPLICABLE_H


/**
  *@author George Wang
  */

#include <util/autostr.h>

class Duplicable
{
    AutoStr2 m_sName;
public:
    Duplicable(const char *pName)
        : m_sName(pName)
    {}
    virtual ~Duplicable() {};
    virtual Duplicable *dup(const char *pName) = 0;
    const char *getName() const
    {   return m_sName.c_str(); }
    void setName(const char *pName)  {   m_sName.setStr(pName);    }
};

#endif
