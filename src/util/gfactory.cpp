/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include <util/gfactory.h>
#include <util/duplicable.h>

GFactory::GFactory()
{}
GFactory::~GFactory()
{
    m_typeRegistry.release_objects();
    m_objRegistry.release_objects();
}

Duplicable *GFactory::getObj(const char *pName, const char *pType)
{
    _store::iterator iter = m_objRegistry.find(pName);
    if (iter != m_objRegistry.end())
        return iter.second();
    _store::iterator proto = m_typeRegistry.find(pType);
    if (proto == m_typeRegistry.end())
        return NULL;
    Duplicable *pObj = proto.second()->dup(pName);
    if (pObj)
        m_objRegistry.insert(pObj->getName(), pObj);
    return pObj;
}

Duplicable *GFactory::remove(_store *pStore, const char *pName)
{
    _store::iterator iter = pStore->remove(pName);
    if (iter != pStore->end())
        return iter.second();
    else
        return NULL;
}

int GFactory::registerType(Duplicable *pType)
{
    if (!pType)
        return -1;
    m_typeRegistry.insert(pType->getName(), pType);
    return 0;
}


