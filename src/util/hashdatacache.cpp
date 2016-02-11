/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include <util/hashdatacache.h>
#include <util/pool.h>
#include <util/keydata.h>
#include <util/ni_fio.h>
#include <log4cxx/logger.h>

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h>


HashDataCache::~HashDataCache()
{
}

const KeyData *HashDataCache::getData(const char *pKey)
{
    const_iterator iter;
    iter = find(pKey);
    if (iter != end())
        return iter.second();
    return NULL;
}



DataStore::~DataStore()
{
    if (m_uriDataStore)
        g_pool.deallocate2(m_uriDataStore);
}

void DataStore::setDataStoreURI(const char *pURI)
{
    if (m_uriDataStore)
        g_pool.deallocate2(m_uriDataStore);
    m_uriDataStore = g_pool.dupstr(pURI);
}



int FileStore::isStoreChanged(long time)
{
    long curTime = ::time(NULL);
    if (curTime != m_lLastCheckTime)
    {
        struct stat st;
        m_lLastCheckTime = curTime;
        if (nio_stat(getDataStoreURI(), &st) == -1)
            return -1;
        if (m_lModifiedTime != st.st_mtime)
            return 1;
    }
    if (time < m_lModifiedTime)
        return 1;
    return 0;
}

int FileStore::open()
{
    if (m_fp == NULL)
    {
        struct stat st;
        if (nio_stat(getDataStoreURI(), &st) == -1)
            return errno;
        if (S_ISDIR(st.st_mode))
            return EINVAL;
        m_fp = fopen(getDataStoreURI(), "r");
        if (m_fp == NULL)
        {
            LS_NOTICE("Failed to open file: '%s'", getDataStoreURI());
            return errno;
        }
        m_lModifiedTime = st.st_mtime;
    }
    return 0;
}

void FileStore::close()
{
    if (m_fp)
    {
        fclose(m_fp);
        m_fp = NULL;
    }
}



#define TEMP_BUF_LEN 4096

KeyData *FileStore::getNext()
{
    char pBuf[TEMP_BUF_LEN + 1];
    if (!m_fp)
        return NULL;
    while (!feof(m_fp))
    {
        if (fgets(pBuf, TEMP_BUF_LEN, m_fp))
        {
            KeyData *pData = parseLine(pBuf, &pBuf[strlen(pBuf)]);
            if (pData)
                return pData;
        }
    }
    return NULL;
}

KeyData *FileStore::getDataFromStore(const char *pKey, int keyLen)
{

    const char *pPos;
    KeyData *pData = NULL;
    int fd;
    struct stat st;
    const char *pBuf, *pEnd;
    const char *pLine, *pLineEnd;
    fd = nio_open(getDataStoreURI(), O_RDONLY, 0644);
    if (fd == -1)
        return NULL;
    fstat(fd, &st);
    pBuf = (const char *) mmap(NULL, st.st_size, PROT_READ,
                               MAP_PRIVATE | MAP_FILE, fd, 0);
    if (pBuf == MAP_FAILED)
        return NULL;
    m_lModifiedTime = st.st_mtime;
    pEnd = pBuf + st.st_size;
    nio_close(fd);
    pLine = pBuf;
    while (pLine < pEnd)
    {
        pLineEnd = (const char *)memchr(pLine, '\n', pEnd - pLine);
        if (!pLineEnd)
            pLineEnd = pEnd;
        if (strncmp(pLine, pKey, keyLen) == 0)
        {
            register char ch;
            pPos = pLine + keyLen;
            while (((ch = *pPos) == ' ') || (ch == '\t'))
                ++pPos;
            if (*pPos++ == ':')
            {
                const char *p = pLineEnd;
                while ((p > pPos) && (((ch = p[-1]) == '\n') || (ch == '\r')))
                    --p;
                pData = parseLine(pKey, keyLen, pPos, p);
                if (pData)
                    break;
            }
        }
        if (pLineEnd < pEnd)
            pLine = pLineEnd + 1;
        else
            break;
    }
    munmap((void *)pBuf, st.st_size);
    return pData;
}



