/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include <util/gpath.h>
#include <util/gpointerlist.h>
#include <util/hashstringmap.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <log4cxx/logger.h>

class ReadLinkInfo
{
public:
    AutoStr     m_sPath;
    AutoStr     m_sRes;
    int32_t     m_iRet;
    int32_t     m_iErrno;
};


class ReadLinkCache : public HashStringMap< ReadLinkInfo * >
{
public:
    ReadLinkCache(int initSize)
        : HashStringMap< ReadLinkInfo * >(initSize)
    {}
    ~ReadLinkCache()
    {}
    int readlink(const char *path, char *buf, size_t bufsiz)
    {
        int ret;
        iterator iter = find(path);
        if (iter)
        {
            ret = iter.second()->m_iRet;
            if (ret != -1)
                memccpy(buf, iter.second()->m_sRes.c_str(), 0, bufsiz);
            else
                errno = iter.second()->m_iErrno;
        }
        else
        {
            ret = ::readlink(path, buf, bufsiz);
            ReadLinkInfo *pInfo = new ReadLinkInfo();
            if (pInfo)
            {
                pInfo->m_sPath.setStr(path);
                pInfo->m_iRet = ret;
                pInfo->m_iErrno = errno;
                if (ret != -1)
                {
                    *(buf + ret) = 0;
                    pInfo->m_sRes.setStr(buf);
                }
                insert(pInfo->m_sPath.c_str(), pInfo);
            }
        }
        return ret;
    }

    void clear()
    {   release_objects();  }
};


static ReadLinkCache *s_readLinkCache = NULL;


int GPath::initReadLinkCache()
{
    s_readLinkCache = new ReadLinkCache(1000);
    return s_readLinkCache != NULL;
}


void GPath::clearReadLinkCache()
{
    if (s_readLinkCache)
        s_readLinkCache->clear();

}


bool GPath::isValid(const char *path)
{
    return (access(path, F_OK) == 0);
}


bool GPath::isWritable(const char *path)
{
    char buf[256];
    strcpy(buf, path);
    char *pEnd = strrchr(buf, '/');
    if (pEnd != NULL)
    {
        *pEnd = 0;
        return (access(buf, F_OK | W_OK) == 0);
    }
    return false;
}


int GPath::clean(char *path)
{
    clean(path, strlen(path));
    return 0;
}


int GPath::clean(char *path, int len)
{
    /**
        State:                        /      a
                           remove <------[3]---> [0]
                             |            ^   /
                             |            |  /
                             |           .| /
                        /    V    .       |/    /
                a[0] -----> [1] -------> [2] -------> remove './' ----> [1]
                    <------  ^\           |
                        a    | \/         |a
                             |  \        [0]
                             remove '/'
    */
    char ch;
    char *p0 = path;
    char *p1 = NULL;
    int state = (*p0 != '/');
    char *pEnd = p0 + len + 1 ;
    while ((ch = *p0++))
    {
        switch (state)
        {
        case 0:
            if (ch == '/')
                state = 1;
            break;
        case 1:
            if (ch == '.')
                state = 2;
            else if (ch == '/')
            {
                memmove(p0 - 1, p0, pEnd - p0);
                --p0;
                --pEnd;
            }
            else
                state = 0;
            break;
        case 2:
            if (ch == '.')
                state = 3;
            else if (ch == '/')
            {
                state = 1;
                memmove(p0 - 2, p0, pEnd - p0);
                p0 -= 2;
                pEnd -= 2;
            }
            else
                state = 0;
            break;
        case 3:
            if (ch == '/')
            {
                for (p1 = p0 - 5 ; p1 >= path ; --p1)
                {
                    if (*p1 == '/')
                        break;
                }
                if (p1 < path)
                    p1 = path;
                memmove(p1 + 1, p0, pEnd - p0);

                pEnd -= p0 - (p1 + 1);
                p0 = p1 + 1;
                state = 1;
            }
            else
                state = 0;
            break;
        }
    }
    return p0 - path - 1;
}


int GPath::concat(char *dest, size_t size, const char *pRoot,
                  const char *pAppend)
{
    int len1 = strlen(pRoot);
    int len2 = strlen(pAppend);
    if (len1 + len2 + 2 >= (int)size)
        return -1;
    strcpy(dest, pRoot);
    char *pEnd = dest + len1 - 1;
    if (*pEnd != '/')
    {
        *(++pEnd) = '/';
        *(++pEnd) = 0;
    }
    else
        ++pEnd;
    char *p2 = (char *)pAppend;
    if (*p2 == '/')
    {
        len2 -- ;
        p2 ++;
    }
    memmove(pEnd, p2, len2 + 1);

    return 0;
}


