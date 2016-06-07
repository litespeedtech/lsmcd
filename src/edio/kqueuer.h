/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef KQUEUER_H
#define KQUEUER_H

#if defined(__FreeBSD__ ) || defined(__NetBSD__) || defined(__OpenBSD__) \
    || defined(macintosh) || defined(__APPLE__) || defined(__APPLE_CC__)


#include <multiplexer.h>
#include <reactorindex.h>

#include <sys/types.h>
#include <sys/event.h>
/**
  *@author George Wang
  */

struct kevent;
class KQueuer : public Multiplexer
{
    int m_fdKQ;
    ReactorIndex    m_reactorIndex;
    int m_changeCapacity;
    int m_curChange;
    struct kevent *m_pChanges;
    struct kevent   m_results[16];
    struct kevent *m_pResCur;
    struct kevent *m_pResEnd;

    //struct kevent   m_trace;
    //int             m_traceCounter;

    int allocateChangeBuf(int capacity);
    int deallocateChangeBuf();
    int appendEvent(EventReactor *pHandler, short filter, unsigned short flag);
    int addEvent(EventReactor *pHandler, short mask);
    int appendEvent(EventReactor *pHandler, int fd, short filter,
                    unsigned short flags)
    {
        if (fd == -1)
            return LS_OK;
        if (m_curChange == m_changeCapacity)
        {
            if (allocateChangeBuf(m_changeCapacity + 64) == -1)
                return LS_FAIL;
        }
        struct kevent *pEvent = m_pChanges + m_curChange++;
        pEvent->ident  = fd;
        pEvent->filter = filter;
        pEvent->flags  = flags;
        pEvent->udata  = pHandler;
        return LS_OK;
    }
    void processAioEvent(struct kevent *pEvent);
    void processSocketEvent(struct kevent *pEvent);
    int  processEvents();

public:
    KQueuer();
    ~KQueuer();
    virtual int getHandle() const   {   return m_fdKQ;  }
    virtual int init(int capacity = DEFAULT_CAPACITY);
    virtual int add(EventReactor *pHandler, short mask);
    virtual int remove(EventReactor *pHandler);
    virtual int waitAndProcessEvents(int iTimeoutMilliSec);
    virtual void timerExecute();
    virtual void setPriHandler(EventReactor::pri_handler handler);

    virtual void continueRead(EventReactor *pHandler);
    virtual void suspendRead(EventReactor *pHandler);
    virtual void continueWrite(EventReactor *pHandler);
    virtual void suspendWrite(EventReactor *pHandler);
    virtual void switchWriteToRead(EventReactor *pHandler);
    virtual void switchReadToWrite(EventReactor *pHandler);

    static int getFdKQ();
};

#endif //defined(__FreeBSD__ ) || defined(__NetBSD__) || defined(__OpenBSD__) 
//|| defined(macintosh) || defined(__APPLE__) || defined(__APPLE_CC__)


#endif
