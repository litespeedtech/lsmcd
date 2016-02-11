/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef HASHDATACACHE_H
#define HASHDATACACHE_H


/**
  *@author George Wang
  */

#include <stdio.h>
#include <sys/types.h>
#include <util/hashstringmap.h>

class KeyData;

class HashDataCache : public HashStringMap< KeyData *>
{
    int     m_cacheExpire;
    int     m_maxCacheSize;

    HashDataCache(const HashDataCache &);
    HashDataCache &operator=(const HashDataCache &);
public:
    HashDataCache(int expire, int cacheSize)
        : m_cacheExpire(expire)
        , m_maxCacheSize(cacheSize)
    {}

    HashDataCache()
        : m_cacheExpire(10)
        , m_maxCacheSize(1024)
    {}
    ~HashDataCache();
    void setExpire(int expire)    {   m_cacheExpire = expire;     }
    int  getExpire() const          {   return m_cacheExpire;       }
    void setMaxSize(int size)     {   m_maxCacheSize = size;      }
    int  getMaxSize() const         {   return m_maxCacheSize;      }

    const KeyData *getData(const char *pKey);
};

class DataStore
{
    char   *m_uriDataStore;

public:
    DataStore()
        : m_uriDataStore(NULL)
    {}
    virtual ~DataStore();
    void setDataStoreURI(const char *pURI);
    const char *getDataStoreURI() const   {   return m_uriDataStore;  }

    virtual KeyData *getDataFromStore(const char *pKey, int len) = 0;
    virtual int       isStoreChanged(long time) = 0;
    virtual KeyData *newEmptyData(const char *pKey, int len) = 0;
    virtual KeyData *getNext()     {   return NULL;    }
};

class FileStore : public DataStore
{
    time_t        m_lModifiedTime;
    time_t        m_lLastCheckTime;

    FILE         *m_fp;

protected:
    KeyData *getDataFromStore(const char *pKey, int keyLen);
    virtual KeyData *parseLine(char *pLine, char *pLineEnd) = 0;
    virtual KeyData *parseLine(const char *pKey, int keyLen,
                               const char *pLine, const char *pLineEnd) = 0;

public:
    FileStore()
        : m_lModifiedTime(1)
        , m_lLastCheckTime(0)
        , m_fp(NULL)
    {}
    virtual ~FileStore() {}
    virtual int isStoreChanged(long time);
    virtual KeyData *getNext();
    int open();
    void close();
};

#endif
