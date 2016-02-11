/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef GNEW_H
#define GNEW_H


#include <stddef.h>

extern "C++"
{

    extern void *operator new(size_t sz);
    extern void operator delete(void *p);

};
#endif
