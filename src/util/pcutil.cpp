/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include <util/pcutil.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(linux) || defined(__linux) || defined(__linux__)
#include <sched.h>
#define _GNC_SOURCE
#include <pthread.h>
#endif

#ifdef __FreeBSD__
#include <sys/sysctl.h>
#include <sys/param.h>
#include <sys/cpuset.h>
#endif

PCUtil::PCUtil()
{
}
PCUtil::~PCUtil()
{
}


int PCUtil::waitChildren()
{
    int status, pid;
    int count = 0;
    while (true)
    {
        pid = waitpid(-1, &status, WNOHANG | WUNTRACED);
        if (pid <= 0)
        {
            //if ((pid < 1)&&( errno == EINTR ))
            //    continue;
            break;
        }
        ++count;
    }
    return count;
}

int PCUtil::s_nCpu = -1;
cpu_set_t    PCUtil::s_maskAll;

int PCUtil::getNumProcessors()
{
    if (s_nCpu > 0)
        return s_nCpu;
#if defined(linux) || defined(__linux) || defined(__linux__)
    s_nCpu = sysconf(_SC_NPROCESSORS_ONLN);
#else
    int mib[2];
    size_t len = sizeof(s_nCpu);
    mib[0] = CTL_HW;
    mib[1] = HW_NCPU;
    sysctl(mib, 2, &s_nCpu, &len, NULL, 0);
    if (s_nCpu <= 0)
    {
        mib[1] = HW_NCPU;
        sysctl(mib, 2, &s_nCpu, &len, NULL, 0);
    }
#endif
    if (s_nCpu <= 0)
        s_nCpu = 1;
    getAffinityMask(s_nCpu, s_nCpu, s_nCpu, &s_maskAll);
    return s_nCpu;
}


void PCUtil::getAffinityMask(int iCpuCount, int iProcessNum,
                             int iNumCoresToUse, cpu_set_t *mask)
{
    CPU_ZERO(mask);
    if (iCpuCount <= iNumCoresToUse)
        for (int i = 0; i < iCpuCount; i++)
            CPU_SET(i, mask);
    else
        for (int i = 0; i < iNumCoresToUse; i++)
            CPU_SET(
                ((((iProcessNum + (i * 3)) % iCpuCount) / (iCpuCount / 2)) 
                   + (2 * (iProcessNum + (i * 3))) % iCpuCount),
                mask
            );
    return;
}

int PCUtil::setCpuAffinity(cpu_set_t *mask)
{
#ifdef __FreeBSD__
    return cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_PID, -1,
                              sizeof(cpuset_t), mask);
#elif defined(linux) || defined(__linux) || defined(__linux__)
    return pthread_setaffinity_np( pthread_self(), sizeof(cpu_set_t), mask);
    //return sched_setaffinity(0, sizeof(cpu_set_t), mask);
#endif
    return 0;
}

void PCUtil::setCpuAffinityAll()
{
    if (s_nCpu < 0)
        getNumProcessors();
    setCpuAffinity(&s_maskAll);
}

