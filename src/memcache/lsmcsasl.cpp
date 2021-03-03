/*****************************************************************************
*    Open LiteSpeed is an open source HTTP server.                           *
*    Copyright (C) 2013 - 2015  LiteSpeed Technologies, Inc.                 *
*                                                                            *
*    This program is free software: you can redistribute it and/or modify    *
*    it under the terms of the GNU General Public License as published by    *
*    the Free Software Foundation, either version 3 of the License, or       *
*    (at your option) any later version.                                     *
*                                                                            *
*    This program is distributed in the hope that it will be useful,         *
*    but WITHOUT ANY WARRANTY; without even the implied warranty of          *
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the            *
*    GNU General Public License for more details.                            *
*                                                                            *
*    You should have received a copy of the GNU General Public License       *
*    along with this program. If not, see http://www.gnu.org/licenses/.      *
*****************************************************************************/
#include <memcache/lsmcsasl.h>

#ifdef USE_SASL
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <lcrepl/lcreplconf.h>
#include <sasl/saslplug.h>
#include <log4cxx/logger.h>
//#include <memcache/traceBuffer.h>

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 256
#endif

typedef int (*sasl_callback_ft_local)(void);

const char *LsMcSasl::s_pAppName = "memcached";
char       *LsMcSasl::s_pSaslPwdb = NULL;
uint8_t     LsMcSasl::verbose = 0;
char       *LsMcSasl::s_pHostName = NULL;

#define PWENT_MAXLEN 256
static int chkSaslPwdb(sasl_conn_t *conn,
                       void *context,
                       const char *user,
                       const char *pass,
                       unsigned passlen,
                       struct propctx *propctx)
{
    size_t userlen = strlen(user);
    LS_DBG_M("chkSaslPwdb user: %s\n", user);
    
    if ((passlen + userlen) > (PWENT_MAXLEN - 4))
    {
        fprintf(stderr,
                "WARNING: Failed to authenticate <%s> due to too long password (%d)\n",
                user, passlen);
        LS_WARN("WARNING: Failed to authenticate <%s> due to too long password (%d)\n",
                 user,passlen);
        return SASL_NOAUTHZ;
    }

    FILE *pwfile = fopen(LsMcSasl::s_pSaslPwdb, "r");
    if (pwfile == NULL)
    {
        if (LsMcSasl::verbose)
        {
            fprintf(stderr, "WARNING: Failed to open sasl database <%s>",
                    LsMcSasl::s_pSaslPwdb);
        }
        LS_WARN("WARNING: Failed to open sasl database <%s>", LsMcSasl::s_pSaslPwdb);
        return SASL_NOAUTHZ;
    }

    char buf[PWENT_MAXLEN];
    bool ok = false;
    while ((fgets(buf, sizeof(buf), pwfile)) != NULL)
    {
        char *p;
        if ((memcmp(user, buf, userlen) == 0) && (*(p = &buf[userlen]) == ':'))
        {
            if (memcmp(pass, ++p, passlen) == 0)
            {
                char c = p[passlen];
                ok = ((c == ':') || (c == '\n') || (c == '\r') || (c == '\0'));
            }
            break;
        }
    }
    (void)fclose(pwfile);
    if (!ok)
    {
        if (LsMcSasl::verbose)
            fprintf(stderr, "INFO: User <%s> failed to authenticate\n", user);
        LS_INFO("INFO: User <%s> failed to authenticate\n", user);
        return SASL_NOAUTHZ;
    }
    return SASL_OK;
}


sasl_conn_t *LsMcSasl::getSaslConn()
{
    //m_authenticated = false;
    LS_DBG_M("getSaslConn(), m_pSaslConn: %p\n", m_pSaslConn);
    if (m_pSaslConn)
        return m_pSaslConn;
    int result = sasl_server_new(s_pAppName, NULL, NULL, NULL, NULL, NULL, 0, 
                                 &m_pSaslConn);
    if (result == SASL_OK)
        LS_DBG_M("   sasl_server_new says ok, Ptr: %p\n", m_pSaslConn);
    else
        LS_DBG_M("   sasl_server_new return code: %d\n", result);
    return m_pSaslConn;
}


