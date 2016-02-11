/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef DAEMONIZE_H
#define DAEMONIZE_H


/**
  *@author George Wang
  */
#include <sys/types.h>

class Daemonize
{
    Daemonize() {};
    ~Daemonize() {};
public:
    static int daemonize(int nochdir, int noclose);
    static int close();
    //static int writePIDFile( const char * pFile );
    static int initGroups(const char *pUser, gid_t gid, gid_t pri_gid,
                          char *pErr, int errLen);
    static int changeUserChroot(uid_t uid,
                                const char *pChroot, char *pErr, int errLen);
    static int changeUserGroupRoot(const char *pUser, uid_t uid, gid_t gid,
                                   gid_t pri_gid, const char *pChroot, char *pErr, int errLen);
};

#endif
