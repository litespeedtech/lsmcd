/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef SIGNALUTIL_H
#define SIGNALUTIL_H


/**
  *@author George Wang
  */
#include <signal.h>
#include <setjmp.h>

#ifndef sighandler_t
typedef void (*sighandler_t)(int);
#endif
class SignalUtil
{
public:
    SignalUtil();
    ~SignalUtil();
    static sighandler_t signal(int sig, sighandler_t handler);
    static int  initSigbusHandler();
    static inline void setSigBusJmpBuf(sigjmp_buf * pJmp)
    {   s_sigbus_jmp = pJmp;    }
    
private:
    static void sigbus_handler(int sig);
    static sigjmp_buf *s_sigbus_jmp;
};

#endif