/* The locations we may search for a SASL config file if the user didn't
 * specify one in the environment variable SASL_CONF_PATH
 */
const char * const locations[] =
{
    "/usr/local/lsmcd/conf",
    "/etc/sasl2",
    "/etc/sasl",
    "/tmp",
    NULL
};

static int saslGetOpt(void *context __attribute__((unused)),
                      const char *plugin_name __attribute__((unused)),
                      const char *option,
                      const char **result,
                      unsigned *len)
{
    const char *path = getReplConf()->getSaslDB();

    if (path && !strcmp(option, "sasldb_path"))
    {
        *result = path;
        if (len)
            *len = (unsigned) strlen(path);
        LS_DBG_M("saslGetOpt using SASLDB: %s\n", path);
        return SASL_OK;
    }
    LS_DBG_M("saslGetOpt ignore option: %s\n", option);
    return SASL_FAIL;
}


static int sasl_log_callback(void *context, int level, const char *message)
{
    switch (level)
    {
        case SASL_LOG_ERR   :
            LS_ERROR("[SASL] %s\n", message);
            break;
        case SASL_LOG_FAIL  :
        case SASL_LOG_NOTE  :
            LS_NOTICE("[SASL] %s\n", message);
            break;
        case SASL_LOG_WARN  :
            LS_WARN("[SASL] %s\n", message);
            break;
        case SASL_LOG_DEBUG :
        case SASL_LOG_TRACE :        
        default :
            LS_DBG_M("[SASL] %s\n", message);
            break;
    }
    return SASL_OK;
}
        

static sasl_callback_t saslCallbacks[] =
{
   { SASL_CB_SERVER_USERDB_CHECKPASS, (sasl_callback_ft_local)chkSaslPwdb, NULL },
   { SASL_CB_LOG, (sasl_callback_ft_local)sasl_log_callback, NULL },
   { SASL_CB_LIST_END, NULL, NULL },
   { SASL_CB_LIST_END, NULL, NULL }
};


int LsMcSasl::initSasl()
{
    LS_DBG_M("initSASL\n");
    if ((s_pSaslPwdb = getenv("MEMCACHED_SASL_PWDB")) == NULL)
    {
        int lastIndex = 1;
        //LS_DBG_M("MEMCACHED_SASL_PWDB NOT DEFINED!\n");
        if (verbose)
        {
            fprintf(stderr,
                    "INFO: MEMCACHED_SASL_PWDB not specified. "
                    "Internal passwd database disabled\n");
        }
        saslCallbacks[0].id     = SASL_CB_LOG;
        saslCallbacks[0].proc   = (sasl_callback_ft_local)sasl_log_callback;
        if (getReplConf()->getSaslDB())
        {
            LS_DBG_M("Specified SASLDB: %s\n", getReplConf()->getSaslDB());
            saslCallbacks[lastIndex].id = SASL_CB_GETOPT;
            saslCallbacks[lastIndex].proc = (sasl_callback_ft_local)saslGetOpt;
            ++lastIndex;
        }
        saslCallbacks[lastIndex].id     = SASL_CB_LIST_END;
        saslCallbacks[lastIndex].proc   = NULL;
    }

    int result;
    result = sasl_server_init(saslCallbacks, s_pAppName);
    if (result != SASL_OK)
    {
        LS_DBG_M("Error initializing SASL: %d\n", result);
        fprintf(stderr, "Error initializing sasl: %d.\n", result);
        return -1;
    }
    if (verbose)
        fprintf(stderr, "Initialized SASL.\n");
    LS_DBG_M("Initialized SASL\n");
    return 0;
}


int LsMcSasl::listMechs(const char **pResult)
{
    *pResult = NULL;
    LS_DBG_M("SASL listMechs (only PLAIN for now)\n");
    if (getSaslConn() == NULL)
        LS_DBG_M("SASLConn is NULL, can't list Mechs\n");
    else
    {
        *pResult = "PLAIN";
        return 5;
    }
    return -1;
}