int GPath::getAbsolutePath(char *dest, size_t size,
                           const char *relativeRoot, const char *path)
{
    if (path == NULL)
        return -1;
    int iLen;
    if ((relativeRoot) && (*relativeRoot) && (*path != '/'))
    {
        if (concat(dest, size, relativeRoot, path))
            return -1;
        iLen = strlen(dest);
    }
    else
    {
        iLen = strlen(path);
        if (iLen + 1 >= (int)size)
            return -1;

        strcpy(dest, path);
    }
    char *pEnd = dest + iLen - 1;
    if (*pEnd != '/')
    {
        *(++pEnd) = '/';
        *(++pEnd) = 0;
    }

    return clean(dest);
}


int GPath::getAbsoluteFile(char *dest, size_t size,
                           const char *relativeRoot, const char *file)
{
    if (file == NULL)
        return -1;
    while (isspace(*file))
        ++file;
    if ((relativeRoot) && (*relativeRoot) && (*file != '/'))
    {
        if (concat(dest, size, relativeRoot, file))
            return -1;
    }
    else
    {
        int iLen = strlen(file);
        if (iLen + 1 >= (int)size)
            return -1;

        strcpy(dest, file);
    }
    return clean(dest);
}


bool GPath::hasSymLinks(char *path, char *pEnd, char *pStart)
{
    struct stat st;
    int ret;
    char *p;
    if (!pStart)
        pStart = path + 1;
    else if (pStart == path)
        ++pStart;
    while (pStart < pEnd)
    {
        p = (char *)memchr(pStart, '/', pEnd - pStart);
        if (p)
            *p = 0;
        ret = lstat(path, &st);
        if (p)
        {
            *p = '/';
            pStart = p + 1;
        }
        else
            pStart = pEnd;
        if (ret == -1)
            return false;
        if (S_ISLNK(st.st_mode))
            return true;
    }
    return false;
}


#define GPATH_PATH_MAX  4096
#define MAX_LINKS       8
int GPath::checkSymLinks(char *path, char *pEnd, const char *pathBufEnd,
                         char *pStart, int getRealPath, int *hasLink)
{
    struct stat st, stLink;
    int links = 0;
    int ret;
    int r;
    char *p;
    int skip = 0;
    int retrace = 0;
    char *pCur;
    char *pBuf;
    char *pBufEnd;
    char *pStoreEnd;
    char achBuf[GPATH_PATH_MAX];
    achBuf[0] = 0;
    if (hasLink)
        *hasLink = 0;
    if ((!path) || (!pEnd))
    {
        errno = EINVAL;
        return -1;
    }
    if ((!*path) || (*path != '/'))
    {
        errno = ENOENT;
        return -1;
    }
    if (!pStart)
        pStart = path;
    else if ((*pStart) && (*pStart != '/'))
    {
        if (*(--pStart) != '/')
        {
            //errno = ENOENT;
            //return -1;
            pStart = path;
        }
    }
    pCur = pStart;
    if (*pCur == '/')
        ++pCur;
    pStoreEnd = &achBuf[GPATH_PATH_MAX] - 1;
    while (1)
    {
        if (*pStart)
        {
            if (pCur >= pEnd)
                return pEnd - path;
            while (*(pCur + skip) == '/')
                ++skip;
            if (*(pCur + skip) == '.')
            {
                switch (*(pCur + skip + 1))
                {
                case 0:
                    ++skip;
                    continue;
                case '/':
                    skip += 2;
                    continue;
                case '.':
                    if ((*(pCur + skip + 2) == '/') || (*(pCur + skip + 2) == 0))
                    {
                        skip += 2 + (*(pCur + skip + 2) ? 1 : 0);
                        if (pStart + retrace > path)
                        {
                            --retrace;
                            while ((pStart + retrace > path) && (pStart[ retrace ] != '/'))
                                --retrace;
                        }
                        continue;
                    }
                }
            }
            if (skip || retrace)
            {
                memmove(pStart + retrace + 1, pCur + skip, pEnd - pCur - skip + 1);
                pStart += retrace;
                pCur = pStart + 1;
                pEnd -= skip - retrace;
                skip = retrace = 0;
                if (pCur >= pEnd)
                    return pEnd - path;
            }
            p = (char *)strchr(pCur + 1, '/');
            if (p)
                *p = 0;
        }
        else
            p = NULL;
        pBuf = &achBuf[1];

        ret = ::readlink(path, pBuf, pStoreEnd - pBuf);
        if (ret == -1)
        {
            if (errno != EINVAL)
            {
                //fprintf( stderr, "readlink(%s) errno: %d, != %d\n", path, errno, EINVAL );
                ret = pEnd - path;
                if ((errno == ENOENT) || (errno == ENOTDIR))
                {
                    if (p)
                    {
                        ret = p - path;
                        p = NULL;
                    }
                }
                assert(ret >= 0);
                break;
            }
        }
        else
        {
            if (hasLink)
                *hasLink = 1;
            if (!getRealPath)
            {
                errno = EACCES;
                ret = -1;
                break;
            }
            if (++links > MAX_LINKS)
            {
                errno = ELOOP;
                ret = -1;
                break;
            }
            if (getRealPath == 2)
                lstat(path, &stLink);
            pBufEnd = pBuf + ret;
            if (*(pBufEnd - 1) == '/')
            {
                while (*(pBufEnd - 2) == '/')
                    --pBufEnd;
            }
            (*pBufEnd) = 0;
            if (pBuf[0] == '/')
            {
                do
                {
                    ++pBuf;
                }
                while (*pBuf == '/');
                pStart = path;
                *pStart = '/';
                pCur = pStart + 1;

            }
            else
            {
                if (pBuf[0] == '.')
                {
                    if (pBuf[1] == '/')
                    {
                        pBuf += 2;
                        while (*pBuf == '/')
                            ++pBuf;
                    }
                }
            }
            if (p)
            {
                ret = pBufEnd - pBuf;
                while (*(p + 1) == '/')
                    ++p;
                if (*(pBufEnd - 1) == '/')
                    ++p;
                else
                    *p = '/';
                if (pCur + ret + (pEnd - p) >= pathBufEnd)
                {
                    errno = ENAMETOOLONG;
                    return -1;
                }
                memmove(pCur + ret, p , pEnd - p + 1);
                pEnd = pCur + ret + (pEnd - p);
            }
            else
            {
                ret = pBufEnd - pBuf;
                *(pCur + ret) = 0;
                pEnd = pCur + ret;
                if (pEnd >= pathBufEnd)
                {
                    errno = ENAMETOOLONG;
                    return -1;
                }
            }
            memmove(pCur, pBuf, ret);
            if ((getRealPath == 2)
                && (stLink.st_uid))      //link is not owned by root user
            {
                register char ch = *(pCur + ret);
                *(pCur + ret) = 0;
                r = lstat(path, &st);
                *(pCur + ret) = ch;
                if (r == -1)
                {
                    ret = -2;
                    break;
                }
                if (st.st_uid != stLink.st_uid)
                {
                    errno = EACCES;
                    ret = -3;
                    break;
                }

            }

            continue;
        }

        if (p)
        {
            *p = '/';
            pStart = p;
            pCur = p + 1;
        }
        else
            return pEnd - path;
    }
    if (p)
        *p = '/';
    return ret;
}


