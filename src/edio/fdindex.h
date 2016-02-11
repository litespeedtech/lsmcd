/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef FDINDEX_H
#define FDINDEX_H


/**
  *@author George Wang
  */

class FdIndex
{
    unsigned short *m_pIndexes;
    int             m_capacity;

    int deallocate();

public:
    FdIndex();
    ~FdIndex();
    int getCapacity() const     {   return m_capacity;      }
    unsigned short get(int fd) const     {   return m_pIndexes[fd];  }
    int allocate(int capacity);
    int set(int fd, int index)
    {
        if (fd >= m_capacity)
        {
            int new_cap = m_capacity * 2;
            if (new_cap <= fd)
                new_cap = fd + 1;
            if (allocate(new_cap) == -1)
                return -1;
        }
        m_pIndexes[fd] = index;
        return 0;
    }

};

#endif
