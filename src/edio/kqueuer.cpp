/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#if defined(__FreeBSD__ ) || defined(__NetBSD__) || defined(__OpenBSD__) \
    || defined(macintosh) || defined(__APPLE__) || defined(__APPLE_CC__)

#include "kqueuer.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/event.h>
#include <unistd.h>

static int s_fdKQ;

KQueuer::KQueuer()
    : m_fdKQ(-1)
    , m_changeCapacity(0)
    , m_curChange(0)
    , m_pChanges(NULL)
      //, m_traceCounter( 0 )
{
}
KQueuer::~KQueuer()
{
    if (m_fdKQ != -1)
        close(m_fdKQ);
    deallocateChangeBuf();
}

int KQueuer::allocateChangeBuf(int capacity)
{
    struct kevent *pEvents = (struct kevent *) realloc(m_pChanges,
                             capacity * sizeof(struct kevent));
    if (!pEvents)
        return LS_FAIL;
    if (capacity > m_changeCapacity)
        memset(pEvents + m_changeCapacity, 0,
               sizeof(struct kevent) * (capacity - m_changeCapacity));
    m_pChanges = pEvents;
    m_changeCapacity = capacity;
    return LS_OK;
}

int KQueuer::deallocateChangeBuf()
{
    if (m_pChanges)
        free(m_pChanges);
    return LS_OK;
}



int KQueuer::init(int capacity)
{
    if (m_reactorIndex.allocate(capacity) == -1)
        return LS_FAIL;
    if (allocateChangeBuf(64) == -1)
        return LS_FAIL;
    m_fdKQ = kqueue();
    if (m_fdKQ == -1)
        return LS_FAIL;
    s_fdKQ = m_fdKQ;
    ::fcntl(m_fdKQ, F_SETFD, FD_CLOEXEC);
    return LS_OK;
}


int KQueuer::appendEvent(EventReactor *pHandler, short filter,
                         unsigned short flags)
{
    return appendEvent(pHandler, pHandler->getfd(), filter, flags);
}



int KQueuer::addEvent(EventReactor *pHandler, short mask)
{
    if (mask & POLLIN)
        appendEvent(pHandler, EVFILT_READ,  EV_ADD | EV_ENABLE);
    if (mask & POLLOUT)
        appendEvent(pHandler, EVFILT_WRITE, EV_ADD | EV_ENABLE);
    pHandler->setMask2(mask);
    return LS_OK;
}

int KQueuer::add(EventReactor *pHandler, short mask)
{
    if (!pHandler)
        return LS_FAIL;
    pHandler->setPollfd();
    m_reactorIndex.set(pHandler->getfd(), pHandler);
    return addEvent(pHandler, mask);
}

int KQueuer::remove(EventReactor *pHandler)
{
    //appendEvent( pHandler, EVFILT_READ,  EV_DELETE );
    //appendEvent( pHandler, EVFILT_WRITE, EV_DELETE );
    while (m_curChange > 0)
    {
        if (m_pChanges[ m_curChange - 1 ].udata == pHandler)
            --m_curChange;
        else
            break;
    }
    pHandler->setMask2(0);
    m_reactorIndex.set(pHandler->getfd(), NULL);
    return LS_OK;
}

void KQueuer::processAioEvent(struct kevent *pEvent)
{
    //TODO: handle AIO event
}

