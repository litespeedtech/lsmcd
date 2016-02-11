/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef TIMER_H
#define TIMER_H


/**
  *@author George Wang
  */

class TimerTask;
class TimerImpl;

class Timer
{
    TimerImpl *m_impl;
public:
    Timer();
    ~Timer();
    /** Schedules the specified task for execution after the specified delay.
     *  @param task task to be scheduled.
     *  @param delay delay in seconds before task is to be executed.
     */
    int  schedule(TimerTask *task, long delay);
    void cancel(TimerTask *task);
    void execute();
    static int currentTime();
};

#endif
