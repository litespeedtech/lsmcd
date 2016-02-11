/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#if defined(linux) || defined(__linux) || defined(__linux__) || defined(__gnu_linux__)

#include "epoll.h"

#include "log4cxx/logger.h"

#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <sys/types.h>

#if 0
#ifndef __NR_epoll_create

#define __NR_epoll_create   (254)
#define __NR_epoll_ctl      (255)
#define __NR_epoll_wait     (256)

#endif

#ifndef EPOLL_EVENTS

enum EPOLL_EVENTS
{
    EPOLLIN = 0x001,
#define EPOLLIN EPOLLIN

    EPOLLPRI = 0x002,
#define EPOLLPRI EPOLLPRI

    EPOLLOUT = 0x004,
#define EPOLLOUT EPOLLOUT

#ifdef __USE_XOPEN

    EPOLLRDNORM = 0x040,
#define EPOLLRDNORM EPOLLRDNORM

    EPOLLRDBAND = 0x080,
#define EPOLLRDBAND EPOLLRDBAND

    EPOLLWRNORM = 0x100,
#define EPOLLWRNORM EPOLLWRNORM

    EPOLLWRBAND = 0x200,
#define EPOLLWRBAND EPOLLWRBAND

#endif /* #ifdef __USE_XOPEN */

#ifdef __USE_GNU
    EPOLLMSG = 0x400,
#define EPOLLMSG EPOLLMSG
#endif /* #ifdef __USE_GNU */

    EPOLLERR = 0x008,
#define EPOLLERR EPOLLERR

    EPOLLHUP = 0x010
#define EPOLLHUP EPOLLHUP

};

/* Valid opcodes ( "op" parameter ) to issue to epoll_ctl() */
#define EPOLL_CTL_ADD 1 /* Add a file decriptor to the interface */
#define EPOLL_CTL_DEL 2 /* Remove a file decriptor from the interface */
#define EPOLL_CTL_MOD 3 /* Change file decriptor epoll_event structure */
#ifdef HAVE_EPOLL_CTLV
#define EPOLL_CTLV    4 /* Treat this as an epoll_ctlv call */
/* In this case the data.u32 field contains the op */
#endif /* HAVE_EPOLL_CTLV */

typedef union epoll_data
{
    void *ptr;
    int fd;
    __uint32_t u32;
    __uint64_t u64;
} epoll_data_t;

struct epoll_event
{
    __uint32_t events; /* Epoll events */
    epoll_data_t data; /* User data variable */
};


#ifdef HAVE_EPOLL_CTLV
struct epoll_event_ctlv
{
    int op;
    int fd;
    long res;
    struct epoll_event event;
} EPOLL_PACKED;
#endif /* HAVE_EPOLL_CTLV */

#endif

#endif

static int s_loop_fd = -1;
static int s_loop_count = 0;
static int s_problems = 0;
epoll::epoll()
    : m_epfd(-1)
    , m_pResults(NULL)
{
    setFLTag(O_NONBLOCK | O_RDWR);

}

epoll::~epoll()
{
    if (m_epfd != -1)
        close(m_epfd);
    if (m_pResults)
        free(m_pResults);
}

#define EPOLL_RESULT_BUF_SIZE   4096
//#define EPOLL_RESULT_MAX        EPOLL_RESULT_BUF_SIZE / sizeof( struct epoll_event )
#define EPOLL_RESULT_MAX        10
int epoll::init(int capacity)
{
    if (m_reactorIndex.allocate(capacity) == -1)
        return -1;
    if (!m_pResults)
    {
        m_pResults = (struct epoll_event *)malloc(EPOLL_RESULT_BUF_SIZE);
        if (!m_pResults)
            return -1;
        memset(m_pResults, 0, EPOLL_RESULT_BUF_SIZE);
    }
    if (m_epfd != -1)
        close(m_epfd);
    //m_epfd = (syscall(__NR_epoll_create, capacity ));
    m_epfd = epoll_create(capacity);
    if (m_epfd == -1)
        return -1;
    ::fcntl(m_epfd, F_SETFD, FD_CLOEXEC);
    return 0;
}

