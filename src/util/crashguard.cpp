/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include <util/crashguard.h>

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <util/signalutil.h>
#include <log4cxx/logger.h>

static int s_iRunning = 0;
static int s_iSigChild = 0;
static int * s_pidChildren  = NULL;
static int s_pid    = 0;
static int s_usr1   = 0;

static void sigchild(int sig)
{
    //printf( "signchild()!\n" );
    s_iSigChild = 1;
}

static void sig_term(int sig)
{
    //printf( "sig_broadcast(%d)!\n", sig );
    s_iRunning = false;
}

static void sigusr1(int sig)
{
    //printf( "sig_broadcast(%d)!\n", sig );
    s_usr1 = true;
}

static void init()
{
    SignalUtil::signal(SIGTERM, sig_term);
    SignalUtil::signal(SIGINT, sig_term);
    SignalUtil::signal(SIGHUP, sig_term);
    SignalUtil::signal(SIGCHLD, sigchild);
    SignalUtil::signal(SIGUSR1, sigusr1);
}

int CrashGuard::guardCrash()
{
    long lLastForkTime  = 0;
    int  iForkCount     = 0;
    int  rpid   = 0;
    int  ret = 0;
    int  stat;
    int  usr1_noted = 0;
    assert(m_pGuardedApp);
    init();
    s_iRunning = 1;
    while (s_iRunning)
    {
        if (s_pid == rpid)
        {
            long curTime = time(NULL);
            if (curTime - lLastForkTime > 10)
            {
                iForkCount = 0;
                lLastForkTime = curTime;
            }
            else
            {
                ++iForkCount;
                if (iForkCount > 20)
                {
                    ret = m_pGuardedApp->forkTooFreq();
                    if (ret > 0)
                        break;
                    sleep(60);
                }
            }
            m_pGuardedApp->preFork(1);
            s_pid = fork();
            if (s_pid == -1)
            {
                m_pGuardedApp->forkError(1, errno);
                return -1;
            }
            else if (s_pid == 0)
                return 0;
            else
                m_pGuardedApp->postFork(1, s_pid);
            rpid = 0;

        }
        ::sleep(1);
        if (s_usr1 && !usr1_noted)
        {
            usr1_noted = 1;
            LS_NOTICE("[PID: %d] Doing a restart upon USR1\n", getpid());
            kill(-1, SIGTERM);
        }
        
        if (s_iSigChild)
        {
            s_iSigChild = 0;
            //printf( "waitpid()\n" );
            rpid = ::waitpid(-1, &stat, WNOHANG);
            if (rpid > 0)
            {
                if (WIFEXITED(stat))
                {
                    ret = WEXITSTATUS(stat);
                    m_pGuardedApp->childExit(1, rpid, ret);
                    break;
                }
                else if (WIFSIGNALED(stat))
                {
                    int sig_num = WTERMSIG(stat);
                    if (sig_num == SIGKILL)
                        break;
                    if (WCOREDUMP(stat) && sig_num != SIGUSR2)
                        LS_NOTICE("Program crashed and may have produced a core file.  Automatically restarted.\n");
                    ret = m_pGuardedApp->childSignaled(1, rpid, sig_num,
#ifdef WCOREDUMP
                                                       WCOREDUMP(stat)
#else
                                                       - 1
#endif
                                                      );
                    if (ret != 0)
                        break;
                }
            }
        }
        else
            m_pGuardedApp->onGuardTimer();
    }
    if (s_pid > 0)
    {
        if (::kill(s_pid, 0) == 0)
        {
            sleep(1);
            ::kill(s_pid, SIGKILL);
        }
    }
    exit(ret);
}


int CrashGuard::guardCrash(int workers)
{
    long lLastForkTime  = 0;
    int  iForkCount     = 0;
    int  iCurChildren   = 0;
    int  rpid   = 0;
    int  pidChild = 0;
    int  count = 0;
    int  ret;
    int  stat;
    assert(m_pGuardedApp);
    if ( workers <= 0 )
        return 0;
    if (workers > 8)
        workers = 8;
    s_pidChildren = (int*)malloc(workers * sizeof(int));
    memset( s_pidChildren, 0, sizeof(int)*workers);
    init();
    s_iRunning = 1;
    while (s_iRunning)
    {
        if (iCurChildren < workers)
        {
            long curTime = time(NULL);
            if (curTime - lLastForkTime > 10)
            {
                iForkCount = 0;
                lLastForkTime = curTime;
            }
            else
            {
                ++iForkCount;
                if (iForkCount > 20)
                {
                    ret = m_pGuardedApp->forkTooFreq();
                    if (ret > 0)
                        break;
                    int sleep_remain = 60;
                    while (sleep_remain)
                        sleep_remain = sleep(sleep_remain);
                    
                }
            }
            count = 0;
            while(s_pidChildren[count] != 0 && count < workers)
                ++count;
            
            assert(count != workers);
                
            m_pGuardedApp->preFork(count);
            pidChild = fork();
            if (pidChild == -1)
            {
                m_pGuardedApp->forkError(count, errno);
                return -1;
            }
            else if (pidChild == 0)
            {
                return count;
            }
            else
            {
                ++iCurChildren;
                s_pidChildren[count] = pidChild;
                m_pGuardedApp->postFork(count, pidChild);
            }

        }
        else
            ::sleep(1);
        if (s_usr1)
        {
            LS_NOTICE("[PID: %d] Doing a restart upon USR1\n", getpid());
            break;
        }
        
        if (s_iSigChild)
        {
            s_iSigChild = 0;
            //printf( "waitpid()\n" );
            rpid = ::waitpid(-1, &stat, WNOHANG);
            if (rpid > 0)
            {
                count = 0;
                while(s_pidChildren[count] != rpid && count < workers)
                    ++count;
                if (count >= workers)
                    continue;
                s_pidChildren[count] = 0;
                --iCurChildren;
                if (WIFEXITED(stat))
                {
                    ret = WEXITSTATUS(stat);
                    if ( m_pGuardedApp->childExit(count, rpid, ret) != 0)
                        break;
                }
                else if (WIFSIGNALED(stat))
                {
                    int sig_num = WTERMSIG(stat);
                    if (sig_num == SIGKILL)
                        break;
                    if (WCOREDUMP(stat) && sig_num != SIGUSR2)
                        LS_NOTICE("Program crashed and may have produced a core file.  Automatically restarted\n");
                    ret = m_pGuardedApp->childSignaled(count, rpid, sig_num,
#ifdef WCOREDUMP
                                                       WCOREDUMP(stat)
#else
                                                       - 1
#endif
                                                      );
                    if (ret != 0)
                        break;
                }
            }
        }
        else
            m_pGuardedApp->onGuardTimer();
    }
    count = 0;
    while(count < workers)
    {
        int pid = s_pidChildren[count];
        if (pid > 0)
        {
            if (::kill(pid, SIGTERM) == 0)
            {
                sleep(1);
                if (::kill(pid, 0 ) != -1)
                    ::kill(pid, SIGKILL);
            }
        }
        ++count;
    }
    free(s_pidChildren);
    if (s_usr1)
        return 16;
    exit(ret);
}


