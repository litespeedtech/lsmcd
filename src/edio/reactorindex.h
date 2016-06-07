/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef REACTORINDEX_H
#define REACTORINDEX_H


#include <lsdef.h>
#include <stddef.h>

#define MAX_FDINDEX 100000
class EventReactor;

typedef struct ReactorHolder
{
    EventReactor   *m_pReactor;
    unsigned short  m_eventSet;
    unsigned short  m_flags;
    
}ReactorHolder;

class ReactorIndex
{
public:
    ReactorHolder  *m_pIndexes;
    unsigned int    m_capacity;
    unsigned int    m_iUsed;

    int deallocate();

public:
    ReactorIndex();
    ~ReactorIndex();

    unsigned int getUsed() const        {   return m_iUsed;         }
    unsigned int getCapacity() const    {   return m_capacity;      }

    int allocate(int capacity);

    EventReactor *get(int fd) const
    {   return ((unsigned)fd <= m_iUsed) ? m_pIndexes[fd].m_pReactor : NULL;  }

    int set(int fd, EventReactor *pReactor)
    {
        if ((unsigned)fd >= m_capacity)
        {
            if ((unsigned)fd > MAX_FDINDEX)
                return LS_FAIL;
            int new_cap = m_capacity * 2;
            if (new_cap <= fd)
                new_cap = fd + 1;
            if (allocate(new_cap) == -1)
                return LS_FAIL;
        }
        if ((unsigned)fd > m_iUsed)
            m_iUsed = fd;
        m_pIndexes[fd].m_pReactor = pReactor;
        return LS_OK;
    }
    
    void setUpdateFlags(int fd, int val)
    {   m_pIndexes[fd].m_flags = val;   }
    
    unsigned short getUpdateFlags(int fd) const
    {   return m_pIndexes[fd].m_flags;  }
    
    void timerExec();
    int verify(int fd, EventReactor *pReactor)
    {
        if (((unsigned)fd <= m_iUsed) && (m_pIndexes[fd].m_pReactor == pReactor))
            return LS_OK;
        return LS_FAIL;
    }
    
    LS_NO_COPY_ASSIGN(ReactorIndex);
};

#endif
