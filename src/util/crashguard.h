/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef CRASHGUARD_H
#define CRASHGUARD_H


/**
  *@author George Wang
  */
#include <util/guardedapp.h>

class CrashGuard
{
    GuardedApp *m_pGuardedApp;
public:
    CrashGuard(GuardedApp *pApp)
        : m_pGuardedApp(pApp)
    {}
    ~CrashGuard() {};
    int guardCrash();
    int guardCrash(int workers);

};

#endif
