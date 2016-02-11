/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include <util/autostr.h>
#include <util/pool.h>

#include <string.h>

AutoStr::AutoStr()
    : m_pStr(NULL)
{
}
AutoStr::~AutoStr()
{
    if (m_pStr)
        g_pool.deallocate2(m_pStr);
}

AutoStr::AutoStr(const AutoStr &rhs)
    : m_pStr(NULL)
{
    if (rhs.c_str())
        m_pStr = g_pool.dupstr(rhs.c_str());
}

char *AutoStr::prealloc(int size)
{
    char *p = (char *)g_pool.reallocate2(m_pStr, size);
    if (p)
        m_pStr = p;
    return p;
}


AutoStr::AutoStr(const char *pStr)
{
    m_pStr = g_pool.dupstr(pStr);
}

int AutoStr::setStr(const char *pStr)
{
    int len = strlen(pStr);
    return setStr(pStr, len);
}

int AutoStr::setStr(const char *pStr, int len)
{
    if (m_pStr)
        g_pool.deallocate2(m_pStr);
    m_pStr = g_pool.dupstr(pStr, len + 1);
    *(m_pStr + len) = 0;
    return len;
}

AutoStr &AutoStr::operator=(const char *pStr)
{
    setStr(pStr);
    return *this;
}

AutoStr2::AutoStr2()
    : m_iStrLen(0)
{}

AutoStr2::AutoStr2(const AutoStr2 &rhs)
    : AutoStr()
    , m_iStrLen(0)
{
    if (rhs.c_str())
        setStr(rhs.c_str(), rhs.len());
}

int AutoStr2::setStr(const char *pStr)
{   return setStr(pStr, strlen(pStr));   }

void AutoStr2::append(const char *str, const int len)
{
    char *p = prealloc(m_iStrLen + len + 1);
    if (p)
    {
        memcpy(p + m_iStrLen, str, len);
        m_iStrLen += len;
        *(p + m_iStrLen) = 0;
    }
}

