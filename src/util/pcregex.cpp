/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include <util/pcregex.h>
#include <util/hashstringmap.h>
#include <util/stringtool.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <pthread.h>


static int s_use_jit = 0;

#if defined(PCRE_STUDY_JIT_COMPILE)
#if !defined(__sparc__) && !defined(__sparc64__)
static int s_jit_key_inited = 0;
static pthread_key_t s_jit_stack_key;

static void release_jit_stack(void *pValue)
{
    pcre_jit_stack_free((pcre_jit_stack *) pValue);
}


static void init_jit_stack()
{
    s_jit_key_inited = 1;
    pthread_key_create(&s_jit_stack_key, release_jit_stack);
}


static pcre_jit_stack *get_jit_stack(void *)
{
    pcre_jit_stack *jit_stack;

    if (!s_jit_key_inited)
        init_jit_stack();
    jit_stack = (pcre_jit_stack *)pthread_getspecific(s_jit_stack_key);
    if (!jit_stack)
    {
        jit_stack = (pcre_jit_stack *)pcre_jit_stack_alloc(32 * 1024, 512 * 1024);
        pthread_setspecific(s_jit_stack_key, jit_stack);
    }
    return jit_stack;
}
#endif
#endif


void Pcregex::enableJit()
{
#if defined(PCRE_STUDY_JIT_COMPILE)
#if !defined(__sparc__) && !defined(__sparc64__)
    s_use_jit = 1;
#endif
#endif
}



Pcregex::Pcregex()
    : m_type(0)
    , m_flag(0)
    , m_iSubStr(0)
    , m_regex(NULL)
    , m_extra(NULL)
{
}


Pcregex::~Pcregex()
{
    release();
}


void Pcregex::release()
{
    if ((m_type == REGEX_REGULAR) && (m_regex))
    {
        if (m_extra)
        {
#if defined(PCRE_STUDY_JIT_COMPILE)
            if (s_use_jit)
                pcre_free_study(m_extra);
            else
#endif
                pcre_free(m_extra);
        }
        pcre_free(m_regex);
        m_regex = NULL;
    }
    else if (m_str)
        delete m_str;
}


#define MAX_PCRE_PATTERN_LEN 0x1000
int Pcregex::compile(const char *regex, int options, int matchLimit,
                     int recursionLimit)
{
    const char *error;
    int         erroffset, len = -1;
    char        regex_type, pPattern[MAX_PCRE_PATTERN_LEN];

    if ((options & PCREGEX_OPT_ALWAYSREGEX) == 0)
    {
        if (options & REG_ICASE)
            m_flag |= PCREGEX_CI;
        len = isSimple(regex, strlen(regex), m_flag, pPattern,
                       MAX_PCRE_PATTERN_LEN, &regex_type);
    }

    if (len != -1)
    {
        m_type = getType(regex_type);
        m_flag |= (regex_type & (PCREGEX_MATCHBEGIN | PCREGEX_MATCHEND));
        m_str = new AutoStr2(pPattern, len);
        m_pattern.setStr(regex);
        return 0;
    }

    options &= ~PCREGEX_OPT_ALWAYSREGEX;
    m_type = REGEX_REGULAR;
    if (m_regex)
        release();
    m_regex = pcre_compile(regex, options, &error, &erroffset, NULL);
    if (m_regex == NULL)
        return -1;
    m_pattern.setStr(regex);
#if defined(PCRE_STUDY_JIT_COMPILE)
    if (s_use_jit)
        m_extra = pcre_study(m_regex, PCRE_STUDY_JIT_COMPILE, &error);
    else
#endif
        m_extra = pcre_study(m_regex, 0, &error);

    if (m_extra)
    {
#if defined(PCRE_STUDY_JIT_COMPILE)
        if (s_use_jit)
            pcre_assign_jit_stack(m_extra, get_jit_stack, NULL);
#endif
        if (matchLimit > 0)
        {
            m_extra->match_limit = matchLimit;
            m_extra->flags |= PCRE_EXTRA_MATCH_LIMIT;
        }
        if (recursionLimit > 0)
        {
            m_extra->match_limit_recursion = recursionLimit;
            m_extra->flags |= PCRE_EXTRA_MATCH_LIMIT_RECURSION;
        }
    }
    pcre_fullinfo(m_regex, m_extra, PCRE_INFO_CAPTURECOUNT, &len);
    m_iSubStr = len;
    ++m_iSubStr;
    return 0;
}


