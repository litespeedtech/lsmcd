/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef _IENV_H_
#define _IENV_H_
#include <string.h>
class IEnv
{
public:
    IEnv() {}
    virtual ~IEnv() {}
    int add(const char *name, const char *value)
    {   return add(name, strlen(name), value, strlen(value)); }
    virtual int add(const char *name, size_t nameLen,
                    const char *value, size_t valLen) = 0;
    virtual void clear() = 0;
};

#define ADD_ENV(a, x, y) if (a->add(x,y) == -1) return -1
#define ADD_ENVL(a, x,xl, y, yl) a->add(x,xl, y, yl);

#endif
