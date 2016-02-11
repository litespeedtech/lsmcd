/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef PROFILETIME_H
#define PROFILETIME_H


#include <sys/time.h>

class ProfileTime
{
private:
    struct timeval m_tv;
    char *m_pDesc;
public:
    ProfileTime();
    explicit ProfileTime(const char *pDesc);
    ~ProfileTime();
    long getTimeMicroSecs();
};

#endif
