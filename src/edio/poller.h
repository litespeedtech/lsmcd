/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef POLLER_H
#define POLLER_H

#include <edio/multiplexer.h>
#include <edio/pollfdreactor.h>
/**
  *@author George Wang
  */
class Poller : public Multiplexer
{
    PollfdReactor  m_pfdReactors;

    int allocate(int iInitCapacity);
    int deallocate();
protected:
    PollfdReactor &getPfdReactor() {    return m_pfdReactors;   }

public:
    Poller();
    virtual ~Poller();
    virtual int init(int capacity);
    virtual int add(EventReactor *pHandler, short mask);
    virtual int remove(EventReactor *pHandler);
    virtual int waitAndProcessEvents(int iTimeoutMilliSec);
    virtual void timerExecute();
    virtual void setPriHandler(EventReactor::pri_handler handler)
    {   m_pfdReactors.setPriHandler(handler);     }
    virtual int processEvents()
    {   return m_pfdReactors.processEvents();   }
};

#endif