void KQueuer::processSocketEvent(struct kevent *pEvent)
{
    EventReactor *pReactor = (EventReactor *)pEvent->udata;
    if (pReactor && (pReactor->getfd() == (int)pEvent->ident))
    {
        short revent;
        if (pEvent->flags & EV_ERROR)
        {
            if (pEvent->data != ENOENT)
                fprintf(stderr,
                        "kevent() error, fd: %d, error: %d, filter: %d flags: %04X\n",
                        pReactor->getfd(), (int)pEvent->data, pEvent->filter, pEvent->flags);
            //revent = POLLERR;
            return;
        }
        else
        {
            switch (pEvent->filter)
            {
            case EVFILT_READ:
                if (pEvent->flags & EV_EOF)
                    revent = POLLHUP | POLLIN;
                else if (pReactor->getEvents() & POLLIN)
                    revent = POLLIN;
                else
                {
                    appendEvent(pReactor, pReactor->getfd(), EVFILT_READ, EV_DELETE);
                    return;
                }
                break;
            case EVFILT_WRITE:
                if (pReactor->getEvents() & POLLOUT)
                    revent = POLLOUT;
                else
                {
                    appendEvent(pReactor, pReactor->getfd(), EVFILT_WRITE, EV_DELETE);
                    return;
                }
                break;
            default:
                fprintf(stderr, "Kqueue: unkown event, ident: %d, "
                        " filter: %hd, flags: %hd, fflags: %d, data: %d,"
                        " udata: %p\n", (int)pEvent->ident,
                        pEvent->filter, pEvent->flags, pEvent->fflags,
                        (int)pEvent->data, pEvent->udata);
                appendEvent(NULL, (int)pEvent->ident, pEvent->filter, EV_DELETE);
                return;
            }
        }
        pReactor->assignRevent(revent);
        pReactor->handleEvents(revent);
    }
    else
    {
//                fprintf( stderr, "Kqueue: mystery event, ident: %d, "
//                                 " filter: %hd, flags: %hd, fflags: %d, data: %d,"
//                                 " udata: %p, reactor fd: %d\n", (int)pEvent->ident,
//                                 pEvent->filter, pEvent->flags, pEvent->fflags,
//                                 (int)pEvent->data, pEvent->udata,
//                                 (pReactor)?pReactor->getfd():-1 );
        //wil get this if modify event after socket closed, new socket created with the same file descriptor
        if (pEvent->flags & EV_ERROR)
        {
            fprintf(stderr,
                    "kevent(), mismatch handler, fd: %d, error: %d, filter: %d flags: %04X\n",
                    pReactor->getfd(), (int)pEvent->data, pEvent->filter, pEvent->flags);
            //if ( (int)pEvent->data != EBADF )
            //    close( (int)pEvent->ident );
        }
        else if (!pReactor)
            appendEvent(NULL, (int)pEvent->ident, pEvent->filter, EV_DELETE);
//                if ( pEvent->filter == EVFILT_READ )
//                    appendEvent( NULL, (int)pEvent->ident, EVFILT_READ,  EV_DELETE );
//                if ( pEvent->filter == EVFILT_READ )
//                    appendEvent( NULL, (int)pEvent->ident, EVFILT_WRITE, EV_DELETE );
    }

}

