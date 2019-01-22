/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include <util/pidfile.h>
#include <util/ni_fio.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

PidFile::PidFile()
    : m_fdPid(-1)
{
}
PidFile::~PidFile()
{
}

static int lockFile(int fd, short lockType = F_WRLCK)
{
    int ret;
    struct flock lock;
    lock.l_type = lockType;
    lock.l_start = 0;
    lock.l_whence = SEEK_SET;
    lock.l_len = 0;
    while (1)
    {
        ret = fcntl(fd, F_SETLK, &lock);
        if ((ret == -1) && (errno == EINTR))
            continue;
        return ret;
    }
}

static int writePid(int fd, pid_t pid)
{
    char achBuf[20];
    int len = snprintf(achBuf, 20, "%d\n", (int)pid);
    if (ftruncate(fd, 0) || (nio_write(fd, achBuf, len) != len))
    {
        nio_close(fd);
        return -1;
    }
    return fd;
}

#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif


int PidFile::createDir(const char* pDir)
{
    char *pSlash;
    
    pSlash = strrchr((char *)pDir, '/');
    if ((!pSlash) || (pSlash == pDir))
        return 0;
    char pDirOnly[strlen(pDir) + 1];
    strcpy(pDirOnly, pDir);
    pDirOnly[pSlash - pDir] = 0;
    if (access(pDirOnly, 0) == 0)
        return 0;
    // Go to the top and create the dirs on the way back down.
    if (createDir(pDirOnly) == -1)
        return -1;
    if (mkdir(pDirOnly, 0755) == -1)
        return -1;
    return 0;
}

int PidFile::openPidFile(const char *pPidFile)
{
    //closePidFile();
    if (m_fdPid == -1)
    {
        if (createDir(pPidFile) == -1)
            return -1;
        m_fdPid  = nio_open(pPidFile, O_WRONLY | O_CREAT | O_NOFOLLOW, 0644);
        if (m_fdPid != -1)
            fcntl(m_fdPid, F_SETFD,  FD_CLOEXEC);
    }
    return m_fdPid;
}

int PidFile::openTmpPidFile(const char *pPidFile)
{
    char achTmp[4096];
    int fd;
    int n = snprintf(achTmp, 4095, "%s.XXXXXX", pPidFile);
    if (n > 4095)
        return -1;
    if (createDir(achTmp) == -1)
        return -1;
    fd = mkstemp(achTmp);
    if (fd != -1)
    {
        fcntl(fd, F_SETFD,  FD_CLOEXEC);
        if (rename(achTmp, pPidFile) == -1)
        {
            unlink(achTmp);
            close(fd);
            fd = -1;
        }
        chmod(pPidFile, 0644);
    }
    return fd;
}

int PidFile::lockPidFile(const char *pPidFile)
{
    int fd;
    if (openPidFile(pPidFile) < 0)
        return -1;
    if (lockFile(m_fdPid))
    {
        nio_close(m_fdPid);
        m_fdPid = -1;
        return -2;
    }
    fd = openTmpPidFile(pPidFile);
    if (fd != -1)
    {
        if (lockFile(fd))
        {
            nio_close(fd);
            nio_close(m_fdPid);
            m_fdPid = -1;
            return -2;
        }
        closePidFile();
        m_fdPid = fd;
    }
    return 0;
}
int PidFile::writePidFile(const char *pPidFile, int pid)
{
    int fd = openTmpPidFile(pPidFile);
    if (fd < 0)
        return -1;
    ::writePid(fd, pid);
    nio_close(fd);
    m_fdPid = -1;
    return 0;
}

int PidFile::writePid(int pid)
{
    int ret = ::writePid(m_fdPid, pid);
    if (ret < 0)
        return ret;
    while (1)
    {
        ret = fstat(m_fdPid, &m_stPid);
        if ((ret == -1) && (errno = EINTR))
            continue;
        return ret;
    }
}

void PidFile::closePidFile()
{
    if (m_fdPid >= 0)
    {
        lockFile(m_fdPid, F_UNLCK);
        close(m_fdPid);
        m_fdPid = -1;
    }
}


int PidFile::testAndRemovePidFile(const char *pPidFile)
{
    struct stat st;
    if ((nio_stat(pPidFile, &st) == 0) &&
        (st.st_mtime == m_stPid.st_mtime) &&
        (st.st_size == m_stPid.st_size) &&
        (st.st_ino == m_stPid.st_ino))
        unlink(pPidFile);
    return 0;
}

int PidFile::testAndRelockPidFile(const char *pPidFile, int pid)
{
    struct stat st;
    if ((nio_stat(pPidFile, &st) == -1) ||
        (st.st_mtime != m_stPid.st_mtime) ||
        (st.st_size != m_stPid.st_size) ||
        (st.st_ino != m_stPid.st_ino))
    {
        closePidFile();
        int ret = lockPidFile(pPidFile);
        if (!ret)
            ret = writePid(pid);
        return ret;
    }
    return 0;
}



