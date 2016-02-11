/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include <util/env.h>

#include <stdio.h>
#include <stdlib.h>

void ArrayOfPointer::clear()
{
    iterator iter = begin();
    for (; iter != end(); ++iter)
    {
        if (*iter != NULL)
            free(*iter);
    }
    TPointerList<char>::clear();

}

int Env::add(const char *name, size_t nameLen, const char *value,
             size_t valLen)
{
    char *tmp;

    if (name && (value == 0))
        return 0;
    if ((nameLen > 1024) || (valLen > 65535))
        return 0;
    if (name == 0)
        push_back((char *) NULL);
    else
    {
        tmp = (char *)malloc(nameLen + 2 + valLen);
        if (tmp == 0)
            return -1;

        memcpy(tmp, name, nameLen);
        char *p = tmp + nameLen;
        *p++ = '=';
        memcpy(p, value, valLen);
        p += valLen;
        *p = 0;

        //snprintf( tmp, nameLen + 2 + valLen, "%s=%s", name, value );
        push_back(tmp);
    }
    return 0;
}

int Env::add(const char *pEnv)
{
    char *tmp;

    if (!pEnv)
        push_back((char *) NULL);
    else if (strchr(pEnv, '=') == NULL)
        return -1;
    else
    {
        tmp = strdup(pEnv);
        if (tmp == 0)
            return -1;
        push_back(tmp);
    }
    return 0;

}


const char *Env::find(const char *name) const
{
    if (!name)
        return NULL;
    int nameLen = strlen(name);
    for (const_iterator iter = begin(); iter != end(); ++iter)
    {
        if ((*iter) && strncmp(name, (*iter), nameLen) == 0)
        {
            const char *p = (*iter) + nameLen;
            while ((*p == ' ') || (*p == '\t'))
                ++p;
            if (*p == '=')
                return p + 1;
        }
    }
    return NULL;
}

void Env::remove(const char *name)
{
    if (!name)
        return;
    int nameLen = strlen(name);
    for (iterator iter = begin(); iter != end(); ++iter)
    {
        if ((*iter) && strncmp(name, (*iter), nameLen) == 0)
        {
            const char *p = (*iter) + nameLen;
            while ((*p == ' ') || (*p == '\t'))
                ++p;
            if (*p == '=')
            {
                free(*iter);
                erase(iter);
                break;
            }
        }
    }
    return;
}


int Env::update(const char *name, const char *value)
{
    if (!name)
        return 0;
    int nameLen = strlen(name);
    for (iterator iter = begin(); iter != end(); ++iter)
    {
        if ((*iter) && strncmp(name, (*iter), nameLen) == 0)
        {
            char *p = (*iter) + nameLen;
            while ((*p == ' ') || (*p == '\t'))
                ++p;
            if (*p == '=')
            {
                if ((value) && (strlen(p + 1) >= strlen(value)))
                {
                    strcpy(p + 1, value);
                    return 0;
                }
                else
                {
                    delete *iter;
                    erase(iter);
                    break;
                }
            }
        }
    }
    if (value)
        return add(name, value);
    return 0;
}


int Env::add(const Env *pEnv)
{
    for (const_iterator iter = pEnv->begin(); iter != pEnv->end(); ++iter)
        add(*iter);
    return 0;
}