int KQueuer::waitAndProcessEvents(int iTimeoutMilliSec)
{
    struct timespec timeout;
    timeout.tv_sec  = iTimeoutMilliSec / 1000;
    timeout.tv_nsec = (iTimeoutMilliSec % 1000) * 1000000;
    int ret;
    ret = kevent(m_fdKQ, m_pChanges, m_curChange, m_results, 16, &timeout);

    if ((ret == -1) && ((errno == EBADF) || (errno == ENOENT)))
    {
        if (m_curChange > 0)
        {
            memset(&timeout, 0, sizeof(timeout));
            for (int i = 0; i < m_curChange; ++i)
            {
                ret = kevent(m_fdKQ, m_pChanges + i, 1, m_results, 0, &timeout);
                if ((ret == -1) && ((errno == EBADF) || (errno == ENOENT))
                    && (m_pChanges[i].flags != EV_DELETE))
                {
                    ((EventReactor *)(m_pChanges[i].udata))->assignRevent(POLLERR);
                    ((EventReactor *)(m_pChanges[i].udata))->handleEvents(POLLERR);
                }
            }
        }
        memset(m_pChanges, 0, m_curChange * sizeof(struct kevent));
        m_curChange = 0;
        ret = 0;
    }

    if (m_curChange > 0)
    {
//        if ( m_curChange == 1 )
//        {
//            if ( memcmp( &m_trace, m_pChanges, sizeof( m_trace )) == 0 )
//            {
//                ++m_traceCounter;
//                if ( m_traceCounter >= 10 )
//                {
//                    fprintf( stderr, "Kqueue problem: loop event, ident: %d, "
//                                     " filter: %hd, flags: %hd, fflags: %d, data: %d,"
//                                     " udata: %p\n", (int)m_pChanges->ident,
//                                     m_pChanges->filter, m_pChanges->flags, m_pChanges->fflags,
//                                     (int)m_pChanges->data, m_pChanges->udata );
//                    fprintf( stderr, "Kqueue problem: return event, ident: %d, "
//                                     " filter: %hd, flags: %hd, fflags: %d, data: %d,"
//                                     " udata: %p\n", (int)results[0].ident,
//                                     results[0].filter, results[0].flags, results[0].fflags,
//                                     (int)results[0].data, results[0].udata );
//
//                    m_traceCounter = 0;
//                }
//            }
//            else
//            {
//                memmove( &m_trace, m_pChanges, sizeof( m_trace ) );
//                m_traceCounter = 0;
//            }
//        }
//        else
//            m_traceCounter = 0;
        memset(m_pChanges, 0, m_curChange * sizeof(struct kevent));
        m_curChange = 0;
    }
//    else
//        m_traceCounter = 0;
    if (ret > 0)
    {
        m_pResCur = m_results;
        m_pResEnd = &m_results[ret];
        processEvents();
    }
    return ret;
}

int KQueuer::processEvents()
{
    struct kevent *p;
    int ret = m_pResEnd - m_pResCur;
    if (ret <= 0)
        return LS_OK;
    while (m_pResCur < m_pResEnd)
    {
        p = m_pResCur++;
        if (p->filter == EVFILT_AIO)
            processAioEvent(p);
        else
            processSocketEvent(p);
    }
    return ret;
}

void KQueuer::timerExecute()
{
    m_reactorIndex.timerExec();
}

void KQueuer::setPriHandler(EventReactor::pri_handler handler)
{
}

void KQueuer::continueRead(EventReactor *pHandler)
{
    if (!(pHandler->getEvents() & POLLIN))
    {
        appendEvent(pHandler, EVFILT_READ,  EV_ADD | EV_ENABLE);
        pHandler->orMask2(POLLIN);
    }
}

void KQueuer::suspendRead(EventReactor *pHandler)
{
    if (pHandler->getEvents() & POLLIN)
    {
        appendEvent(pHandler, EVFILT_READ,  EV_DELETE);
        pHandler->andMask2(~POLLIN);
    }
}

void KQueuer::continueWrite(EventReactor *pHandler)
{
    if (!(pHandler->getEvents() & POLLOUT))
    {
        appendEvent(pHandler, EVFILT_WRITE,  EV_ADD | EV_ENABLE);
        pHandler->orMask2(POLLOUT);
    }
}

void KQueuer::suspendWrite(EventReactor *pHandler)
{
    if (pHandler->getEvents() & POLLOUT)
    {
        appendEvent(pHandler, EVFILT_WRITE,  EV_DELETE);
        pHandler->andMask2(~POLLOUT);
    }
}

void KQueuer::switchWriteToRead(EventReactor *pHandler)
{
    suspendWrite(pHandler);
    continueRead(pHandler);
}

void KQueuer::switchReadToWrite(EventReactor *pHandler)
{
    suspendRead(pHandler);
    continueWrite(pHandler);
}

int KQueuer::getFdKQ()
{   return s_fdKQ; }



#endif //defined(__FreeBSD__ ) || defined(__NetBSD__) || defined(__OpenBSD__) 
//|| defined(macintosh) || defined(__APPLE__) || defined(__APPLE_CC__)

