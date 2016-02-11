/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include "timer.h"
#include <util/timerprocessor.h>
#include <util/timertask.h>

#include <errno.h>
#include <stdio.h>
#include <time.h>

#include <map>
#include <set>

static long s_lTime = 0;

class TimerTaskList : public std::set<TimerTask *>
{
public:
    void execute();
};


void TimerTaskList::execute()
{
    iterator iterEnd = end();
    iterator iter;
    for (iter = begin(); iter != iterEnd; ++iter)
    {
        TimerProcessor *pProcessor = (*iter)->getProcessor();
        assert(pProcessor);
        pProcessor->onTimer(*iter);
    }
}


class TimerImpl : private std::map<long, TimerTaskList>
{
public:
    void add(TimerTask *task, long time);
    void remove(TimerTask *task);
    void execute();
};

void TimerImpl::add(TimerTask *task, long time)
{
    static TimerTaskList empty;
    task->setScheduledTime(time);
    std::pair< iterator, bool > ret = insert(value_type(time, empty));
    ret.first->second.insert(task);
}

void TimerImpl::remove(TimerTask *task)
{
    iterator iter = find(task->getScheduledTime());
    if (iter != end())
        iter->second.erase(task);
}

void TimerImpl::execute()
{
    iterator iter;
    iterator iterEnd = end();
    for (iter = begin(); iter != iterEnd;)
    {
        if (iter->first <= s_lTime)
        {
            iter->second.execute();
            iterator iterTemp = iter++;
            erase(iterTemp);
        }
        else
            break;
    }
}


Timer::Timer()
{
    s_lTime = time(NULL);
    m_impl = new TimerImpl();
}

Timer::~Timer()
{
}


/** Schedules the specified task for execution after the specified delay.  */
int Timer::schedule(TimerTask *task, long delay)
{
    if (delay < 0)
        return EINVAL;
    if (delay == 0)
        task->getProcessor()->onTimer(task);
    else
    {
        m_impl->add(task, delay + s_lTime);
        task->setTimer(this);
    }
    return 0;
}

void Timer::cancel(TimerTask *task)
{
    m_impl->remove(task);
    task->setTimer(NULL);
}

void Timer::execute()
{
    //printf( "Timer::execute()\n" );
    long lCurTime = time(NULL);
    if (lCurTime != s_lTime)
    {
        s_lTime = lCurTime;
        m_impl->execute();
    }
}

int Timer::currentTime()
{
    return s_lTime;
}


