//config.cpp
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


void upcase(char *pStr)
{
    for (char *pCh = pStr ; *pCh ; pCh++)
        *pCh = toupper(*pCh);
}

bool KeyValPair::setValue(const char *pKey, const char *pValue)
{
    if ((!pKey) || (!pValue))
        return false;
    m_sKey = pKey;
    m_sVal = pValue;
    return true;
}

Config::Config()
    : m_iLineCount(0)
    , m_sDelim("=")
    , m_sFileName("")
    , m_pFileHandle(NULL)
    , m_sError("")
{

}


Config::~Config()
{
    HashStringMap< KeyValPair *>::iterator it = m_mapConfig.begin();
    for ( ; it != m_mapConfig.end(); )
    {
        delete (it.second());
        it = m_mapConfig.next ( it );
    }
    
    m_mapConfig.clear();
}


bool Config::loadConfig(const char *szFileName, const char *szDelim)
{
    m_sFileName = szFileName ;
    m_sDelim    = szDelim ;
    return loadConfig();
}


bool Config::open()
{
    m_pFileHandle = fopen(m_sFileName.c_str(), "r");

    if (m_pFileHandle == NULL)
    {
        m_sError = "Unable to open config file";
        onConfigError(m_sError);
        return false;
    }

    m_iLineCount = 0 ;
    return true;
}


bool Config::open(const char *szFileName)
{
    m_sFileName = szFileName ;
    return open();
}

bool Config::close()
{
    if (m_pFileHandle)
        fclose(m_pFileHandle);
    return true;
}

#define MAX_LINE_LEN 20480
bool Config::read()
{

    char chBuf[MAX_LINE_LEN];
    char sLine[MAX_LINE_LEN];
    char *pKey = NULL;
    char *pVal = NULL ;
    while (!feof(m_pFileHandle))
    {
        fgets(sLine, MAX_LINE_LEN, m_pFileHandle);
        if (strlen(sLine) >= MAX_LINE_LEN - 1)
        {
            snprintf(chBuf, MAX_LINE_LEN
                     , "Config::%d line is too long!"
                     , m_iLineCount);
            m_sError = chBuf;
            if (!onConfigError(m_sError))
                return false;
        }
        m_iLineCount ++ ;

        trimSpace(sLine);

        if (!isValidline(sLine))
            continue;

        pVal = strpbrk(sLine, m_sDelim.c_str());
        if (pVal)
        {
            pKey = sLine;
            *(char *)pVal = 0 ;
            pVal += strlen(m_sDelim.c_str());
        }
        else
        {
            snprintf(chBuf, MAX_LINE_LEN
                     , "Config::%d line Syntex error!"
                     , m_iLineCount);
            m_sError = chBuf;
            if (!onConfigError(m_sError))
                return false;
        }

        trimSpace(pKey);
        trimSpace(pVal);

        if (strlen(pKey) > 0)
        {
            upcase(pKey);
            if (!onOption(pKey, pVal))
            {
                snprintf(chBuf, MAX_LINE_LEN
                         , "Config:: Invalid key or value at %d line!"
                         , m_iLineCount);
                m_sError = chBuf;

                return false;
            }
            continue ;
        }
        else
        {
            snprintf(chBuf, MAX_LINE_LEN
                     , "Config:: Empty key at %d line!"
                     , m_iLineCount);
            m_sError = chBuf;

            return false;
        }
    }
    close();
    m_pFileHandle = NULL;
    return true ;
}

//////////////////////////////////////////////
//Ret Value:
//        true: stop
//        false: continue
/////////////////////////////////////////////

bool Config::onConfigError(const AutoStr &error)
{
    return false;
}

const char *Config::getErrorMessage() const
{
    return m_sError.c_str();
}

bool Config::loadConfig()
{
    bool bRet = true;
    if (!open())
        bRet = false;
    else
    {
        while (!Config::isFinish())
        {
            if (!Config::read())
            {
                if (onConfigError(AutoStr(getErrorMessage())))
                {
                    bRet = false;
                    break;
                }
            }
        }
    }
    close();
    return bRet;
}

bool Config::onOption(const char *pchKey, const char *pchValue)
{
    return setConfig(pchKey, pchValue);
}

void Config::trimSpace(char *pStr)
{
    //int loc1 ;
    if (pStr == NULL)
        return;
    char *p = pStr + strlen(pStr) - 1;
    while ((p >= pStr) && (isspace(*p)))
        p--;
    *(p + 1) = 0 ;
    p = pStr ;
    while ((*p) && (isspace(*p)))
        p++ ;
    if (p != pStr)
        memmove(pStr, p, strlen(p) + 1);
}

const char *Config::getConfig(const char *pKey) const
{
    int len = strlen(pKey) ;
    char sUpKey[len + 1];

    strcpy(sUpKey, pKey);
    upcase(sUpKey);
    HashStringMap< KeyValPair *>::iterator iter = m_mapConfig.find(sUpKey);
    if (iter != m_mapConfig.end())
        return iter.second()->getValue();
    else
        return NULL;

}

bool Config::setConfig(const char *pchKey, const char *pchValue)
{
    HashStringMap< KeyValPair *>::iterator iter = m_mapConfig.find(pchKey);
    if (iter != m_mapConfig.end())
        iter.second()->setValue(pchValue);
    else
    {
        KeyValPair *pPair = new KeyValPair();
        if (!pPair)
        {
            char chBuf[MAX_LINE_LEN];
            snprintf(chBuf, MAX_LINE_LEN
                     , "Config:: Out of memory at %d line!"
                     , m_iLineCount);
            m_sError = chBuf;
            return false;
        }
        pPair->setValue(pchKey, pchValue);
        m_mapConfig.insert(pPair->getKey(), pPair);
    }
    return true;
}

bool Config::isFinish()
{
    if (m_pFileHandle != NULL)
        return feof(m_pFileHandle) ;
    else
        return true ;
}

bool Config::isValidline(const char *sLine)
{
    int len = strlen(sLine);

    if (len == 0 || sLine[0] == '#')
        return false;

    const char invalidChars[] = "# \t\r\n\\";
    bool bValid = false;

    for (int i = 0; i < len; ++i)
    {
        if (!strchr(invalidChars, sLine[i]))
        {
            bValid = true;
            break;
        }
    }

    return bValid;
}

