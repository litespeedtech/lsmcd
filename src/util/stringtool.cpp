/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include <util/stringtool.h>
#include <util/autostr.h>
#include <util/stringlist.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <md5.h>

StringTool::StringTool()
{
}


StringTool::~StringTool()
{
}


char *StringTool::strupper(const char *pSrc, char *pDest)
{
    if (pSrc && pDest)
    {
        char *p1 = pDest;
        while (*pSrc)
            *p1++ = toupper(*pSrc++);
        *p1 = 0;
        return pDest;
    }
    else
        return NULL;
}


char *StringTool::strnupper(const char *pSrc, char *pDest, int &n)
{
    if (pSrc && pDest)
    {
        char *p1 = pDest;
        char *p2 = pDest + n;
        while (p1 < p2 && (*pSrc))
            *p1++ = toupper(*pSrc++);
        if (p1 < p2)
            *p1 = 0;
        n = p1 - pDest;
        return pDest;
    }
    else
        return NULL;
}


char *StringTool::strlower(const char *pSrc, char *pDest)
{
    if (pSrc && pDest)
    {
        char *p1 = pDest;
        while (*pSrc)
            *p1++ = tolower(*pSrc++);
        *p1 = 0;
        return pDest;
    }
    else
        return NULL;
}


char *StringTool::strnlower(const char *pSrc, char *pDest, int &n)
{
    if (pSrc && pDest)
    {
        char *p1 = pDest;
        char *p2 = pDest + n;
        while (p1 < p2 && (*pSrc))
            *p1++ = tolower(*pSrc++);
        if (p1 < p2)
            *p1 = 0;
        n = p1 - pDest;
        return pDest;
    }
    else
        return NULL;
}


char *StringTool::strtrim(char *p)
{
    if (p)
    {
        while ((*p) && (isspace(*p)))
            ++p;
        if (*p)
        {
            char *p1 = p + strlen(p) - 1;
            while ((p1 > p) && (isspace(*p1)))
                --p1;
            *(p1 + 1) = 0;
        }
    }
    return p;
}


int StringTool::strtrim(const char *&pBegin, const char *&pEnd)
{
    if (pBegin && pEnd > pBegin)
    {
        while ((pBegin < pEnd) && (isspace(*pBegin)))
            ++pBegin;
        while ((pBegin < pEnd) && (isspace(*(pEnd - 1))))
            --pEnd;
        return pEnd - pBegin;
    }
    return 0;
}


const char StringTool::s_hex[17] = "0123456789abcdef";

int StringTool::hexEncode(const char *pSrc, int len, char *pDest)
{
    const char *pEnd = pSrc + len;
    if (pDest == pSrc)
    {
        char *pDestEnd = pDest + (len << 1);
        *pDestEnd-- = 0;
        while ((--pEnd) >= pSrc)
        {
            *pDestEnd-- = s_hex[ *pEnd & 0xf ];
            *pDestEnd-- = s_hex[((unsigned char)(*pEnd)) >> 4 ];
        }
    }
    else
    {
        while (pSrc < pEnd)
        {
            *pDest++ = s_hex[((unsigned char)(*pSrc)) >> 4 ];
            *pDest++ = s_hex[ *pSrc++ & 0xf ];
        }
        *pDest = 0;
    }
    return len << 1;
}


int StringTool::hexDecode(const char *pSrc, int len, char *pDest)
{
    const char *pEnd = pSrc + len - 1;
    while (pSrc < pEnd)
    {
        *pDest++ = (hexdigit(*pSrc) << 4) + hexdigit(*(pSrc + 1));
        pSrc += 2;
    }
    return len / 2;
}


int StringTool::str_off_t(char *pBuf, int len, off_t val)
{
    char *p = pBuf;
    char *p1;
    char *pEnd = pBuf + len;
    p1 = pEnd;
    if (val < 0)
    {
        *p++ = '-';
        val = -val;
    }

    do
    {
        *--p1 = '0' + (val % 10);
        val = val / 10;
    }
    while (val > 0);

    if (p1 != p)
        memmove(p, p1, pEnd - p1);
    p += pEnd - p1;
    *p = 0;
    return p - pBuf;

}