// int Pcregex::exec(const char *subject, int length, int startoffset,
//                   int options, int *ovector, int ovecsize) const
// {
//     int iLen;
//     const char *pInput = subject;
//     if ((ovector != NULL) && (ovecsize > 2))
//     {
//         ovector[0] = -1;
//         ovector[1] = -1;
//     }
//     if (m_type == REGEX_REGULAR)
//         return pcre_exec(m_regex, m_extra, subject, length, startoffset,
//                          options, ovector, ovecsize);
//     else if ((iLen = m_str->len()) > length)
//         return PCRE_ERROR_NOMATCH;
//     switch(m_type)
//     {
//     case REGEX_CONTAINS:
//         if (m_flag & PCREGEX_CI)
//             pInput = (const char *)StringTool::strncasestr(pInput, length,
//                                                         m_str->c_str(), iLen);
//         else
//             pInput = (const char *)StringTool::memmem(pInput, length,
//                                                       m_str->c_str(), iLen);
//         if (pInput == NULL)
//             return PCRE_ERROR_NOMATCH;
//         break;
//     case REGEX_ENDSWITH:
//         pInput += (length - iLen);
//         //fall through
//     case REGEX_BEGINSWITH:
//     case REGEX_STREQ:
//         if (((m_flag & PCREGEX_CI)
//                 && (strncasecmp(pInput, m_str->c_str(), iLen) != 0))
//             || (strncmp(pInput, m_str->c_str(), iLen) != 0))
//         {
//             return PCRE_ERROR_NOMATCH;
//         }
//         break;
//     case REGEX_BLANK:
//         if (length != 0)
//             return PCRE_ERROR_NOMATCH;
//         // fall through
//     case REGEX_ANYTHING:
//         break;
//     default:
//         return PCRE_ERROR_NOMATCH;
//     }
//
//     if ((ovector == NULL) || (ovecsize <= 2))
//         return 1;
//     if (m_flag & PCREGEX_MATCHBEGIN)
//         ovector[0] = 0;
//     else
//         ovector[0] = pInput - subject;
//
//     if (m_flag & PCREGEX_MATCHEND)
//         ovector[1] = length;
//     else
//         ovector[1] = ovector[0] + m_str->len();
//     return 1;
// }


int Pcregex::exec(const char *subject, int length, int startoffset,
                  int options, int *ovector, int ovecsize) const
{
    const char *pInput = subject + startoffset;
    int inputLen = length - startoffset;
//     if ((iLen = m_str->len()) > length)
//         return PCRE_ERROR_NOMATCH;
    switch(m_type)
    {
    case REGEX_REGULAR:
        return pcre_exec(m_regex, m_extra, subject, length, startoffset,
                         options, ovector, ovecsize);
    case REGEX_BLANK:
        if (inputLen != 0)
            return PCRE_ERROR_NOMATCH;
        //fall through
    case REGEX_ANYTHING:
        if (ovector && ovecsize >= 2)
        {
            ovector[0] = startoffset;
            ovector[1] = inputLen;
        }
        return 1;

    case REGEX_STREQ:
    case REGEX_ENDSWITH:
    case REGEX_BEGINSWITH:
        switch(m_type)
        {
        case REGEX_STREQ:
            if (inputLen != m_str->len())
                return PCRE_ERROR_NOMATCH;
            break;
        case REGEX_ENDSWITH:
            pInput += (inputLen - m_str->len());
            //fall through
        case REGEX_BEGINSWITH:
            if(inputLen < m_str->len())
                return PCRE_ERROR_NOMATCH;
            break;
        }
        if (m_flag & PCREGEX_CI)
        {
            if (StringTool::memcasecmp(pInput, m_str->c_str(), m_str->len()) != 0)
                return PCRE_ERROR_NOMATCH;
        }
        else if (memcmp(pInput, m_str->c_str(), m_str->len()) != 0)
            return PCRE_ERROR_NOMATCH;
        break;

    case REGEX_CONTAINS:
        if (m_flag & PCREGEX_CI)
            pInput = (const char *)StringTool::strncasestr(pInput, inputLen,
                                                        m_str->c_str(), m_str->len());
        else
            pInput = (const char *)StringTool::memmem(pInput, inputLen,
                                                      m_str->c_str(), m_str->len());
        if (pInput != NULL)
            break;
        //fall through
    default:
        return PCRE_ERROR_NOMATCH;
    }

    if (ovector && ovecsize >= 2)
    {
        if (m_flag & PCREGEX_MATCHBEGIN)
            ovector[0] = startoffset;
        else
            ovector[0] = pInput - subject;
        if (m_flag & PCREGEX_MATCHEND)
            ovector[1] = length - startoffset;
        else
            ovector[1] = ovector[0] + m_str->len();
    }
    return 1;
}


