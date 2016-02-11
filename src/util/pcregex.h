/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef PCREGEX_H
#define PCREGEX_H


/**
  *@author George Wang
  */

#include <util/autostr.h>
#include <pcre.h>
#include <pcreposix.h>


class GHash;

class RegexResult
{
public:
    RegexResult()
        : m_pBuf(NULL)
        , m_matches(0)
    {}
    ~RegexResult() {}

    void setBuf(const char *pBuf)       {   m_pBuf = pBuf;          }
    void setMatches(int matches)        {   m_matches = matches;    }
    int getSubstr(int i, char *&pValue) const;
    int *getVector()                    {   return m_ovector;       }
    int  getMatches() const             {   return m_matches;       }

private:
    const char *m_pBuf;
    int    m_ovector[30];
    int    m_matches;
};

#define REGEX_CONTAINS      0
#define REGEX_BEGINSWITH    1
#define REGEX_ENDSWITH      2
#define REGEX_STREQ         3
#define REGEX_REGULAR       4
#define REGEX_BLANK         5
#define REGEX_ANYTHING      6

#define PCREGEX_CI          (1<<5)

#define PCREGEX_MATCHBEGIN  (1<<6)
#define PCREGEX_MATCHEND    (1<<7)


// NOTICE: This bit is not used by pcre as of version 8.37
// If this bit ends up being used for a pcre option, look for an option
// in pcre.h that is tagged C2.  Those are not used for compilation, so it
// should be safe to use it.
#define PCREGEX_OPT_ALWAYSREGEX 0x80000000

class Pcregex       //pcreapi
{
    char          m_type;
    char          m_flag;
    short         m_iSubStr;
    union
    {
        pcre     *m_regex;
        AutoStr2 *m_str;
    };
    pcre_extra   *m_extra;
    AutoStr       m_pattern;

public:
    Pcregex();
    ~Pcregex();
    int  compile(const char *regex, int options, int matchLimit = 0,
                 int recursionLimit = 0);
    int  exec(const char *subject, int length, int startoffset,
              int options, int *ovector, int ovecsize) const;
    int  exec(const char *subject, int length, int startoffset,
              int options, RegexResult *pRes) const
    {
        pRes->setMatches(exec(subject, length, startoffset, options,
                              pRes->getVector(), 30));
        return pRes->getMatches();
    }
    void release();
    short getSubStrCount() const        {   return m_iSubStr;           }
    const char *getPattern() const      {   return m_pattern.c_str();   }

    static void enableJit();
    static const Pcregex *get(const char *pRegex, int options, int matchLimit = 0,
                 int recursionLimit = 0);
    static void releaseAll();
    
    
    static int isSimple(const char *pInput, int inputSize, int iLowerCase,
                        char *pBuf, int iMaxBufLen, char *pType);
    static char getType(char cType)
    {   return cType & ~(PCREGEX_MATCHBEGIN | PCREGEX_MATCHEND);    }
};


class RegSub
{

    typedef struct
    {
        int m_strBegin;
        int m_strLen;
        int m_param;
    } RegSubEntry;
    AutoStr       m_parsed;
    RegSubEntry *m_pList;
    RegSubEntry *m_pListEnd;

public:
    RegSub();
    RegSub(const RegSub &rhs);
    ~RegSub();
    void release();
    int compile(const char *input);
    int exec(const char *input, const int *ovector, int ovec_num,
             char *output, int &length);
};


#endif
