/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */

#include <util/strmatchrescache.h>

int StrMatchResCache::find(hash_key_t key, int& isMatch)
{
    isMatch = 0;
    GHash::iterator pElem = m_cache.find((void *)key);
    if (pElem == NULL)
        return SMRC_NOTFOUND;
    isMatch = (int)(long)pElem->getData();
    if (isMatch == -1)
        return SMRC_INVALID;
    return SMRC_FOUND;
}


int StrMatchResCache::invalidate(hash_key_t key)
{
    int match = -1;
    GHash::iterator pElem = m_cache.find((void *)key);
    if (pElem == NULL)
        return 0;
    pElem->setData((void *)(long)match);
    return 1;
}




