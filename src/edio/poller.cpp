/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include "poller.h"

#include "eventreactor.h"
#include "lookupfd.h"
#include "pollfdreactor.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>


Poller::Poller()
    : m_pfdReactors()
{
}


Poller::~Poller()
{
}


int Poller::allocate(int capacity)
{
    return m_pfdReactors.allocate(capacity);
}


int Poller::init(int capacity)
{
    return allocate(capacity);
}


int Poller::add(EventReactor *pHandler, short mask)
{
    assert(pHandler);
    m_pfdReactors.add(pHandler, mask);
    return LS_OK;
}


int Poller::remove(EventReactor *pHandler)
{
    assert(pHandler);
    m_pfdReactors.remove(pHandler);
    return LS_OK;
}


int Poller::waitAndProcessEvents(int iTimeoutMilliSec)
{
    int events = ::poll(m_pfdReactors.getPollfd(),
                        m_pfdReactors.getSize(),
                        iTimeoutMilliSec);
    if (events > 0)
    {
        m_pfdReactors.setEvents(events);
        m_pfdReactors.processAllEvents();
        return LS_OK;
    }
    return events;
}


void Poller::timerExecute()
{
    m_pfdReactors.timerExecute();
}


