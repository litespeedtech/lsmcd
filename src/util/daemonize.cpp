/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include <util/daemonize.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <log4cxx/logger.h>

int Daemonize::daemonize(int nochdir, int noclose)
{
#ifdef daemon
    return daemon(nochdir, noclose);
#else
    if (fork())
        _exit(0);
    if (setsid() == -1)
        return -1;
    if (fork())
        _exit(0);
    if (!nochdir)
    {
        if (!getuid())
            if (chroot("/"))
            {
                // should not happen
                LS_ERROR( "Failed to chroot to / (%d) : %s", errno, strerror( errno ) );
            }
        if (chdir("/"))
        {
            LS_ERROR( "Failed to chdir to / (%d) : %s", errno, strerror( errno ) );
        }
    }
    if (!noclose)
        close();
    return 0;
#endif
}

int Daemonize::close()
{
    int fd = open("/dev/null", O_RDWR);
    if (fd != -1)
    {
        dup2(fd, 0);
        dup2(fd, 1);
        dup2(fd, 2);
        ::close(fd);
        return 0;
    }
    return -1;
}

//int Daemonize::writePIDFile( const char * pFileName )
//{
//    FILE* pidfp = fopen( pFileName, "w" );
//    if ( pidfp == NULL )
//        return -1;
//    fprintf( pidfp, "%d\n", (int)getpid() );
//    fclose( pidfp );
//    return 0;
//}

int Daemonize::changeUserGroupRoot(const char *pUser, uid_t uid, gid_t gid,
                                   gid_t pri_gid, const char *pChroot, char *pErr, int errLen)
{
    if (initGroups(pUser, gid, pri_gid, pErr, errLen) == -1)
        return -1;
    return changeUserChroot(uid, pChroot, pErr, errLen);
}

int Daemonize::initGroups(const char *pUser, gid_t gid, gid_t pri_gid,
                          char *pErr, int errLen)
{
    if (getuid() == 0)
    {
        if (setgid(gid)  < 0)
        {
            snprintf(pErr, errLen, "setgid( %d ) failed!", (int)gid);
            return -1;
        }
        if (initgroups(pUser, pri_gid) == -1)
        {
            snprintf(pErr, errLen, "initgroups( %s, %d ) failed!",
                     pUser, (int)pri_gid);
            return -1;
        }
    }
    return 0;
}

int Daemonize::changeUserChroot(uid_t uid,
                                const char *pChroot, char *pErr, int errLen)
{
    if (getuid() == 0)
    {
        if ((pChroot) && (*(pChroot + 1) != 0))
        {
            if (chroot(pChroot) == -1)
            {
                snprintf(pErr, errLen, "chroot( %s ) failed!", pChroot);
                return -1;
            }
        }
        if (setuid(uid) < 0)
        {
            snprintf(pErr, errLen, "setuid( %d ) failed!", (int)uid);
            return -1;
        }
        if (setuid(0) != -1)
        {
            snprintf(pErr, errLen,
                     "[Kernel bug] setuid(0) succeed after gave up root privilege!");
            return -1;
        }
    }
    return 0;
}
