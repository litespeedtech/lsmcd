/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef KEYDATA_H
#define KEYDATA_H


#include <util/autostr.h>

class KeyData
{
    AutoStr     m_key;
public:
    KeyData() {};
    virtual ~KeyData() {};
    const char *getKey() const        {   return m_key.c_str();  }
    void setKey(const char *key)    {   m_key = key;           }
    void setKey(const char *pKey, int len)
    {   m_key.setStr(pKey, len); }

};

#endif
