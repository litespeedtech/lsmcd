/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef STRINGMAP_H
#define STRINGMAP_H


/**
  *@author George Wang
  */

#include <string.h>

#include <map>
namespace std
{
template <>
class less< const char * >
{
public:
    bool operator()(const char *const &x, const char *const &y) const
    {   return (strcmp(x, y) < 0); }
};

}

template< class T >
class StringMap : public std::map< const char *, T >
{
    typedef std::map< const char *, T> CONT;

public:
    StringMap(int initsize = 0) {};
    ~StringMap() {};

    typedef typename CONT::iterator iterator;
    typedef typename CONT::const_iterator const_iterator;

    bool insert(const char *pKey, const T &val)
    {
        typename std::pair< iterator, bool > ret;
        ret = CONT::insert(std::make_pair(pKey, val));
        return ret.second;
    }
    bool insertUpdate(const char *pKey, const T &val)
    {
        typename std::pair< iterator, bool > ret;
        ret = CONT::insert(std::make_pair(pKey, val));
        if (!ret.second)
            ret.first->second = val;
        return true;
    }

    const_iterator find(const char *pKey) const
    {
        return CONT::find(pKey);
    }

    iterator find(const char *pKey)
    {
        return CONT::find(pKey);
    }

    size_t remove(const char *pKey)
    {
        return CONT::erase(pKey);
    }

    void release_objects()
    {
        iterator iter;
        for (iter = CONT::begin(); iter != CONT::end();)
        {
            T &p = iter->second;
            CONT::erase(iter);
            delete p;
            iter = CONT::begin();
        }
    }
};

#endif
