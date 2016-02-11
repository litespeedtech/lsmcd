/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef STRINGLIST_H
#define STRINGLIST_H


/**
  *@author George Wang
  */

#include <util/gpointerlist.h>
class AutoStr2;
class StringList: public TPointerList<AutoStr2>
{
public:
    StringList()    {}
    ~StringList();
    StringList(const StringList &rhs);
    const AutoStr2 *add(const char *pStr, int len);
    const AutoStr2 *add(const char *pStr);
    const AutoStr2 *find(const char *pString) const;
    int split(const char *pBegin, const char *pEnd, const char *delim);
    void remove(const char *pString);
    void clear();
    void sort();
    AutoStr2 *bfind(const char *pStr) const;
    void insert(AutoStr2 *pDir);
    AutoStr2 *const *lower_bound(const char *pStr) const;
    int compare(const StringList &rhs) const;
};

class ChainedStringList: public StringList
{
public:
    ChainedStringList()
        : StringList()
        , m_pNext(NULL)
    {}
    ChainedStringList *getNext() const  {   return m_pNext;     }
    void setNext(ChainedStringList *p) {   m_pNext = p;        }

private:
    ChainedStringList *m_pNext;
};
#endif
