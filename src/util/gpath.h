/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef GPATH_H
#define GPATH_H


/**
  *@author George Wang
  */

#include <stddef.h>

struct stat;
class GPath
{
    GPath() {};
    ~GPath() {};
public:
    static int getAbsolutePath(char *dest, size_t size,
                               const char *relativeRoot, const char *path);
    static int getAbsoluteFile(char *dest, size_t size,
                               const char *relativeRoot, const char *file);
    static int concat(char *dest, size_t size, const char *pRoot,
                      const char *pAppend);
    static int clean(char *path);
    static int clean(char *path, int len);
    static bool isValid(const char *path);
    static bool isWritable(const char *path);
    static bool hasSymLinks(char *path, char *pEnd, char *pStart);
    static int  checkSymLinks(char *path, char *pEnd, const char *pathBufEnd,
                              char *pStart, int getRealPath, int *hasLink = NULL);
    static bool isChanged(struct stat *stNew, struct stat *stOld);
    static int  readFile(char *pBuf, int bufLen, const char *pName,
                         const char *pBase = NULL);
    static int  writeFile(const char *pBuf, int bufLen, const char *pName,
                          int mode = 0644, const char *pBase = NULL);
    static int  createMissingPath(char *pBuf, int mode = 0755, int uid = -1,
                                  int gid = -1);

    static int  initReadLinkCache();
    static void clearReadLinkCache();
    static int  safeCreateFile( const char *pFile, int mode);

};

#endif
