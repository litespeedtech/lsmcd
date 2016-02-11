/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef ENV_H
#define ENV_H


/**
  *@author George Wang
  */
#include <util/ienv.h>
#include <util/gpointerlist.h>

class ArrayOfPointer : public TPointerList<char>
{
public:
    ArrayOfPointer() {}
    ArrayOfPointer(int initSize): TPointerList<char>(initSize) {}
    ~ArrayOfPointer()
    {   clear();    }
    void clear();
};

class Env : public IEnv, public ArrayOfPointer
{
public:
    Env() {};
    Env(int initSize): ArrayOfPointer(initSize) {}
    ~Env() {};
    int add(const char *name, const char *value)
    {   return IEnv::add(name, value); }
    int add(const char *name, size_t nameLen,
            const char *value, size_t valLen);
    int add(const char *pEnv);
    int add(const Env *pEnv);
    const char *find(const char *name) const ;
    void clear()    { ArrayOfPointer::clear();  }
    char *const *get() const
    {   return &((*this)[0]);   }
    char **get()
    {   return &((*this)[0]);   }
    int update(const char *name, const char *value);
    void remove(const char *name);
};


#endif
