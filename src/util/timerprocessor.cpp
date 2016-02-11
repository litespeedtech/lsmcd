/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include "timerprocessor.h"
#include <util/timer.h>
#include <util/timertask.h>

#include <set>

class TaskSet: public std::set<TimerTask *>
{

};

TimerProcessor::TimerProcessor()
{
    m_pSet = new TaskSet();
}
TimerProcessor::~TimerProcessor()
{
    reset();
    delete m_pSet;
}

void TimerProcessor::reset()
{
    TaskSet::iterator iter = m_pSet->begin();
    for (; iter != m_pSet->end(); ++iter)
    {
        (*iter)->setProcessor(NULL);
        remove(*iter);
    }
    m_pSet->clear();
}

void TimerProcessor::add(TimerTask *pTask)
{
    m_pSet->insert(pTask);
    pTask->setProcessor(this);
}

void TimerProcessor::remove(TimerTask *pTask)
{
    if (pTask->getProcessor())
    {
        m_pSet->erase(pTask);
        pTask->setProcessor(NULL);
    }
    Timer *timer = pTask->getTimer();
    if (timer)
        timer->cancel(pTask);


}



