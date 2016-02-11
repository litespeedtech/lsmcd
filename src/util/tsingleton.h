/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef TSINGLETON_H
#define TSINGLETON_H


//#ifdef _REENTRENT
//#endif
#ifdef _REENTRENT
#include <thread/tmutex.h>
#endif

#include <assert.h>
#include <stdlib.h>

template < class T >
class TSingleton
{
private:
    TSingleton(const TSingleton &rhs);
    void operator=(const TSingleton &rhs);
protected:
    TSingleton() {};
    ~TSingleton() {};
public:
    static T &getInstance()
    {
#ifdef LAZY_CREATE
        static T *s_pInstance = NULL;
        if (s_pInstance == NULL)
        {
#ifdef _REENTRENT
            {
                static Mutex mutex;
                LockMutex lock(mutex);
                if (s_pInstance == NULL)
                    s_pInstance = new T();
            }
#else
            s_pInstance = new T();
#endif
            assert(s_pInstance != NULL);
        }
        return *s_pInstance;
#else  //LAZY_CREATE
        static T  s_instance;
        return s_instance;
#endif //LAZY_CREATE
    }

};


#endif
