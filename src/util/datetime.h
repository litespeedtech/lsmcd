/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef DATETIME_H
#define DATETIME_H


/**
  *@author George Wang
  */

#include <time.h>

#define RFC_1123_TIME_LEN 29
class DateTime
{
public:
    static time_t s_curTime;
    static int    s_curTimeUs;
    DateTime();
    ~DateTime();

    static time_t parseHttpTime(const char *s);
    static char  *getRFCTime(time_t t, char *buf);
    static char  *getLogTime(time_t lTime, char *pBuf, int bGMT = 0);
};

#endif