typedef HashStringMap<Pcregex *>  PcregexStore;
typedef THash<PcregexStore *>     PcregexStores;


static PcregexStores *s_pStores = NULL;
static int s_reused = 0;


const Pcregex *Pcregex::get(const char *pRegStr, int options, int matchLimit,
                 int recursionLimit)
{
    if (s_pStores == NULL)
        s_pStores = new PcregexStores(10, NULL, NULL);
    PcregexStores::iterator iterStore = s_pStores->find((void*)(long)options);
    PcregexStore *pStore;
    PcregexStore::iterator iter;
    if (iterStore != s_pStores->end())
    {
        pStore = iterStore.second();
        iter = pStore->find(pRegStr);
        if (iter != pStore->end())
        {
            ++s_reused;
            return iter.second();
        }
    }
    else
    {
        pStore = new PcregexStore(100);
        s_pStores->insert((void*)(long)options, pStore);
    }

    Pcregex * pRegex = new Pcregex();
    if (pRegex->compile( pRegStr, options, matchLimit, recursionLimit) == -1)
    {
        delete pRegex;
        pRegex = NULL;
    }
    else
    {
        pStore->insert(pRegex->getPattern(), pRegex);
    }

    return pRegex;
}


void Pcregex::releaseAll()
{
    if (!s_pStores)
        return;
    PcregexStores::iterator iterStore;
    for( iterStore = s_pStores->begin(); iterStore != s_pStores->end();)
    {
        iterStore.second()->release_objects();
        iterStore = s_pStores->next(iterStore);
    }
    s_pStores->release_objects();
    delete s_pStores;
    s_pStores = NULL;
}


int Pcregex::isSimple(const char *pInput, int inputSize, int iLowerCase,
                      char *pBuf, int iMaxBufLen, char *pType)
{
    const char *p = pInput, *pEnd = pInput + inputSize;
    char ch, *pSet = pBuf, *pSetEnd = NULL;
    if (pType)
        *pType = 0;

    if (pSet != NULL)
        pSetEnd = pBuf + iMaxBufLen;

    if (*p == '^')
    {
        *pType |= REGEX_BEGINSWITH;
        ++p;
    }
    if (pEnd - p >= 2 && *p == '.' && *(p + 1) == '*' )
    {
        p += 2;
        *pType &= ~REGEX_BEGINSWITH;
        *pType |= PCREGEX_MATCHBEGIN;
    }

    if (*(pEnd - 1) == '$')
    {
        *pType |= REGEX_ENDSWITH;
        --pEnd;
    }
    if (pEnd - p >= 2 && *(pEnd - 2) == '.' && *(pEnd - 1) == '*' )
    {
        pEnd -= 2;
        *pType &= ~REGEX_ENDSWITH;
        *pType |= PCREGEX_MATCHEND;
    }
    if (p == pEnd)
    {
        if (*pType == REGEX_STREQ)
            *pType = REGEX_BLANK;
        else
            *pType = (char)(unsigned int)(REGEX_ANYTHING | PCREGEX_MATCHEND);
        return 0;
    }

    while (p < pEnd)
    {
        ch = *p;
        switch (ch)
        {
        case '^': case '$': case '|': case '?':
        case '*': case '+': case '[': case '{':
            return -1;
        case '\\':
            if (++p == pEnd)
                return -1;
            ch = *p;
            switch (ch)
            {
            case '\\': case '^': case '$': case '.':
            case '|': case '?': case '*': case '+':
            case '(': case ')': case '[': case '{':
                break;
            default:
                return -1;
            }
            break;
        case '.':
            if (p + 1 == pEnd)
                return -1;
            ch = *(p + 1);
            switch (ch)
            {
            case '*':
            case '+':
            case '?':
                return -1;
            default:
                break;
            }
            ch = '.';
            break;
        case '(':
        case ')':
            return -1;
        case 'A': case 'B': case 'C': case 'D':
        case 'E': case 'F': case 'G': case 'H':
        case 'I': case 'J': case 'K': case 'L':
        case 'M': case 'N': case 'O': case 'P':
        case 'Q': case 'R': case 'S': case 'T':
        case 'U': case 'V': case 'W': case 'X':
        case 'Y': case 'Z':
            if (iLowerCase != 0)
                ch = 'a' + (ch - 'A');
            break;
        default:
            break;
        }
        if (pSet != NULL)
        {
            *pSet = ch;
            ++pSet;
            if (pSet >= pSetEnd)
                break;
        }
        ++p;
    }
    return (pSet != NULL ? pSet - pBuf : 1);
}