char *LsMcSasl::getHostName(void)
{
    if (s_pHostName)
        return s_pHostName;
    
    char hostname[HOST_NAME_MAX + 1];
    gethostname(hostname, sizeof(hostname));
    LsMcSasl::s_pHostName = strdup(hostname);
    if (!LsMcSasl::s_pHostName)
        LS_ERROR("Insufficient memory saving host name\n");
    char *pMixed = LsMcSasl::s_pHostName;
    while (*pMixed)
    {
        *pMixed = tolower(*pMixed);
        pMixed++;
    }
    
    return s_pHostName;
}


int LsMcSasl::rebuildAuth(char *pVal, unsigned int mechLen, unsigned int valLen, 
                          char *sval, char **ppVal, unsigned int *pValLen)
{
    /* The existing format for pVal is:
     *      - (preceded by non-NULL terminated mech when it's pBuf) 
     *      - NULL terminated user
     *      - NULL terminated user 
     *      - non-NULL terminated password.  
     * The user MUST include the realm or it will fail.  The doc says it's our 
     * responsibility to assure that.  Ok.  */
    LS_DBG_M("rebuildAuth for missing server name (realm)\n"); 
    unsigned int index = strlen(pVal) + 1;
    unsigned int counter = 1;
    const char *user1 = pVal;
    const char *user2 = NULL;
    const char *password = NULL;
    unsigned int password_len;
    while ((index < valLen) && (counter < 3))
    {
        //LS_DBG_M("   pval[%d]: %s\n", index, &pVal[index]);
        if (counter == 1)
            user2 = &pVal[index];
        else
            password = &pVal[index];
        counter++;
        if (counter == 3)
        {
            password_len = valLen - index;
            index = valLen;
        }
        else 
            index += strlen(&pVal[index]) + 1;
    }
    if (!user1[0])
    {
        LS_DBG_M("user1 is NULL, use user2 (%s)\n", user2);
        user1 = user2;
    }
    if (!user2[0])
    {
        LS_DBG_M("user2 is NULL, use user1 (%s)\n", user1);
        user2 = user1;
    }
    if ((!user1[0]) || (strcmp(user1, user2))) 
    {
        LS_ERROR("SASL User names not matching %s %s (%s)\n", user1, user2, password);
        return -1;
    }
    char *hostname = getHostName();
    if (!hostname)
        return -1;
    
    char full_username[HOST_NAME_MAX + valLen];
    snprintf(full_username, sizeof(full_username), "%s@%s", user1, hostname);
    LS_DBG_M("Converting user: %s to %s\n",
             user1, full_username);
    unsigned int full_username_len = strlen(full_username);
    strcpy(sval, full_username);
    index = full_username_len + 1;
    strcpy(&sval[index], full_username);
    index += full_username_len + 1;
    memcpy(&sval[index], password, password_len);
    index += password_len;
    valLen = index;
    *pValLen = valLen;
    *ppVal = sval;
    //index = 0;
    //while (index < valLen)
    //{
    //    LS_DBG_M("   val[%d]: %s\n", index, &sval[index]);
    //    index += strlen(&sval[index]) + 1;
    //}
    LS_DBG_M("pVal: %s, valLen: %d\n", *ppVal, valLen);
    return 0;
}


