/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef STRINGTOOL_H
#define STRINGTOOL_H

#include <stddef.h>
#include <sys/types.h>

/**
  *@author George Wang
  */

template< typename T> class TPointerList;

inline void skipLeadingSpace(const char **p)
{
    char ch;
    while (((ch = **p) == ' ') || (ch == '\t'))
        ++(*p);
}

inline void skipTrailingSpace(const char **p)
{
    char ch;
    while (((ch = (*p)[-1]) == ' ') || (ch == '\t'))
        --(*p);
}

inline char hexdigit(char ch)
{
    return (((ch) <= '9') ? (ch) - '0' : ((ch) & 7) + 9);
}

inline char hex2digits(const char *p)
{
    return (hexdigit(*p) << 4) | hexdigit(*(p + 1));
}


class StrParse
{
    const char *m_pBegin;
    const char *m_pEnd;
    const char *m_delim;
    int          m_delimLen;
    const char *m_pStrEnd;
public:
    StrParse(const char *pBegin, const char *pEnd, const char *delim,
             int delimLen)
        : m_pBegin(pBegin)
        , m_pEnd(pEnd)
        , m_delim(delim)
        , m_delimLen(delimLen)
    {
    }
    ~StrParse() {}
    int isEnd() const { return m_pEnd <= m_pBegin ;  }

    const char *parse();

    const char *trim_parse()
    {
        skipLeadingSpace(&m_pBegin);
        const char *p = parse();
        if (p && (p != m_pStrEnd))
            skipTrailingSpace(&m_pStrEnd);
        return p;
    }

    const char *getStrEnd() const  {   return m_pStrEnd;   }

};

class StringList;
class AutoStr2;


class StringTool
{
    StringTool();
    ~StringTool();
public:
    static const char s_hex[17];
    static inline char getHex(char x)
    {   return s_hex[ x & 0xf ];    }
    static char *strupper(const char *pSrc, char *pDest);
    static char *strnupper(const char *p, char *pDest, int &n);
    static char *strlower(const char *pSrc, char *pDest);
    static char *strnlower(const char *pSrc, char *pDest, int &n);
    static char *strtrim(char *p);
    static int    strtrim(const char *&pBegin, const char *&pEnd);
    static int    hexEncode(const char *pSrc, int len, char *pDest);
    static int    hexDecode(const char *pSrc, int len, char *pDest);
    static int    strmatch(const char *pSrc, const char *pEnd,
                           AutoStr2 *const *begin, AutoStr2 *const *end, int case_sens);
    static StringList *parseMatchPattern(const char *pPattern);
    static const char *memNextArg(const char **s, int len,
                                  const char *pDelim = NULL, int delimLen = 0);
    static char *memNextArg(char **s, int len, const char *pDelim = NULL,
                            int delimLen = 0);
    static const char *getLine(const char *pBegin,
                               const char *pEnd);
    static int parseNextArg(const char *&pRuleStr, const char *pEnd,
                            const char *&argBegin, const char *&argEnd, const char *&pError);
    static char *convertMatchToReg(char *pStr, char *pBufEnd);
    static const char *findCloseBracket(const char *pBegin, const char *pEnd,
                                        char chOpen, char chClose);

    static const char *findCharInBracket(const char *pBegin,
                                         const char *pEnd, char searched, char chOpen, char chClose);
    static int str_off_t(char *pBuf, int len, off_t val);
    static int unescapeQuote(char *pBegin, char *pEnd, int ch);
    static const char *lookupSubString(const char *pInput, const char *pEnd,
                                       const char *key,
                                       int keyLength, int *retLength, char sep, char comp);
    static const char *mempbrk(const char *pInput, int iSize,
                               const char *accept, int acceptLength);
    static void *memmem(const char *haystack, size_t haystacklen,
                        const char *needle, size_t needleLength);
    static int memcasecmp(const char *s1, const char *s2, size_t len);
    static void *strncasestr(const char *haystack, size_t haystacklen,
                                   const char *needle, size_t needleLength);
    static size_t memspn(const char *pInput, int inputSize, const char *accept,
                         int acceptLen);
    static size_t memcspn(const char *pInput, int inputSize,
                          const char *accept, int acceptLen);

    static void getMd5(const char *src, int len, unsigned char *dstBin);
    static long long atoll(const char *pValue, int base = 10);
};

#endif
