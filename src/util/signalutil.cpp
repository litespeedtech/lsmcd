/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include <util/signalutil.h>
#include <stdlib.h>

SignalUtil::SignalUtil()
{
}
SignalUtil::~SignalUtil()
{
}
sighandler_t SignalUtil::signal(int sig, sighandler_t f)
{
    struct sigaction act, oact;

    act.sa_handler = f;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    if (sig == SIGALRM)
    {
#ifdef SA_INTERRUPT
        act.sa_flags |= SA_INTERRUPT; /* SunOS */
#endif
    }
    else
    {
#ifdef SA_RESTART
        act.sa_flags |= SA_RESTART;
#endif
    }
    if (sigaction(sig, &act, &oact) < 0)
        return (SIG_ERR);
    return (oact.sa_handler);
}

sigjmp_buf *SignalUtil::s_sigbus_jmp = NULL;


void SignalUtil::sigbus_handler(int sig)
{
    if (SignalUtil::s_sigbus_jmp)
    {
        siglongjmp(*SignalUtil::s_sigbus_jmp, 1);
    }
    abort();
}

int  SignalUtil::initSigbusHandler()
{
    SignalUtil::signal(SIGBUS, SignalUtil::sigbus_handler);
    return 0;
}


