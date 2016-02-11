/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef AUTOSTR_H
#define AUTOSTR_H


/**
  *@author George Wang
  */

#include <string.h>

class AutoStr
{
    char *m_pStr;
    AutoStr &operator=(const AutoStr *pStr);

public:
    AutoStr();
    explicit AutoStr(const char *pStr);
    AutoStr(const AutoStr &);
    ~AutoStr();
    AutoStr &operator=(const char *pStr);
    AutoStr &operator=(const AutoStr &rhs)
    {
        setStr(rhs.c_str());
        return *this;
    }
    int setStr(const char *pStr);
    int setStr(const char *pStr, int len);
    char *prealloc(int size);
    char *buf()                    {   return m_pStr;  }
    char *buf() const              {   return m_pStr;  }
    const char *c_str()    const   {   return m_pStr;  }
    //operator const char *() const   {   return m_pStr;  }
    void swap(AutoStr &rhs)
    {
        char * p = m_pStr;
        m_pStr = rhs.m_pStr;
        rhs.m_pStr = p;
    }

};

class AutoStr2 : public AutoStr
{
    int m_iStrLen;
    AutoStr2 &operator=(const AutoStr *pStr);
    AutoStr2 &operator=(const AutoStr2 *pStr);
public:
    AutoStr2();
    AutoStr2(const AutoStr2 &);
    AutoStr2(const char *pStr)
    {   m_iStrLen = AutoStr::setStr(pStr); }
    AutoStr2(const char *pStr, int len)
        : m_iStrLen(len)
    {   AutoStr::setStr(pStr, len);   }

    ~AutoStr2() {}
    AutoStr2 &operator=(const char *pStr)
    {
        m_iStrLen = AutoStr::setStr(pStr);
        return *this;
    }
    AutoStr2 &operator=(const AutoStr2 &rhs)
    {
        m_iStrLen = AutoStr::setStr(rhs.c_str(), rhs.len());
        return *this;
    }

    int len() const         {   return m_iStrLen;   }
    void setLen(int len)  {   m_iStrLen = len;    }
    int setStr(const char *pStr);
    int setStr(const char *pStr, int len)
    {   m_iStrLen = len; return AutoStr::setStr(pStr, len);   }
    void append(const char *pStr, const int len);
    void swap(AutoStr2 &rhs)
    {
        AutoStr::swap(rhs);
        int len = m_iStrLen;
        m_iStrLen = rhs.m_iStrLen;
        rhs.m_iStrLen = len;
    }
};

static inline int operator==(const AutoStr &s1, const char *s2)
{   return ((s1.c_str() == s2) || (s1.c_str() && s2 && !strcmp(s1.c_str(), s2)));    }
static inline int operator==(const char *s1, const AutoStr &s2)
{   return ((s1 == s2.c_str()) || (s1 && s2.c_str() && !strcmp(s1, s2.c_str())));    }
static inline int operator==(const AutoStr &s1, const AutoStr2 &s2)
{   return ((s1.c_str() == s2.c_str()) || (s1.c_str() && s2.c_str() && !strcmp(s1.c_str(), s2.c_str())));    }

#endif
