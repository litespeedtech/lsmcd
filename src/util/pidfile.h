/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef PIDFILE_H
#define PIDFILE_H


/**
  *@author George Wang
  */

#include <sys/stat.h>

class PidFile
{
    int             m_fdPid;
    struct stat     m_stPid;

    int openPidFile(const char *pPidFile);
    int openTmpPidFile(const char *pPidFile);

public:

    PidFile();
    ~PidFile();
    int lockPidFile(const char *pPidFile);
    int writePidFile(const char *pPidFile, int pid);
    int writePid(int pid);
    void closePidFile();
    int testAndRelockPidFile(const char *pPidFile, int pid);
    int testAndRemovePidFile(const char *pPidFile);
};

#endif