const char *StrParse::parse()
{
    const char *pStrBegin;
    if (!m_pBegin || !m_delim || !(*m_delim))
        return NULL;
    if (m_pBegin < m_pEnd)
    {
        pStrBegin = m_pBegin;
        m_pStrEnd = StringTool::mempbrk(m_pBegin, m_pEnd - m_pBegin, m_delim,
                                        m_delimLen);
        if (!m_pStrEnd)
            m_pBegin = m_pStrEnd = m_pEnd;
        else
            m_pBegin = m_pStrEnd + 1;
    }
    else
        pStrBegin = NULL;
    return pStrBegin;
}


StringList *StringTool::parseMatchPattern(const char *pPattern)
{
    char *pBegin;
    char ch;
    StringList *pList = new StringList();
    if (!pList)
        return NULL;
    char achBuf[2048];
    pBegin = achBuf;
    *pBegin++ = 0;
    while ((ch = *pPattern++))
    {
        switch (ch)
        {
        case '*':
        case '?':
            if (pBegin - 1 != achBuf)
                pList->add(achBuf, pBegin - achBuf);
            pBegin = achBuf;
            while (1)
            {
                *pBegin++ = ch;
                if (*pPattern != ch)
                    break;
                ++pPattern;

            }
            if (ch == '*')
                pList->add(achBuf, 1);
            else
                pList->add(achBuf, pBegin - achBuf);
            pBegin = achBuf;
            *pBegin++ = 0;
            break;

        case '\\':
            ch = *pPattern++;
        //fall through
        default:
            if (ch)
                *pBegin++ = ch;
            break;

        }
    }
    if (pBegin - 1 != achBuf)
        pList->add(achBuf, pBegin - achBuf);
    return pList;
}


//  return value:
//          0 : match
//         != 0: not match
//
int StringTool::strmatch(const char *pSrc, const char *pSrcEnd,
                         AutoStr2 *const *begin, AutoStr2 *const *end, int case_sens)
{
    if (!pSrcEnd)
        pSrcEnd = pSrc + strlen(pSrc);
    char ch;
    int c = -1;
    int len;
    const char *p = NULL;
    if ((!begin) || (!end))
        return -1;
    while ((begin < end) && (*((p = (*begin)->c_str())) != '*'))
    {
        len = (*begin)->len();
        if (!*p)
        {
            ++p;
            --len;
            if (len > pSrcEnd - pSrc)
                return -1;
            if (case_sens)
                c = strncmp(pSrc, p, len);
            else
                c = strncasecmp(pSrc, p, len);
            if (c)
                return 1;
        }
        else // must be '?'
        {
            if (len > pSrcEnd - pSrc)
                return -1;
        }
        pSrc += len;
        ++begin;
    }

    while ((begin < end) && (*(p = (*(end - 1))->c_str()) != '*'))
    {
        len = (*(end - 1))->len();
        if (!*p)
        {
            ++p;
            --len;
            if (len > pSrcEnd - pSrc)
                return -1;
            if (case_sens)
                c = strncmp(pSrcEnd - len, p, len);
            else
                c = strncasecmp(pSrcEnd - len, p, len);
            if (c)
                return 1;
        }
        else // must be '?'
        {
            if (len > pSrcEnd - pSrc)
                return -1;
        }
        pSrcEnd -= len;
        --end;
    }
    if (end - begin == 1)    //only a "*" left
        return 0;
    if (end - begin == 0)    //nothing left in pattern
        return pSrc != pSrcEnd;
    while (begin < end)
    {
        p = (*begin)->c_str();
        if ((*p != '*') && (*p != '?'))
            break;
        if (*p == '?')
            pSrc += (*begin)->len();
        ++begin;
    }
    if (end == begin)
        return pSrc != pSrcEnd;
    len = (*begin)->len() - 1;
    ch = *(++p);
    char search[4];
    if (!case_sens)
    {
        search[0] = tolower(ch);
        search[1] = toupper(ch);
        search[2] = 0;
    }
    while (pSrcEnd - pSrc >= len)
    {
        if (case_sens)
            pSrc = (const char *)memchr(pSrc, ch, pSrcEnd - pSrc);
        else
            pSrc = strpbrk(pSrc, search);
        if (!pSrc || (pSrcEnd - pSrc < len))
            return -1;
        int ret = strmatch(pSrc, pSrcEnd, begin, end, case_sens);
        if (ret != 1)
            return ret;
        ++pSrc;
    }
    return -1;
}


