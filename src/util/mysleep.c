/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include <util/mysleep.h>

#include <errno.h>
#ifndef __USE_POSIX199309
#define __USE_POSIX199309
#endif
#include <time.h>

void my_sleep(int millisec)
{
    struct timespec tm;
    if (!millisec)
        return;
    tm.tv_sec = millisec / 1000;
    tm.tv_nsec = millisec % 1000 * 1000000;
    while ((nanosleep(&tm, &tm) == -1) && (errno == EINTR)
           && (tm.tv_sec > 0) && (tm.tv_nsec > 0));
}


