/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef DEVPOLLER_H
#define DEVPOLLER_H

#if defined(sun) || defined(__sun)

#include <edio/multiplexer.h>
#include <edio/reactorindex.h>
#include <sys/poll.h>
/**
  *@author George Wang
  */

#define MAX_CHANGES 40
#define MAX_EVENTS  20
class DevPoller : public Multiplexer
{
    int             m_fdDP;
    ReactorIndex    m_reactorIndex;

    int             m_curChanges;
    struct pollfd   m_events[MAX_EVENTS];
    struct pollfd   m_changes[MAX_CHANGES];
    struct pollfd *m_pEventCur;
    struct pollfd *m_pEventEnd;

    int applyChanges();

    int appendChange(int fd, short mask)
    {
        if (m_curChanges >= MAX_CHANGES)
            if (applyChanges() == -1)
                return -1;
        m_changes[m_curChanges].fd       = fd;
        m_changes[m_curChanges++].events = mask;
        return 0;
    }
    void setEvent(EventReactor *pHandler, short mask)
    {
        if (mask != pHandler->getEvents())
        {
            appendChange(pHandler->getfd(), POLLREMOVE);
            appendChange(pHandler->getfd(), mask);
            pHandler->setMask2(mask);
        }
    }

    void addEvent(EventReactor *pHandler, short mask)
    {
        if ((mask & pHandler->getEvents()) != mask)
        {
            pHandler->orMask2(mask);
            appendChange(pHandler->getfd(), pHandler->getEvents());
        }
    }

    void removeEvent(EventReactor *pHandler, short mask)
    {
        if (mask & pHandler->getEvents())
        {
            appendChange(pHandler->getfd(), POLLREMOVE);
            pHandler->andMask2(~mask);
            appendChange(pHandler->getfd(), pHandler->getEvents());
        }
    }

public:
    DevPoller();
    ~DevPoller();

    virtual int getHandle() const   {   return m_fdDP;  }
    virtual int init(int capacity = DEFAULT_CAPACITY);
    virtual int add(EventReactor *pHandler, short mask);
    virtual int remove(EventReactor *pHandler);
    virtual int waitAndProcessEvents(int iTimeoutMilliSec);
    virtual void timerExecute();
    virtual void setPriHandler(EventReactor::pri_handler handler);
    virtual int processEvents();

    virtual void continueRead(EventReactor *pHandler);
    virtual void suspendRead(EventReactor *pHandler);
    virtual void continueWrite(EventReactor *pHandler);
    virtual void suspendWrite(EventReactor *pHandler);

    virtual void switchWriteToRead(EventReactor *pHandler)
    {   setEvent(pHandler, POLLIN | POLLERR | POLLHUP);   }

    virtual void switchReadToWrite(EventReactor *pHandler)
    {   setEvent(pHandler, POLLOUT | POLLERR | POLLHUP);  }

};

#endif //defined(sun) || defined(__sun)

#endif
