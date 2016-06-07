/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include "lookupfd.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>


/** No descriptions */
int LookUpFD::allocate(int capacity)
{
    // Resize arrays indexed by fd if fd is beyond what we've seen.
    int *pn = (int *)realloc(m_pIntArray, capacity * sizeof(int));
    if (!pn)
        return ENOMEM;
    // Clear new elements
    //for (int i=m_iCapacity; i<n; i++)
    //  pn[i] = -1;
    memset(pn + m_iCapacity, -1,
           sizeof(int) * (capacity - m_iCapacity));
    m_pIntArray = pn;

    m_iCapacity = capacity;
    return LS_OK;

}
/** No descriptions */
int LookUpFD::deallocate()
{
    if (m_pIntArray)
    {
        free(m_pIntArray);
        m_pIntArray = NULL;
        m_iCapacity = 0;
    }
    return LS_OK;
}
/** No descriptions */
int LookUpFD::grow(int fd)
{
    // Resize arrays indexed by fd if fd is beyond what we've seen.
    int n = m_iCapacity * 2;
    if (n < fd + 10)
        n = fd + 10;
    return allocate(n);
}