int epoll::reinit()
{
    struct epoll_event epevt;
    close(m_epfd);
    m_epfd = (syscall(__NR_epoll_create, m_reactorIndex.getCapacity()));
    if (m_epfd == -1)
        return -1;
    epevt.data.u64 = 0;
    ::fcntl(m_epfd, F_SETFD, FD_CLOEXEC);
    int n = m_reactorIndex.getUsed();
    for (int i = 0; i < n; ++i)
    {
        EventReactor *pHandler = m_reactorIndex.get(i);
        if (pHandler)
        {
            epevt.data.u64 = 0;
            epevt.data.fd = pHandler->getfd();
            epevt.events = pHandler->getEvents();
            //(syscall(__NR_epoll_ctl, m_epfd, EPOLL_CTL_ADD, epevt.data.fd, &epevt));
            epoll_ctl(m_epfd, EPOLL_CTL_ADD, epevt.data.fd, &epevt);
        }
    }
    return 0;
}
/*
#include <typeinfo>
void dump_type_info( EventReactor * pHandler, const char * pMsg )
{
    LS_INFO( "[%d] %s EventReactor: %p, fd: %d, type: %s", getpid(), pMsg, pHandler, pHandler->getfd(),
                typeid( *pHandler ).name() ));
}
*/
int epoll::add(EventReactor *pHandler, short mask)
{
    struct epoll_event epevt;
    int fd = pHandler->getfd();
    if (fd == -1)
        return 0;
    //assert( fd > 1 );
    if (fd > 100000)
        return -1;
    memset(&epevt, 0, sizeof(struct epoll_event));
    m_reactorIndex.set(fd, pHandler);
    pHandler->setPollfd();
    pHandler->setMask2(mask);
    pHandler->clearRevent();
    epevt.data.u64 = 0;
    epevt.data.fd = fd;
    epevt.events = mask;
    //if (LS_LOG_ENABLED(LOG4CXX_NS::Level::DBG_LESS) || s_problems )
    //    dump_type_info( pHandler, "added" );
    //return (syscall(__NR_epoll_ctl, m_epfd, EPOLL_CTL_ADD, fd, &epevt));
    return epoll_ctl(m_epfd, EPOLL_CTL_ADD, fd, &epevt);
}

int epoll::updateEvents(EventReactor *pHandler, short mask)
{
    struct epoll_event epevt;
    int fd = pHandler->getfd();
    if (fd == -1)
        return 0;
    assert(pHandler == m_reactorIndex.get(fd));
    pHandler->setMask2(mask);
    //assert( fd > 1 );
    memset(&epevt, 0, sizeof(struct epoll_event));
    epevt.data.u64 = 0;
    epevt.data.fd = fd;
    epevt.events = mask;
    //return (syscall(__NR_epoll_ctl, m_epfd, EPOLL_CTL_MOD, fd, &epevt));
    return epoll_ctl(m_epfd, EPOLL_CTL_MOD, fd, &epevt);
}

int epoll::remove(EventReactor *pHandler)
{
    struct epoll_event epevt;
    int fd = pHandler->getfd();
    if (fd == -1)
        return 0;
    //assert( pHandler == m_reactorIndex.get( fd ) );
    pHandler->clearRevent();
    //assert( fd > 1 );
    memset(&epevt, 0, sizeof(struct epoll_event));

    epevt.data.u64 = 0;
    epevt.data.fd = fd;
    epevt.events = 0;

    if (fd <= (int)m_reactorIndex.getUsed())
        m_reactorIndex.set(fd, NULL);
    //if (LS_LOG_ENABLED(LOG4CXX_NS::Level::DBG_LESS) || s_problems )
    //    dump_type_info( pHandler, "remove" );
    //return (syscall(__NR_epoll_ctl, m_epfd, EPOLL_CTL_DEL, fd, &epevt ));
    return epoll_ctl(m_epfd, EPOLL_CTL_DEL, fd, &epevt);
}

int epoll::waitAndProcessEvents(int iTimeoutMilliSec)
{
    //int ret = (syscall(__NR_epoll_wait, m_epfd, m_pResults, EPOLL_RESULT_MAX, iTimeoutMilliSec ));
    int ret = epoll_wait(m_epfd, m_pResults, EPOLL_RESULT_MAX,
                         iTimeoutMilliSec);
    if (ret <= 0)
        return ret;
    if (ret == 1)
    {
        int fd = m_pResults->data.fd;
        EventReactor *pReactor = m_reactorIndex.get(fd);
        if (pReactor && (pReactor->getfd() == fd))
        {
            if (m_pResults->events & POLLHUP)
                pReactor->incHupCounter();
            pReactor->assignRevent(m_pResults->events);
            pReactor->handleEvents(m_pResults->events);
        }
        return 1;
    }
    //if ( ret > EPOLL_RESULT_MAX )
    //    ret = EPOLL_RESULT_MAX;
    int    problem_detected = 0;
    m_pResEnd = m_pResults + ret;
    m_pResCur = m_pResults;
    struct epoll_event *p = m_pResults;
    while (p < m_pResEnd)
    {
        int fd = p->data.fd;
        EventReactor *pReactor = m_reactorIndex.get(fd);
        assert(p->events);
        if (pReactor)
        {
            if (pReactor->getfd() == fd)
                pReactor->assignRevent(p->events);
            else
                p->data.fd = -1;
        }
        else
        {
            //p->data.fd = -1;
            if ((s_loop_fd == -1) || (s_loop_fd == fd))
            {
                if (s_loop_fd == -1)
                {
                    s_loop_fd = fd;
                    s_loop_count = 0;
                }
                problem_detected = 1;
                ++s_loop_count;
                if (s_loop_count == 10)
                {
                    if (p->events & (POLLHUP | POLLERR))
                        close(fd);
                    else
                    {
                        struct epoll_event epevt;
                        memset(&epevt, 0, sizeof(struct epoll_event));
                        epevt.data.u64 = 0;
                        epevt.data.fd = fd;
                        epevt.events = 0;
                        (syscall(__NR_epoll_ctl, m_epfd, EPOLL_CTL_DEL, fd, &epevt));
                        LS_WARN("[%d] Remove looping fd: %d, event: %d\n", getpid(), fd,
                                p->events);
                        ++s_problems;
                    }
                }
                else if (s_loop_count >= 20)
                {
                    LS_WARN("Looping fd: %d, event: %d\n", fd, p->events);
                    assert(p->events);
                    problem_detected = 0;
                }
            }
        }
        ++p;
    }
    if (!problem_detected && s_loop_count)
    {
        s_loop_fd = -1;
        s_loop_count = 0;
    }
    return processEvents();
}

