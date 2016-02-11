#ifndef TOBJARRAY_H
#define TOBJARRAY_H

#include <stdlib.h>
#include <string.h>

template< typename T >
class TObjArray
{

public:
    TObjArray()
        : m_capacity(0)
        , m_size(0)
        , m_pArray(NULL)
    {}

    ~TObjArray()
    {
        if (m_pArray)
            free(m_pArray);
    }

    int alloc(int newMax)
    {
        if (newMax <= m_capacity)
            return 0;
        T *pNewBuf = (T *)realloc(m_pArray,
                                  sizeof(T) * newMax);
        if (pNewBuf)
        {
            m_pArray = (char *)pNewBuf;
            m_capacity = newMax;
        }
        else
            return -1;
        return 0;
    }

    int size() const        {   return m_size;      }
    int capacity() const    {   return m_capacity;  }

    T *get(int index)
    {
        if (index < m_size)
            return ((T *)m_pArray) + index;
        else
            return NULL;
    }

    T *newObj()
    {
        if (m_size >= m_capacity)
        {
            if (alloc(m_capacity ? (m_capacity << 1) : 4) == -1)
                return NULL;
        }
        return ((T *)m_pArray) + m_size++ ;
    }

    void clear()    {   m_size = 0;     }

    T *begin()      {   return (T *)m_pArray;    }
    T *end()        {   return (T *)m_pArray + m_size;   }

    void copy(const TObjArray &other)
    {
        alloc(other.capacity());
        m_size = other.m_size;
        memmove(m_pArray, other.m_pArray, m_size * sizeof(T));
    }

private:

    int     m_capacity;
    int     m_size;
    char   *m_pArray;

    TObjArray(const TObjArray &other);
    TObjArray &operator=(const TObjArray &other);

};

#endif // TOBJARRAY_H