bool GPath::isChanged(struct stat *stNew, struct stat *stOld)
{
    return ((stNew->st_size != stOld->st_size) ||
            (stNew->st_ino != stOld->st_ino) ||
            (stNew->st_mtime != stOld->st_mtime));
}


#include <util/ni_fio.h>

int  GPath::readFile(char *pBuf, int bufLen, const char *pName,
                     const char *pBase)
{
    char achFilePath[512];
    int fd;
    if (getAbsoluteFile(achFilePath, 512, pBase, pName) == -1)
        return -1;
    fd = nio_open(achFilePath, O_RDONLY, 0644);
    if (fd == -1)
        return -1;
    int ret, total = 0;
    while ((bufLen > total)
           && ((ret = nio_read(fd, &pBuf[total], bufLen - total)) > 0))
        total += ret;
    nio_close(fd);
    return total;
}


int  GPath::writeFile(const char *pBuf, int bufLen, const char *pName,
                      int mode, const char *pBase)
{
    char achFilePath[512];
    int fd;
    if (getAbsoluteFile(achFilePath, 512, pBase, pName) == -1)
        return -1;
    fd = nio_creat(achFilePath, mode);
    if (fd == -1)
        return -1;
    int total = 0;
    while (total < bufLen)
    {
        int ret = nio_write(fd, pBuf + total, bufLen - total);
        if (ret > 0)
            total += ret;
        else
            break;
    }
    nio_close(fd);
    return total;
}


int GPath::createMissingPath(char *pBuf, int mode, int uid, int gid)
{
    struct stat st;
    if (!GPath::isValid(pBuf))
    {
        char *p = strchr(pBuf + 1, '/');
        while (p)
        {
            *p = 0;
            if (nio_stat(pBuf, &st) == -1)
            {
                if (mkdir(pBuf, mode) == -1)
                    return -1;
                if ((uid != -1) || (gid != -1))
                {
                    if (chown(pBuf, uid, gid))
                    {
                        LS_ERROR( "Failed to chown (%d) : %s", errno, strerror( errno ) );
                        return -1;
                    }
                }
            }
            *p++ = '/';
            p = strchr(p, '/');
        }
    }
    return 0;

}


int GPath::safeCreateFile( const char *pFile, int mode)
{
    char achTmp[4096];
    int fd;
    int n = snprintf(achTmp, 4095, "%s.XXXXXX", pFile);
    if (n > 4095)
        return -1;
    fd = mkstemp(achTmp);
    if (fd != -1)
    {
        fcntl(fd, F_SETFD,  FD_CLOEXEC);
        if (rename(achTmp, pFile) == -1)
        {
            unlink(achTmp);
            close(fd);
            fd = -1;
        }
        chmod(pFile, mode);
    }
    return fd;
}