RegSub::RegSub()
    : m_pList(NULL)
    , m_pListEnd(NULL)
{}


RegSub::RegSub(const RegSub &rhs)
{
    m_parsed.setStr(rhs.m_parsed.c_str(),
                    (char *)rhs.m_pListEnd - rhs.m_parsed.c_str());
    m_pList = (RegSubEntry *)(m_parsed.c_str() +
                              ((char *)rhs.m_pList - rhs.m_parsed.c_str()));
    m_pListEnd = m_pList + (rhs.m_pListEnd - rhs.m_pList);
}


RegSub::~RegSub()
{
}


int RegSub::compile(const char *rule)
{
    if (!rule)
        return -1;
    const char *p = rule;
    char c;
    int entries = 0;
    while ((c = *p++) != '\0')
    {
        if (c == '&')
            ++entries;
        else if (c == '$' && isdigit(*p))
        {
            ++p;
            ++entries;
        }
        else if (c == '\\' && (*p == '$' || *p == '&'))
            ++p;
    }
    ++entries;
    int bufSize = strlen(rule) + 1;
    bufSize = ((bufSize + 7) >> 3) << 3;
    if (m_parsed.prealloc(bufSize + entries * sizeof(RegSubEntry)) == NULL)
        return -1;
    m_pList = (RegSubEntry *)(m_parsed.buf() + bufSize);
    memset(m_pList, 0xff, entries * sizeof(RegSubEntry));

    char *pDest = m_parsed.buf();
    p = rule;
    RegSubEntry *pEntry = m_pList;
    pEntry->m_strBegin = 0;
    pEntry->m_strLen = 0;
    while ((c = *p++) != '\0')
    {
        if (c == '&')
            pEntry->m_param = 0;
        else if (c == '$' && isdigit(*p))
            pEntry->m_param = *p++ - '0';
        else
        {
            if (c == '\\' && (*p == '$' || *p == '&'))
                c = *p++;
            *pDest++ = c;
            ++(pEntry->m_strLen);
            continue;
        }
        ++pEntry;
        pEntry->m_strBegin = pDest - m_parsed.buf();
        pEntry->m_strLen = 0;
    }
    *pDest = 0;
    if (pEntry->m_strLen == 0)
        --entries;
    else
        ++pEntry;
    m_pListEnd = pEntry;
    assert(pEntry - m_pList == entries);
    return 0;
}


int RegSub::exec(const char *input, const int *ovector, int ovec_num,
                 char *output, int &length)
{
    RegSubEntry *pEntry = m_pList;
    char *p = output;
    char *pBufEnd = output + length;
    while (pEntry < m_pListEnd)
    {
        if (pEntry->m_strLen > 0)
        {
            if (p + pEntry->m_strLen < pBufEnd)
                memmove(p, m_parsed.c_str() + pEntry->m_strBegin, pEntry->m_strLen);
            p += pEntry->m_strLen;
        }
        if ((pEntry->m_param >= 0) && (pEntry->m_param < ovec_num))
        {
            const int *pParam = ovector + (pEntry->m_param << 1);
            int len = *(pParam + 1) - *pParam;
            if (len > 0)
            {
                if (p + len < pBufEnd)
                    memmove(p, input + *pParam , len);
                p += len;
            }
        }
        ++pEntry;
    }
    if (p < pBufEnd)
        *p = 0;
    length = p - output;
    return (p < pBufEnd) ? 0 : -1;
}


int RegexResult::getSubstr(int i, char *&pValue) const
{
    if (i < m_matches)
    {
        const int *pParam = &m_ovector[ i << 1 ];
        pValue = (char *)m_pBuf + *pParam;
        return *(pParam + 1) - *pParam;
    }
    return 0;
}



