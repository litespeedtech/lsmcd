/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */

#ifndef STRMATCHRESCACHE_H
#define STRMATCHRESCACHE_H

#include <util/ghash.h>
#include <util/tsingleton.h>

#define SMRC_FOUND      1
#define SMRC_NOTFOUND   0
#define SMRC_INVALID   -1



class StrMatchResCache : public TSingleton<StrMatchResCache>
{
    friend class TSingleton<StrMatchResCache>;
    GHash  m_cache;

    StrMatchResCache(const StrMatchResCache &rhs);
    void operator=(const StrMatchResCache &rhs);
public:

    StrMatchResCache()
        : m_cache(13, NULL, NULL)
    {}
    ~StrMatchResCache()
    {}

    static hash_key_t getHashKey(const char *pStr, int iStrLen,
                                 hash_key_t seed = 0)
    {   return XXH(pStr, iStrLen, seed);   }
    static hash_key_t getCacheKey(hash_key_t iInputKey, hash_key_t iPatKey)
    {   return (iInputKey ^ iPatKey);   }

    int find(hash_key_t key, int &isMatch);

    int insert(hash_key_t key, int isMatch)
    {
        return (m_cache.insert((void *)key, (void *)(long)isMatch) != NULL);
    }

    int invalidate(hash_key_t key);
};

#endif