int epoll::processEvents()
{
    struct epoll_event *p;
    int count = m_pResCur >= m_pResEnd;
    if (count > 0)
        return 0;
    while (m_pResCur < m_pResEnd)
    {
        p = m_pResCur++;
        int fd = p->data.fd;
        if (fd != -1)
        {
            EventReactor *pReactor = m_reactorIndex.get(fd);
            if (pReactor && (pReactor->getAssignedRevent() == (int)p->events))
            {
                if (p->events & POLLHUP)
                    pReactor->incHupCounter();
                pReactor->handleEvents(p->events);
            }
        }
    }

    memset(m_pResults, 0, (char *)m_pResEnd - (char *)m_pResults);
    return count;

}

/*
int epoll::waitAndProcessEvents( int iTimeoutMilliSec )
{
    int ret = (syscall(__NR_epoll_wait, m_epfd, m_pResults, EPOLL_RESULT_MAX, iTimeoutMilliSec ));
    if ( ret > 0 )
    {
        //if ( ret > EPOLL_RESULT_MAX )
        //    ret = EPOLL_RESULT_MAX;
        register int    problem_detected = 1;
        register struct epoll_event * m_pResEnd= m_pResults + ret;
        register struct epoll_event * p = m_pResults;
        while( p < m_pResEnd )
        {
            int fd = p->data.fd;
            EventReactor * pReactor = m_reactorIndex.get( fd );
            if ( pReactor && (pReactor->getfd() == fd ) )
            {
                pReactor->handleEvents( p->events );
                problem_detected = 0;
            }
            else if ( !pReactor )
            {

                if (( s_loop_fd == -1 )||( s_loop_fd == fd ))
                {
                    s_loop_fd = fd;
                    ++s_loop_count;
                    if ( s_loop_count == 10 )
                    {
                        struct epoll_event epevt;
                        memset( &epevt, 0, sizeof( struct epoll_event ) );
                        epevt.data.u64 = 0;
                        epevt.data.fd = fd;
                        epevt.events = 0;
                        (syscall(__NR_epoll_ctl, m_epfd, EPOLL_CTL_DEL, fd, &epevt ));
                        LS_WARN( "Remove looping fd: %d, event: %d\n", fd, p->events ));
                    }
                    else if ( s_loop_count >= 20 )
                    {
                        LS_WARN( "Looping fd: %d, event: %d\n", fd, p->events ));
                        //close( fd );
                        //char achCmd[100];
                        //snprintf( achCmd, 100, "lsof -p %d 1>&2", getpid() );
                        //system( achCmd );
                        //abort();
                        problem_detected = 0;
                    }
                }

                //struct epoll_event epevt;
                //memset( &epevt, 0, sizeof( struct epoll_event ) );

                //epevt.data.u64 = 0;
                //epevt.data.fd = fd;
                //epevt.events = 0;

                //(syscall(__NR_epoll_ctl, m_epfd, EPOLL_CTL_DEL, fd, &epevt ));

                //close( fd );
                //if ( p->data.u32 != p->data.u64 )
                //{
                //    reinit();
                //    return 0;
                //}
                //(syscall(__NR_epoll_ctl, m_epfd, EPOLL_CTL_DEL, fd, m_pResEnd ));
            }
            ++p;
        }
        if ( !problem_detected && s_loop_count )
        {
            s_loop_fd = -1;
            s_loop_count = 0;
        }
        memset( m_pResults, 0, sizeof( struct epoll_event) * ret );
    }
    return ret;

}
*/

void epoll::timerExecute()
{
    m_reactorIndex.timerExec();
}

void epoll::continueRead(EventReactor *pHandler)
{
    if (!(pHandler->getEvents() & POLLIN))
        addEvent(pHandler, POLLIN);
}

void epoll::suspendRead(EventReactor *pHandler)
{
    if (pHandler->getEvents() & POLLIN)
        removeEvent(pHandler, POLLIN);
}

void epoll::continueWrite(EventReactor *pHandler)
{
    if (!(pHandler->getEvents() & POLLOUT))
        addEvent(pHandler, POLLOUT);
}

void epoll::suspendWrite(EventReactor *pHandler)
{
    if (pHandler->getEvents() & POLLOUT)
        removeEvent(pHandler, POLLOUT);
}

void epoll::switchWriteToRead(EventReactor *pHandler)
{
    setEvents(pHandler, POLLIN | POLLHUP | POLLERR);
}

void epoll::switchReadToWrite(EventReactor *pHandler)
{
    setEvents(pHandler, POLLOUT | POLLHUP | POLLERR);
}

#endif
