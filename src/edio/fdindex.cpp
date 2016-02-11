/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include "fdindex.h"

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

FdIndex::FdIndex()
    : m_pIndexes(NULL)
    , m_capacity(0)
{
}
FdIndex::~FdIndex()
{
    deallocate();
}

int FdIndex::allocate(int capacity)
{
    unsigned short *pIndexes = (unsigned short *) realloc(m_pIndexes,
                               capacity * sizeof(short));
    if (!pIndexes)
        return -1;
    if (capacity > m_capacity)
        memset(pIndexes + m_capacity, -1,
               sizeof(short) * (capacity - m_capacity));
    m_pIndexes = pIndexes;
    m_capacity = capacity;
    return 0;
}

int FdIndex::deallocate()
{
    if (m_pIndexes)
        free(m_pIndexes);
    return 0;
}


