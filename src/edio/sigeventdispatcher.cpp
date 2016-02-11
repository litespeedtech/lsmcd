/*****************************************************************************
*    Open LiteSpeed is an open source HTTP server.                           *
*    Copyright (C) 2013  LiteSpeed Technologies, Inc.                        *
*                                                                            *
*    This program is free software: you can redistribute it and/or modify    *
*    it under the terms of the GNU General Public License as published by    *
*    the Free Software Foundation, either version 3 of the License, or       *
*    (at your option) any later version.                                     *
*                                                                            *
*    This program is distributed in the hope that it will be useful,         *
*    but WITHOUT ANY WARRANTY; without even the implied warranty of          *
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the            *
*    GNU General Public License for more details.                            *
*                                                                            *
*    You should have received a copy of the GNU General Public License       *
*    along with this program. If not, see http://www.gnu.org/licenses/.      *
*****************************************************************************/

#include <edio/sigeventdispatcher.h>

#if defined(LS_AIO_USE_KQ)
#include <sys/param.h>
#include <sys/linker.h>

static short s_iAiokoLoaded = -1;

void SigEventDispatcher::setAiokoLoaded()
{
    s_iAiokoLoaded = (kldfind("aio.ko") != -1);
}

short SigEventDispatcher::aiokoIsLoaded()
{
    return s_iAiokoLoaded;
}

#elif defined(LS_AIO_USE_SIGNAL)

#include <edio/multiplexer.h>
#include <edio/multiplexerfactory.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#if defined(linux) || defined(__linux) || defined(__linux__) || defined(__gnu_linux__)

#include <sys/syscall.h>

#if !defined( __NR_signalfd )
#if defined(__x86_64__)
#define __NR_signalfd 282
#elif defined(__i386__)
#define __NR_signalfd 321
#else
#error Cannot detect your architecture!
#endif //defined(__i386__)
#endif //!defined(__NR_signalfd )

#define SIZEOF_SIG (_NSIG / 8)
#define SIZEOF_SIGSET (SIZEOF_SIG > sizeof(sigset_t) ? sizeof(sigset_t): SIZEOF_SIG)


struct signalfd_siginfo
{
    u_int32_t ssi_signo;
    int32_t ssi_errno;
    int32_t ssi_code;
    u_int32_t ssi_pid;
    u_int32_t ssi_uid;
    int32_t ssi_fd;
    u_int32_t ssi_tid;
    u_int32_t ssi_band;
    u_int32_t ssi_overrun;
    u_int32_t ssi_trapno;
    int32_t ssi_status;
    int32_t ssi_int;
    u_int64_t ssi_ptr;
    u_int64_t ssi_utime;
    u_int64_t ssi_stime;
    u_int64_t ssi_addr;
    u_int8_t __pad[48];
};

static inline int signalfd(int ufc, sigset_t const *mask, int flag)
{
    return syscall(__NR_signalfd, ufc, mask, SIZEOF_SIGSET);
}

static int s_useSignalfd = 0;

#endif //defined(linux) || defined(__linux) || defined(__linux__) || defined(__gnu_linux__)

int SigEventDispatcher::processSigEvent()
{
#if defined(linux) || defined(__linux) || defined(__linux__) || defined(__gnu_linux__)
    if (s_useSignalfd)
        return 0;
#endif //defined(linux) || defined(__linux) || defined(__linux__) || defined(__gnu_linux__)

#ifdef LS_AIO_USE_SIGNAL
    struct timespec timeout;
    siginfo_t si;
    sigset_t ss;

    timeout.tv_sec = 0;
    timeout.tv_nsec = 0;

    sigemptyset(&ss);
    sigaddset(&ss, HS_AIO);

    while (sigtimedwait(&ss, &si, &timeout) > 0)
    {
        if (!sigismember(&ss, HS_AIO))
            continue;
        ((AioEventHandler *)si.si_value.sival_ptr)->onAioEvent();
    }
#endif
    return 0;
}

int SigEventDispatcher::init()
{
    sigset_t ss;
    sigemptyset(&ss);
    sigaddset(&ss, HS_AIO);
    sigprocmask(SIG_BLOCK, &ss, NULL);

#if defined(linux) || defined(__linux) || defined(__linux__) || defined(__gnu_linux__)
    SigEventDispatcher *pReactor;
    pReactor = new SigEventDispatcher(&ss);
    if (pReactor->getfd() != -1)
    {
        MultiplexerFactory::getMultiplexer()->add(pReactor, POLLIN);
        s_useSignalfd = 1;
    }
    else
        delete pReactor;
#endif
    return 0;
}


SigEventDispatcher::SigEventDispatcher(sigset_t *ss)
{
#if defined(linux) || defined(__linux) || defined(__linux__) || defined(__gnu_linux__)
    int fd = signalfd(-1, ss, 0);
    if (fd != -1)
    {
        fcntl(fd, F_SETFD, FD_CLOEXEC);
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
        setfd(fd);
    }
#endif
}

int SigEventDispatcher::handleEvents(short event)
{
#if defined(linux) || defined(__linux) || defined(__linux__) || defined(__gnu_linux__)
    int i, readCount = 0;
    signalfd_siginfo si[5];
    if (!(event & POLLIN))
        return 0;
    do
    {
        if ((readCount = read(getfd(), &si, sizeof(signalfd_siginfo) * 5)) < 0)
        {
            if (errno == EAGAIN)
                break;
            return -1;
        }
        else if (readCount == 0)
            return 0;
        readCount /= sizeof(signalfd_siginfo);
        for (i = 0; i < readCount; ++i)
            ((AioEventHandler *)si[i].ssi_ptr)->onAioEvent();
    }
    while (readCount == 5);
#endif
    return 0;
}

#endif //defined(LS_AIO_USE_SIGNAL)
