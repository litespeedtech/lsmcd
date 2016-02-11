/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef MUTLIPLEXERFACTORY_H
#define MUTLIPLEXERFACTORY_H


/**
  *@author George Wang
  */
#include <util/tsingleton.h>

class Multiplexer;
class MultiplexerFactory
{
    friend class TSingleton< MultiplexerFactory >;
    MultiplexerFactory();
    ~MultiplexerFactory();

    static int s_iMaxFds;
    static Multiplexer           *s_pMultiplexer;

public:
    enum
    {
        POLL,
        SELECT,
        DEV_POLL,
        KQUEUE,
        RT_SIG,
        EPOLL,
        BEST
    };
    static int getType(const char *pType);
    static Multiplexer *create(int type);
    static void recycle(Multiplexer *ptr);
    static Multiplexer *getMultiplexer()
    {   return s_pMultiplexer;      }
    static void setMultiplexer(Multiplexer *pMultiplexer)
    {   s_pMultiplexer = pMultiplexer;  }
};

#endif
