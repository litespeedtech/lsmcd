/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef HASHSTRINGMAP_H
#define HASHSTRINGMAP_H


/**
  *@author George Wang
  */

#include <util/ghash.h>
#include <util/autostr.h>

template< class T >
class HashStringMap
    : public GHash
{
public:
    class iterator
    {
        GHash::iterator m_iter;
    public:
        iterator() : m_iter(NULL)
        {}

        iterator(GHash::iterator iter) : m_iter(iter)
        {}
        iterator(GHash::const_iterator iter)
            : m_iter((GHash::iterator)iter)
        {}

        iterator(const iterator &rhs) : m_iter(rhs.m_iter)
        {}

        const char *first() const
        {  return (const char *)(m_iter->first());   }

        T second() const
        {   return (T)(m_iter->second());   }

        operator GHash::iterator()
        {   return m_iter;  }

    };
    typedef iterator const_iterator;

    HashStringMap(int initsize = 29,
                  GHash::hash_fn hf = GHash::hash_string,
                  GHash::val_comp vc = GHash::comp_string)
        : GHash(initsize, hf, vc)
    {};
    ~HashStringMap() {};

    iterator insert(const char *pKey, const T &val)
    {
        return GHash::insert(pKey, val);
    }

    iterator update(const char *pKey, const T &val)
    {
        return GHash::update(pKey, val);
    }

    iterator remove(const char *pKey)
    {
        iterator iter = GHash::find(pKey);
        if (iter != end())
            GHash::erase(iter);
        return iter;
    }

    static int deleteObj(GHash::iterator iter)
    {
        delete(T)(iter->second());
        return 0;
    }

    void release_objects()
    {
        GHash::for_each(begin(), end(), deleteObj);
        GHash::clear();
    }

};

class StrStr
{
public:
    AutoStr str1;
    AutoStr str2;
};

class StrStrHashMap : public HashStringMap<StrStr *>
{
public:
    StrStrHashMap(int initsize = 29, GHash::hash_fn hf = GHash::hash_string,
                  GHash::val_comp vc = GHash::comp_string)
        : HashStringMap<StrStr * >(initsize, hf, vc)
    {};
    ~StrStrHashMap() {  release_objects();   };
    iterator insert_update(const char *pKey, const char *pValue)
    {
        iterator iter = find(pKey);
        if (iter != end())
        {
            iter.second()->str2.setStr(pValue);
            return iter;
        }
        else
        {
            StrStr *pStr = new StrStr();
            pStr->str1.setStr(pKey);
            pStr->str2.setStr(pValue);
            return insert(pStr->str1.c_str(), pStr);
        }
    }


};


#endif
