/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef LOOKUPFD_H
#define LOOKUPFD_H

#include <lsdef.h>
#include <assert.h>

/**
  *@author George Wang
  */

class LookUpFD
{
    /// number of elements allocated in m_fd2client[]
    int m_iCapacity;

    /// Resizable lookup table indexed by fd to find entry in m_pfds or m_clients
    int *m_pIntArray;

public:
    explicit LookUpFD(int iCapacity = 16)
        : m_iCapacity(0)
        , m_pIntArray(0)
    {
        if (iCapacity > 0)
            allocate(iCapacity);
    }

    ~LookUpFD()
    {   deallocate(); }

    /** No descriptions */
    int    allocate(int capacity);
    /** No descriptions */
    int    deallocate();
    void   set(int fd, int index)
    {
        if (fd >= m_iCapacity)
            grow(fd);
        //assert( m_pIntArray[fd] == -1 );
        m_pIntArray[fd] = index;
    }
    int  capacity() const   {   return m_iCapacity; }
    int     get(int fd)
    {    return m_pIntArray[fd];  }
    /** No descriptions */
    int     grow(int fd);
    void    remove(int fd)
    {
        m_pIntArray[fd] = -1;
    }
    LS_NO_COPY_ASSIGN(LookUpFD);
};

#endif
