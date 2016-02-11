/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include "systeminfo.h"
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>


SystemInfo::SystemInfo()
{
}
SystemInfo::~SystemInfo()
{
}

int SystemInfo::s_iPageSize = 0;

int SystemInfo::getPageSize()
{
    if (s_iPageSize == 0)
        s_iPageSize = sysconf(_SC_PAGESIZE);
    return s_iPageSize;
}

unsigned long long SystemInfo::maxOpenFile(unsigned long long max)
{
    struct  rlimit rl;
    unsigned long long iMaxOpenFiles = 0;
    if (getrlimit(RLIMIT_NOFILE, &rl) == 0)
    {
        iMaxOpenFiles = rl.rlim_cur;
        if ((rl.rlim_cur != RLIM_INFINITY) && (max > rl.rlim_cur))
        {
            if ((max <= rl.rlim_max) || getuid())
                rl.rlim_cur = rl.rlim_max;
            else
                rl.rlim_cur = rl.rlim_max = max;
            //if ( rl.rlim_cur == RLIM_INFINITY )
            //    rl.rlim_cur = 4096;
            if (setrlimit(RLIMIT_NOFILE, &rl) == 0)
                iMaxOpenFiles = rl.rlim_cur;
        }
    }
    return iMaxOpenFiles;
}



