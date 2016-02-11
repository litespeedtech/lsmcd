/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef TIMERTASK_H
#define TIMERTASK_H


class TimerProcessor;
class Timer;
class TimerTask
{
    long               m_scheduled;
    int                m_event;
    Timer             *m_pTimer;
    TimerProcessor    *m_pProcessor;
public:
    TimerTask();
    TimerTask(TimerProcessor *pProcessor)
        : m_scheduled(0)
        , m_event(0)
        , m_pTimer(0)
        , m_pProcessor(pProcessor)
    {}
    ~TimerTask();
    void reset();
    int  getEvent() const
    {   return m_event;     }
    void setEvent(int event)
    {   m_event = event;    }
    long getScheduledTime() const
    {   return m_scheduled; }
    void setScheduledTime(long scheduled)
    {   m_scheduled = scheduled;    }
    TimerProcessor *getProcessor() const
    {   return m_pProcessor;    }
    void setProcessor(TimerProcessor *pProcessor)
    {   m_pProcessor = pProcessor;  }
    Timer *getTimer() const
    {   return m_pTimer;    }
    void setTimer(Timer *pTimer)
    {   m_pTimer = pTimer;   }
};

#endif
