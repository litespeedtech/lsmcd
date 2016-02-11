/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include "multiplexerfactory.h"

#include <edio/devpoller.h>
#include <edio/epoll.h>
#include <edio/kqueuer.h>
#include <edio/poller.h>
#include <edio/rtsigio.h>

#include <stdlib.h>
#include <string.h>

int MultiplexerFactory::s_iMaxFds = 4096;
Multiplexer *MultiplexerFactory::s_pMultiplexer = NULL;

static const char *s_sType[MultiplexerFactory::BEST + 1] =
{
    "poll",
    "select",
    "devpoll",
    "kqueue",
    "rtsig",
    "epoll",
    "best"
};

int MultiplexerFactory::getType(const char *pType)
{
    int i;
    if (!pType)
        return POLL;
    for (i = 0; i < BEST + 1; ++i)
    {
        if (strcasecmp(pType, s_sType[i]) == 0)
            break;
    }
    if (i == BEST)
    {
#if defined(linux) || defined(__linux) || defined(__linux__) || defined(__gnu_linux__)
        i = EPOLL;
#endif

#if defined(sun) || defined(__sun)
        i = DEV_POLL;
#endif

#if defined(__FreeBSD__ ) || defined(__NetBSD__) || defined(__OpenBSD__) \
    || defined(macintosh) || defined(__APPLE__) || defined(__APPLE_CC__)
        i = KQUEUE;
#endif
    }
    return i;
}


Multiplexer *MultiplexerFactory::create(int type)
{
    switch (type)
    {
#if defined(linux) || defined(__linux) || defined(__linux__) || defined(__gnu_linux__)
#if 0
    case RT_SIG:
        return new RTsigio();
#endif

    case BEST:
    case EPOLL:
        return new epoll();
#endif

#if defined(sun) || defined(__sun)
    case BEST:
    case DEV_POLL:
        return new DevPoller();
#endif

#if defined(__FreeBSD__ ) || defined(__NetBSD__) || defined(__OpenBSD__) \
    || defined(macintosh) || defined(__APPLE__) || defined(__APPLE_CC__)
    case BEST:
    case KQUEUE:
        return new KQueuer();
#endif
    case POLL:
        return new Poller();
    default:
        return NULL;
    }
}

void MultiplexerFactory::recycle(Multiplexer *ptr)
{
    if (ptr)
        delete ptr;
}
