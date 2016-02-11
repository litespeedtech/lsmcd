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
    EventReactor **pIndexes = (EventReactor **) realloc(m_pIndexes,
                              capacity * sizeof(EventReactor *));
    if (!pIndexes)
        return -1;
    if ((unsigned)capacity > m_capacity)
        memset(pIndexes + m_capacity, 0,
               sizeof(EventReactor *) * (capacity - m_capacity));
    m_pIndexes = pIndexes;
    m_capacity = capacity;
    return 0;
}

int ReactorIndex::deallocate()
{
    if (m_pIndexes)
        free(m_pIndexes);
    return 0;
}


//#include <typeinfo>
//#include <unistd.h>
//#include <http/httplog.h>

void ReactorIndex::timerExec()
{
    int i;
    while (((m_iUsed) > 0) && (m_pIndexes[m_iUsed] == NULL))
        --m_iUsed;
    for (i = 0; i <= (int)m_iUsed; ++i)
    {
        if (m_pIndexes[i])
        {
            if (m_pIndexes[i]->getfd() == i)
                m_pIndexes[i]->onTimer();
            else
            {
//                LS_ERROR( "[%d] ReactorIndex[%d]=%p, getfd()=%d, type: %s", getpid(), i, m_pIndexes[i], m_pIndexes[i]->getfd(),
//                typeid( *m_pIndexes[i] ).name() ));

                m_pIndexes[i] = NULL;
            }
        }
    }
}