char *StringTool::convertMatchToReg(char *pStr, char *pBufEnd)
{
    char *pEnd = pStr + strlen(pStr);
    char *p = pStr;
    while (1)
    {
        switch (*p)
        {
        case 0:
            return pStr;
        case '?':
            *p = '.';
            break;
        case '*':
            if (pEnd == pBufEnd)
                return NULL;
            memmove(p + 1, p, ++pEnd - p);
            *p++ = '.';
            break;
        }
        ++p;
    }
    return pStr;
}


char *StringTool::memNextArg(char **s, int len, const char *pDelim,
                             int iDelimLen)
{
    const char *p = *s;
    char *p1 = (char *)memNextArg(&p, len, pDelim, iDelimLen);
    *s = (char *)p;
    return p1;
}


const char *StringTool::memNextArg(const char **s, int len,
                                   const char *pDelim, int iDelimLen)
{
    const char *p = *s;
    if (!pDelim)
    {
        pDelim = " \t\r\n";
        iDelimLen = 4;
    }
    if ((**s == '\"') || (**s == '\''))
    {
        char ch = *(*s)++;
        len--;
        while ((p = (const char *)memchr(p + 1, ch, len)) != NULL)
        {
            const char *p1 = p;
            while ((p1 > *s) && ('\\' == *(p1 - 1)))
                --p1;
            if ((p - p1) % 2 == 0)
                break;
        }
    }
    else
        p = mempbrk(*s, len, pDelim, iDelimLen);
    return p;
}


const char *StringTool::getLine(const char *pBegin, const char *pEnd)
{
    if (pEnd <= pBegin)
        return NULL;
    const char *p = (const char *)memchr(pBegin, '\n', pEnd - pBegin);
    if (!p)
        return pEnd;
    else
        return p;
}


int StringTool::parseNextArg(const char *&pRuleStr, const char *pEnd,
                             const char *&argBegin, const char *&argEnd, const char *&pError)
{
    int quoted = 0;
    while ((pRuleStr < pEnd) && (isspace(*pRuleStr)))
        ++pRuleStr;
    if ((*pRuleStr == '"') || (*pRuleStr == '\''))
    {
        quoted = *pRuleStr;
        ++pRuleStr;
    }
    argBegin = argEnd = pRuleStr;
    while (pRuleStr < pEnd)
    {
        char ch = *pRuleStr;
        if (ch == '\\')
            pRuleStr += 2;
        else if (((quoted) && (ch == quoted)) ||
                 ((!quoted) && (isspace(ch))))
        {
            argEnd = pRuleStr++;
            return 0;
        }
        else
            ++pRuleStr;
    }
    if (quoted)
    {
        argEnd = pEnd;
        return 0;
    }
    if (pRuleStr > pEnd)
    {
        pError = "pre-mature end of line";
        return -1;
    }
    argEnd = pRuleStr;
    return 0;
}


const char *StringTool::findCloseBracket(const char *pBegin,
        const char *pEnd, char chOpen, char chClose)
{
    int dep = 1;
    while (pBegin < pEnd)
    {
        char ch = *pBegin;
        if (ch == chOpen)
            ++dep;
        else if (ch == chClose)
        {
            --dep;
            if (dep == 0)
                break;
        }
        ++pBegin;
    }
    return pBegin;
}


