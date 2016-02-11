/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef ITIMER_H
#define ITIMER_H



class ITimer
{
public:
    ITimer()    {};
    virtual ~ITimer()   {};
    virtual void onTimer() = 0;
};

#endif
