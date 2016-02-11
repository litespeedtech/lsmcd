/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */

#if defined(sun) || defined(__sun)

#include "devpoller.h"
#include <edio/eventreactor.h>

//#include <http/httplog.h>

#include <errno.h>
#include <fcntl.h>
#include <sys/devpoll.h>
#include <unistd.h>


DevPoller::DevPoller()
    : m_fdDP(-1)
    , m_curChanges(0)
{
}
DevPoller::~DevPoller()
{
    if (m_fdDP != -1)
        close(m_fdDP);
}

int DevPoller::init(int capacity)
{
    if (m_reactorIndex.allocate(capacity) == -1)
        return -1;
    if ((m_fdDP = open("/dev/poll", O_RDWR)) < 0)
        return -1;
    ::fcntl(m_fdDP, F_SETFD, FD_CLOEXEC);
    return 0;
}

int DevPoller::applyChanges()
{
    int ret = write(m_fdDP, m_changes, sizeof(struct pollfd) * m_curChanges);
    if (ret != (int)sizeof(struct pollfd) * m_curChanges)
        return -1;
    m_curChanges = 0;
    return 0;
}


int DevPoller::add(EventReactor *pHandler, short mask)
{
    if (!pHandler)
        return -1;
    int fd = pHandler->getfd();
    if (m_reactorIndex.get(fd))
    {
        errno = EEXIST;
        return -1;
    }
    pHandler->setPollfd();
    pHandler->setMask2(mask);
    if (!appendChange(fd, mask))
        m_reactorIndex.set(fd, pHandler);
    return 0;

}

int DevPoller::remove(EventReactor *pHandler)
{
    if (!pHandler)
        return -1;
    int fd = pHandler->getfd();
    if (!m_reactorIndex.get(fd))
        return 0;
    if (!appendChange(fd, POLLREMOVE))
        m_reactorIndex.set(fd, NULL);
    applyChanges();
    return 0;
}

int DevPoller::waitAndProcessEvents(int iTimeoutMilliSec)
{
    if (m_curChanges)
        if (applyChanges() == -1)
            return -1;
    struct dvpoll dvp;
    dvp.dp_fds     = m_events;
    dvp.dp_nfds    = MAX_EVENTS;
    dvp.dp_timeout = iTimeoutMilliSec;
    int ret = ioctl(m_fdDP, DP_POLL, &dvp);
    if (ret > 0)
    {
        m_pEventCur = m_events;
        m_pEventEnd = &m_events[ret];
        processEvents();
    }
    return ret;

}


int DevPoller::processEvents()
{
    struct pollfd *p;
    while (m_pEventCur < m_pEventEnd)
    {
        p = m_pEventCur++;
        register int fd = p->fd;
        //LS_DBG( "DevPoller: fd: %d, revents: %hd", fd, p->revents ));
        if (fd > m_reactorIndex.getCapacity())
        {
            //LS_DBG( "DevPoller: overflow, remove fd: %d", fd ));
            appendChange(fd, POLLREMOVE);
        }
        else
        {
            EventReactor *pReactor = m_reactorIndex.get(fd);
            //if ( !pReactor )
            //    LS_DBG( "DevPoller: pReactor is NULL, remove fd: %d", fd ));
            if ((pReactor) && (p->fd == pReactor->getfd()))
            {
                pReactor->setRevent(p->revents);
                pReactor->handleEvents(p->revents);
            }
            else
            {
                //LS_DBG( "DevPoller: does not match, remove fd: %d", fd ));
                appendChange(fd, POLLREMOVE);
            }
        }
    }
    return 0;
}

void DevPoller::timerExecute()
{
    m_reactorIndex.timerExec();
}

void DevPoller::setPriHandler(EventReactor::pri_handler handler)
{
}


void DevPoller::continueRead(EventReactor *pHandler)
{
    addEvent(pHandler, POLLIN);
}

void DevPoller::suspendRead(EventReactor *pHandler)
{
    removeEvent(pHandler, POLLIN);
}

void DevPoller::continueWrite(EventReactor *pHandler)
{
    addEvent(pHandler, POLLOUT);
}

void DevPoller::suspendWrite(EventReactor *pHandler)
{
    removeEvent(pHandler, POLLOUT);
}


#endif //defined(sun) || defined(__sun)

