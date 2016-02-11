/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include <util/stringlist.h>
#include <util/autostr.h>
#include <util/stringtool.h>
#include <stdlib.h>
#include <string.h>

StringList::StringList(const StringList &rhs)
    : TPointerList< AutoStr2 >()
{
    for (const_iterator iter = rhs.begin(); iter != rhs.end(); ++iter)
        add((*iter)->c_str(), (*iter)->len());
}




StringList::~StringList()
{
    release_objects();
}

const AutoStr2 *StringList::add(const char *pStr, int len)
{
    AutoStr2 *pTemp = new AutoStr2();
    if (!pTemp)
        return NULL;
    pTemp->setStr(pStr, len);
    push_back(pTemp);
    return pTemp;
}


const AutoStr2 *StringList::add(const char *pIndexFile)
{
    AutoStr2 *pTemp = new AutoStr2(pIndexFile);
    if (!pTemp)
        return NULL;
    push_back(pTemp);
    return pTemp;
}


int StringList::split(const char *pBegin, const char *pEnd,
                      const char *delim)
{
    StrParse strparse(pBegin, pEnd, delim, strlen(delim));
    const char *p;
    while (!strparse.isEnd())
    {
        p = strparse.trim_parse();
        if (!p)
            break;
        if (p != strparse.getStrEnd())
        {
            if (!add(p, strparse.getStrEnd() - p))
                break;
        }
    }
    return size();
}


void StringList::clear()
{
    release_objects();
}

const AutoStr2 *StringList::find(const char *pString) const
{
    if (pString)
    {
        for (const_iterator iter = begin(); iter != end(); ++iter)
        {
            if (strcmp(pString, (*iter)->c_str()) == 0)
                return *iter;
        }
    }
    return NULL;
}

void StringList::remove(const char *pString)
{
    for (iterator iter = begin(); iter != end(); ++iter)
    {
        if (pString == (*iter)->c_str())
        {
            delete(*iter);
            erase(iter);
            break;
        }
    }
}

static int cmp(const void *val1, const void *val2)
{
    return strcmp((*(const AutoStr2 **)val1)->c_str(),
                  (*(const AutoStr2 **)val2)->c_str());
}

void StringList::sort()
{
    ::qsort(begin(), size(), sizeof(AutoStr2 **), cmp);
}



void StringList::insert(AutoStr2 *pDir)
{
    push_back(pDir);
    sort();
}

AutoStr2 *const *StringList::lower_bound(const char *pStr) const
{
    if (!pStr)
        return NULL;
    const_iterator e = end();
    const_iterator b = begin();
    int c = -1;
    while (b < e)
    {
        const_iterator m = b + (e - b) / 2;
        c = strcmp(pStr, (*m)->c_str());
        if (c == 0)
            return m;
        else if (c < 0)
            e = m;
        else
            b = m + 1;
    }
    return b;

}

AutoStr2 *StringList::bfind(const char *pPath) const
{
    AutoStr2 *const *p = lower_bound(pPath);
    if ((p != end()) && (strcmp(pPath, (*p)->c_str()) == 0))
        return *p;
    return NULL;
}

int StringList::compare(const StringList &rhs) const
{
    int ret = size() - rhs.size();
    if ( ret )
        return ret;
    const_iterator iter1, iter2;
    for( iter1 = begin(), iter2 = rhs.begin(); iter1 != end(); ++iter1, ++iter2 )
    {
        ret = strcmp( (*iter1)->c_str(), (*iter2)->c_str() );
        if ( ret )
            return ret;
    }
    return 0;
}



