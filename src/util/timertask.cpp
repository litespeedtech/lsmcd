/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include "timertask.h"

TimerTask::TimerTask()
    : m_scheduled(0)
    , m_event(0)
    , m_pTimer(0)
    , m_pProcessor(0)
{
}
TimerTask::~TimerTask()
{
}

void TimerTask::reset()
{
    m_scheduled     = 0;
    m_event         = 0;
    m_pProcessor    = 0;
}


