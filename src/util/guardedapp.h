/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */

#ifndef GUARDEDAPP_H
#define GUARDEDAPP_H


/**
  *@author George Wang
  */

#include <sys/types.h>

class GuardedApp
{
public:
    GuardedApp() {};
    virtual ~GuardedApp() {};
    virtual int forkTooFreq()           {   return 0;   }
    virtual int preFork(int seq)        {   return 0;   }
    virtual int forkError(int seq, int err)       {   return 0;   }
    virtual int postFork(int seq, pid_t pid)      {   return 0;   }
    virtual int childExit(int seq, pid_t pid, int stat)   {   return 0;   }
    virtual int childSignaled(int seq, pid_t pid, int signal, int coredump)
    {   return 0;   }
    virtual int cleanUp(int seq, pid_t pid, char *pBB)       {   return 0;   }
    virtual void onGuardTimer() {}
};

#endif
