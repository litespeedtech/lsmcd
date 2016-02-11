/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef GFACTORY_H
#define GFACTORY_H


/**
  *@author George Wang
  */
#include <util/hashstringmap.h>

class Duplicable;

class GFactory
{
    typedef HashStringMap<Duplicable *> _store;

    _store      m_typeRegistry;
    _store      m_objRegistry;
    Duplicable *remove(_store *pStore, const char *pName);

public:
    GFactory();
    ~GFactory();
    Duplicable *getObj(const char *pName, const char *pType);
    Duplicable *removeObj(const char *pName)
    {   return remove(&m_objRegistry, pName);     }
    Duplicable *removeType(const char *pType)
    {   return remove(&m_typeRegistry, pType);    }
    int registerType(Duplicable *pType);
};

#endif
