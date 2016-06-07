/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include "reactorindex.h"
#include <edio/eventreactor.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

ReactorIndex::ReactorIndex()
    : m_pIndexes(NULL)
    , m_capacity(0)
    , m_iUsed(0)
{
}

ReactorIndex::~ReactorIndex()
{
    deallocate();
}

int ReactorIndex::allocate(int capacity)
{
    ReactorHolder *pIndexes = (ReactorHolder *) realloc(m_pIndexes,
                              capacity * sizeof(ReactorHolder));
    if (!pIndexes)
        return LS_FAIL;
    if ((unsigned)capacity > m_capacity)
        memset(pIndexes + m_capacity, 0,
               sizeof(ReactorHolder) * (capacity - m_capacity));
    m_pIndexes = pIndexes;
    m_capacity = capacity;
    return LS_OK;
}

int ReactorIndex::deallocate()
{
    if (m_pIndexes)
        free(m_pIndexes);
    return LS_OK;
}


//#include <typeinfo>
//#include <unistd.h>
//#include <http/httplog.h>

void ReactorIndex::timerExec()
{
    unsigned int i;
    while (((m_iUsed) > 0) && (m_pIndexes[m_iUsed].m_pReactor == NULL))
        --m_iUsed;
    for (i = 0; i <= m_iUsed; ++i)
    {
        EventReactor *pReactor = m_pIndexes[i].m_pReactor;
        if (pReactor)
        {
            if (pReactor->getfd() == (int)i)
                pReactor->onTimer();
            else
            {
//                LS_ERROR( "[%d] ReactorIndex[%d]=%p, getfd()=%d, type: %s", getpid(), i, m_pIndexes[i], m_pIndexes[i]->getfd(),
//                typeid( *m_pIndexes[i] ).name() ));

                m_pIndexes[i].m_pReactor = NULL;
            }
        }
    }
}