const char *StringTool::findCharInBracket(const char *pBegin,
        const char *pEnd, char searched, char chOpen, char chClose)
{
    int dep = 1;
    while (pBegin < pEnd)
    {
        char ch = *pBegin;
        if (ch == chOpen)
            ++dep;
        else if (ch == chClose)
        {
            --dep;
            if (dep == 0)
                return NULL;
        }
        else if ((dep == 1) && (ch == searched))
            return pBegin;
        ++pBegin;
    }
    return NULL;
}



//void testBuildArgv()
//{
//    char cmd[] = "/path/to/cmd\targ1  arg2\targ3\t \t";
//    TPointerList<char> argvs;
//    char * pDir;
//    StringTool::buildArgv( cmd, &pDir, &argvs );
//    assert( strcmp( pDir, "/path/to" ) == 0 );
//    assert( argvs.size() == 5 );
//    assert( strcmp( argvs[0], "cmd" ) == 0 );
//    assert( strcmp( argvs[1], "arg1" ) == 0 );
//    assert( strcmp( argvs[2], "arg2" ) == 0 );
//    assert( strcmp( argvs[3], "arg3" ) == 0 );
//    assert( NULL == argvs[4] );
//}

int StringTool::unescapeQuote(char *pBegin, char *pEnd, int ch)
{
    int n = 0;
    char *p = pBegin;
    while (p < pEnd - 1)
    {
        p = (char *)memchr(p, '\\', pEnd - p - 1);
        if (!p)
            break;
        if (*(p + 1) == ch)
        {
            if (p != pBegin)
                memmove(pBegin + 1, pBegin, p - pBegin);
            ++n;
            ++pBegin;
            p = p + 1;
        }
        else
            p = p + 2;
    }
    return n;
}


const char *StringTool::lookupSubString(const char *p, const char *pEnd,
                                        const char *key,
                                        int keyLength, int *retLength, char sep, char comp)
{
    const char *ptr;
    *retLength = 0;
    if (keyLength <= 0)
        return NULL;
    while (p < pEnd)
    {
        while (*p == ' ' && (pEnd - p) >= keyLength)
            p++;
        if ((pEnd - p) < keyLength)
            return NULL;
        if (memcmp(p, key, keyLength) == 0)
        {
            ptr = p;
            p += keyLength;
            while (p < pEnd && *p == ' ')
                p++;
            if (p == pEnd || *p == sep)
            {
                if (comp == '\0')
                    return ptr;
                break;
            }
            if (*p == comp && comp != '\0')
            {
                p++;
                //check for spaces AFTER comparator
                while (p < pEnd && *p == ' ')
                    p++;
                if (p == pEnd)
                {
                    *retLength = 0;
                    return p;
                }
                if (*p == '\'' || *p == '"')
                {
                    ptr = (const char *)memchr(p + 1, *p, pEnd - p);
                    p++;
                    if (ptr == NULL)
                        ptr = pEnd;
                }
                else
                {
                    ptr = (const char *)memchr(p, sep, pEnd - p);
                    if (ptr == NULL)
                        ptr = pEnd;
                    while (ptr > p && *(ptr - 1) == ' ')
                        ptr--;
                }
                *retLength = ptr - p;
                return p;
            }
        }
        p = (const char *)memchr(p, sep, pEnd - p);
        if (p == NULL)
            return NULL;
        p++;
    }
    return NULL;
}


#define CACHE_SIZE 256
const char *StringTool::mempbrk(const char *pInput, int iSize,
                                const char *accept, int acceptLength)
{
    const char *p, *min = NULL, *pAcceptPtr = accept,
                             *pAcceptEnd = accept + acceptLength;
    while (iSize > 0)
    {
        while (pAcceptPtr < pAcceptEnd)
        {
            if ((p = (const char *)memchr(pInput, *pAcceptPtr,
                                          (iSize > CACHE_SIZE) ? CACHE_SIZE : iSize)) != NULL)
            {
                iSize = p - pInput;
                min = p;
            }
            ++pAcceptPtr;
        }
        if (min != NULL)
            return min;
        pInput += CACHE_SIZE;
        iSize -= CACHE_SIZE;
        pAcceptPtr = accept;
    }
    return NULL;
}


