/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */

#if 0
#if defined(linux) || defined(__linux) || defined(__linux__) || defined(__gnu_linux__)

#ifndef RTSIGIO_H
#define RTSIGIO_H


#include <edio/fdindex.h>
#include <edio/poller.h>
#include <signal.h>

/**
  *@author George Wang
  */
class RTSigData;
class RTsigio : public Poller
{
    sigset_t        m_sigset;
    FdIndex         m_fdindex;
    int allocate(int iInitCapacity);
    int deallocate();
    int enableSigio();

public:

    RTsigio();
    virtual ~RTsigio();
    virtual int init(int capacity);
    virtual int add(EventReactor *pHandler, short mask);
    virtual int remove(EventReactor *pHandler);
    virtual int waitAndProcessEvents(int iTimeoutMilliSec);

};

#endif

#endif

#endif
