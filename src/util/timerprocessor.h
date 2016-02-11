/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef TIMERPROCESSOR_H
#define TIMERPROCESSOR_H


class TimerTask;
class TaskSet;

class TimerProcessor
{
    TaskSet *m_pSet;
public:
    TimerProcessor();
    virtual ~TimerProcessor();
    void reset();
    void add(TimerTask *pSet);
    void remove(TimerTask *pSet);
    virtual void onTimer(TimerTask *task) = 0;
};

#endif