void *StringTool::memmem(const char *haystack, size_t haystacklen,
                         const char *needle, size_t needleLength)
{
    const char *p = haystack + haystacklen;

    if (haystacklen < needleLength)
        return NULL;

    if (needleLength == 0)
        return (void *)haystack;

    while (haystack < p
           && ((haystack = (const char *)memchr(haystack, *needle,
                           p - haystack)) != NULL))
    {
        if ((unsigned int)(p - haystack) < needleLength)
            break;
        if (memcmp(haystack, needle, needleLength) == 0)
            return (void *)haystack;
        haystack++;
    }
    return NULL;
}


int StringTool::memcasecmp(const char *s1, const char *s2, size_t len)
{
    int c1, c2;
    const char *pEnd;

    pEnd = s1 + len;
    while(s1 < pEnd)
    {
        c1 = *s1++;
        c2 = *s2++;
        if (c1 == c2)
            continue;
        c1 = tolower(c1);
        c2 = tolower(c2);
        if (c1 != c2)
            return c1 - c2;
    }
    return 0;
}


void *StringTool::strncasestr(const char *haystack, size_t haystacklen,
                              const char *needle, size_t needleLength)
{
    unsigned char chNeedle, chHaystack;
    const char *pHaystackEnd = haystack + haystacklen - (needleLength - 1);
    if ((haystacklen < needleLength) || (haystack == NULL))
        return NULL;

    if ((needleLength == 0) || (needle == NULL))
        return (void *)haystack;

    chNeedle = *needle++;
    chNeedle = tolower((unsigned char)chNeedle);
    --needleLength;
    pHaystackEnd = haystack + haystacklen - needleLength;

    do
    {
        do
        {
            if (haystack == pHaystackEnd)
                return NULL;
            chHaystack = *haystack++;
        } while ((char)tolower((unsigned char)chHaystack) != chNeedle);
    } while (memcasecmp(haystack, needle, needleLength) != 0);

    haystack--;
    return (void *)haystack;
}


size_t StringTool::memspn(const char *pInput, int inputSize,
                          const char *accept, int acceptLen)
{
    static const int NUM_CHAR = 256;
    char aAcceptArray[NUM_CHAR];
    memset(aAcceptArray, 0, NUM_CHAR);
    for (int i = 0; i < acceptLen; ++i)
        aAcceptArray[(unsigned char)accept[i] ] = 1;
    for (int i = 0; i < inputSize; ++i)
        if (aAcceptArray[(unsigned char)pInput[i] ] == 0)
            return i;
    return inputSize;
}


size_t StringTool::memcspn(const char *pInput, int iSize,
                           const char *reject, int rejectLen)
{
    const char *pEnd = mempbrk(pInput, iSize, reject, rejectLen);
    if (pEnd)
        return pEnd - pInput;
    else
        return iSize;
}


void StringTool::getMd5(const char *src, int len, unsigned char *dstBin)
{
    MD5Context ctx;
    MD5Init(&ctx);
    MD5Update(&ctx, (const unsigned char *)src, len);
    MD5Final(dstBin, &ctx);
}


long long StringTool::atoll(const char *pValue, int base)
{
    char *pEnd;
    long long v = strtoll(pValue, &pEnd, base);
    if (pEnd && *pEnd)
    {
        char ch = *pEnd;
        if (ch == 'G' || ch == 'g')
            return v * 1024 * 1024 * 1024;
        else if (ch == 'M' || ch == 'm')
            return v * 1024 * 1024;
        else if (ch == 'K' || ch == 'k')
            return v * 1024;
    }
    return v;
}

