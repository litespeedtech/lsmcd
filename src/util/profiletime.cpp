/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include "profiletime.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ProfileTime::ProfileTime(const char *pDesc)
    : m_pDesc(NULL)
{
    assert(NULL != pDesc);
    m_pDesc = strdup(pDesc);
    //printf( "Start executing: %s\n", pDesc );
    gettimeofday(&m_tv, NULL);
}
ProfileTime::~ProfileTime()
{
    if (m_pDesc)
    {
        struct timeval now;
        gettimeofday(&now, NULL);
        printf("End executing %s, took: %ld microseconds\n", m_pDesc,
               ((long) now.tv_sec - m_tv.tv_sec) * 1000000 +
               now.tv_usec - m_tv.tv_usec);
        free(m_pDesc);
    }
}

ProfileTime::ProfileTime()
    : m_pDesc(NULL)
{
    gettimeofday(&m_tv, NULL);
}


long ProfileTime::getTimeMicroSecs()
{
    struct timeval now;
    gettimeofday(&now, NULL);
    return ((long) now.tv_sec - m_tv.tv_sec) * 1000000 +
           now.tv_usec - m_tv.tv_usec;
}