int LsMcSasl::chkAuth(char *pBuf, unsigned int mechLen, unsigned int valLen,
    const char **pResult, unsigned int *pLen)
{
    LS_DBG_M("SASL chkAuth, mechLen: %d, valLen: %d\n", mechLen, valLen);
    if (getSaslConn() == NULL)
        return -1;
    int ret;
    char mech[SASLMECH_MAXLEN + 1];
    if (valLen <= 0)
    {
        LS_ERROR("SASL valLen unexpected value: %d\n", valLen);
        return -1;
    }
    char *pVal = pBuf + mechLen;
    *pResult = NULL;
    *pLen = 0;
    ::memcpy(mech, pBuf, mechLen);
    mech[mechLen] = '\0';
    char sval[valLen + HOST_NAME_MAX + 1];
    LS_DBG_M("SASL chkAuth, mech: %s, pVal (user): %s\n", mech, pVal);
    //traceBuffer(pVal, valLen);
    /* Note this can happen if Python or some other language doesn't build the
     * authorization string the way it's expected to be */
    if (!strchr(pVal,'@'))
        if (rebuildAuth(pVal, mechLen, valLen, sval, &pVal, &valLen) == -1)
            return -1;
        
    LS_DBG_M("SASL server_start, mech: %s, pval: %s, valLen: %d, euid: %d\n",
             mech, pVal, valLen, geteuid());
    ret = sasl_server_start(m_pSaslConn, mech, pVal, valLen, pResult, pLen);
    if (ret == SASL_OK)
    {
        LS_DBG_M("SASL authenticated!\n");
        char *user = NULL;
        int sasl_err = sasl_getprop(m_pSaslConn, SASL_USERNAME,
                                    (const void **)&user);
        if (sasl_err == SASL_OK)
        {
            if (m_pUser && strcmp(m_pUser, user))
            {
                LS_DBG_M("SASL user CHANGED, was: %s\n", m_pUser);
                free(m_pUser);
            }
            LS_DBG_M("SASL user: %s\n", user);
            m_pUser = strdup(user);
            if (!m_pUser)
            {
                LS_ERROR("Insufficient memory creating SASL user name\n");
                return -1;
            }
        }
        else
        {
            LS_DBG_M("ERROR getting username #%d\n", ret);
            return -1;
        }
        m_authenticated = true;
        ret = 0;
    }
    else if (ret == SASL_CONTINUE)
    {
        LS_DBG_M("SASL_CONTINUE\n");
        ret = 1;
    }
    else
    {
        LS_ERROR("SASL Error in sasl_server_start: %s\n", 
                 sasl_errstring(ret,NULL,NULL));
        ret = -1;
    }
    return ret;
}


int LsMcSasl::chkAuthStep(char *pBuf, unsigned int valLen,
    const char **pResult, unsigned int *pLen)
{
    LS_DBG_M("SASL chkAuthStep\n");
    if (getSaslConn() == NULL)
    {
        LS_DBG_M("No SASL connection!\n");  
        return -1;
    }
    int ret;
    const char *pVal = ((valLen > 0) ? pBuf : NULL);
    *pResult = NULL;
    *pLen = 0;
    char sval[valLen + 1];
    memcpy(sval, pVal, valLen);
    sval[valLen] = 0;
    LS_DBG_M("SASL server_step, pval: %s\n", sval);
    ret = sasl_server_step(m_pSaslConn, pVal, valLen, pResult, pLen);
    if (ret == SASL_OK)
    {
        LS_DBG_M("SASL authenticated\n");
        char *user = NULL;
        int sasl_err = sasl_getprop(m_pSaslConn, SASL_USERNAME,
                                    (const void **)&user);
        if (sasl_err == SASL_OK)
        {
            if (m_pUser && strcmp(m_pUser, user))
            {
                LS_DBG_M("SASL user CHANGED, was: %s\n", m_pUser);
                free(m_pUser);
            }
            m_pUser = strdup(user);
            if (!m_pUser)
            {
                LS_ERROR("Insufficient memory creating SASL user name\n");
                return -1;
            }
            LS_DBG_M("SASL user: %s\n", user);
        }
        else
        {
            LS_ERROR("ERROR getting username %d\n", sasl_err);
            return -1;
        }
        m_authenticated = true;
        ret = 0;
    }
    else if (ret == SASL_CONTINUE)
    {
        LS_DBG_M("SASL_CONTINUE\n");
        ret = 1;
    }
    else
    {
        LS_ERROR("SASL Error in sasl_server_step: %d\n", ret);
        ret = -1;
    }
    return ret;
}

#endif  // USE_SASL

