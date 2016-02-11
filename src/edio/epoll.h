/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef EPOLL_H
#define EPOLL_H

#if defined(linux) || defined(__linux) || defined(__linux__) || defined(__gnu_linux__)

#include <edio/multiplexer.h>
#include <edio/reactorindex.h>
/**
  *@author George Wang
  */
struct epoll_event;

class epoll : public Multiplexer
{
    int                  m_epfd;
    struct epoll_event *m_pResults;
    struct epoll_event *m_pResEnd;
    struct epoll_event *m_pResCur;
    ReactorIndex         m_reactorIndex;
    int updateEvents(EventReactor *pHandler, short mask);

    void addEvent(EventReactor *pHandler, short mask)
    {
        pHandler->orMask2(mask);
        updateEvents(pHandler, pHandler->getEvents());
    }
    void removeEvent(EventReactor *pHandler, short mask)
    {
        pHandler->andMask2(~mask);
        updateEvents(pHandler, pHandler->getEvents());
    }
    void setEvents(EventReactor *pHandler, short mask)
    {
        if (pHandler->getEvents() != mask)
        {
            pHandler->setMask2(mask);
            updateEvents(pHandler, mask);
        }
    }
    int reinit();

public:
    epoll();
    ~epoll();
    virtual int getHandle() const   {   return m_epfd;  }
    virtual int init(int capacity = DEFAULT_CAPACITY);
    virtual int add(EventReactor *pHandler, short mask);
    virtual int remove(EventReactor *pHandler);
    virtual int waitAndProcessEvents(int iTimeoutMilliSec);
    virtual void timerExecute();
    virtual void setPriHandler(EventReactor::pri_handler handler) {};

    virtual int processEvents();

    virtual void continueRead(EventReactor *pHandler);
    virtual void suspendRead(EventReactor *pHandler);
    virtual void continueWrite(EventReactor *pHandler);
    virtual void suspendWrite(EventReactor *pHandler);
    virtual void switchWriteToRead(EventReactor *pHandler);
    virtual void switchReadToWrite(EventReactor *pHandler);


};


#endif

#endif
