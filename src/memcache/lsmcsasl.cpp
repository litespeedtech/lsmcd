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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sasl/saslplug.h>
#include <log4cxx/logger.h>


const char *LsMcSasl::s_pAppName = "memcached";
char       *LsMcSasl::s_pSaslPwdb = NULL;
uint8_t     LsMcSasl::verbose = 0;

#ifdef ENABLE_SASL_PWDB

#define PWENT_MAXLEN 256
static int chkSaslPwdb(sasl_conn_t *conn,
                       void *context,
                       const char *user,
                       const char *pass,
                       unsigned passlen,
                       struct propctx *propctx)
{
    size_t userlen = strlen(user);
    LS_DBG_M("chkSaslPwdb\n");
    
    if ((passlen + userlen) > (PWENT_MAXLEN - 4))
    {
        fprintf(stderr,
                "WARNING: Failed to authenticate <%s> due to too long password (%d)\n",
                user, passlen);
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
        return SASL_NOAUTHZ;
    }
    return SASL_OK;
}
#endif  // ENABLE_SASL_PWDB


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


#ifdef HAVE_SASL_CB_GETCONF
/* The locations we may search for a SASL config file if the user didn't
 * specify one in the environment variable SASL_CONF_PATH
 */
const char * const locations[] =
{
    "/etc/sasl",
    "/tmp",
    NULL
};

static int getSaslConf(void *context, const char **ppath)
{
    char *path = getenv("SASL_CONF_PATH");
    LS_DBG_M("getSASLConf\n");
    if (path == NULL)
    {
        const char * const *pp = locations;
        char fname[1024];
        while ((path = (char *)*pp++) != NULL)
        {
            snprintf(fname, sizeof(fname), "%s/%s.conf",
                     path, LsMcSasl::s_pAppName);
            LS_DBG_M("Trying path: %s\n", fname);
            if (access(fname, F_OK) == 0)
                break;
        }
    }
    if (LsMcSasl::verbose)
    {
        if (path != NULL)
            fprintf(stderr, "Reading configuration from: <%s>\n", path);
        else
            fprintf(stderr, "Failed to locate a config path\n");
    }
    // sasl.h says path must be allocated on heap, freed by library
    *ppath = ((path != NULL) ? strdup(path) : NULL);
    LS_DBG_M("   SASLConf: %s\n", (*ppath != NULL) ? *ppath : "NULL");
    return (*ppath != NULL) ? SASL_OK : SASL_FAIL;
}
#endif  // HAVE_SASL_CB_GETCONF


static sasl_callback_t saslCallbacks[] =
{
#ifdef ENABLE_SASL_PWDB
   { SASL_CB_SERVER_USERDB_CHECKPASS, (sasl_callback_ft)chkSaslPwdb, NULL },
#endif
#ifdef HAVE_SASL_CB_GETCONF
   { SASL_CB_GETCONFPATH, (sasl_callback_ft)getSaslConf, NULL },
#endif
   { SASL_CB_LIST_END, NULL, NULL }
};


int LsMcSasl::initSasl()
{
    LS_DBG_M("initSASL\n");
#ifdef ENABLE_SASL_PWDB
    if ((s_pSaslPwdb = getenv("MEMCACHED_SASL_PWDB")) == NULL)
    {
        LS_DBG_M("MEMCACHED_SASL_PWDB NOT DEFINED!\n");
        if (verbose)
        {
            fprintf(stderr,
                    "INFO: MEMCACHED_SASL_PWDB not specified. "
                    "Internal passwd database disabled\n");
        }
        saslCallbacks[0].id = SASL_CB_LIST_END;
        saslCallbacks[0].proc = NULL;
    }
#endif

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


int LsMcSasl::chkAuth(char *pBuf, unsigned int mechLen, unsigned int valLen,
    const char **pResult, unsigned int *pLen)
{
    LS_DBG_M("SASL chkAuth\n");
    if (getSaslConn() == NULL)
        return -1;
    int ret;
    char mech[SASLMECH_MAXLEN + 1];
    const char *pVal = ((valLen > 0) ? (pBuf + mechLen) : NULL);
    *pResult = NULL;
    *pLen = 0;
    ::memcpy(mech, pBuf, mechLen);
    mech[mechLen] = '\0';
    char sval[valLen + 1];
    memcpy(sval, pVal, valLen);
    sval[valLen] = 0;
    LS_DBG_M("SASL server_start, mech: %s, pval: %s\n", mech, sval);
    ret = sasl_server_start(m_pSaslConn, mech, pVal, valLen, pResult, pLen);
    if (ret == SASL_OK)
    {
        LS_DBG_M("SASL authenticated!\n");
        m_authenticated = true;
        char *user = NULL;
        if (sasl_getprop(m_pSaslConn, SASL_USERNAME, (const void **)&user) == SASL_OK)
            LS_DBG_M("SASL user: %s\n", user);
        else
            LS_DBG_M("ERROR getting username!\n");
        ret = 0;
    }
    else if (ret == SASL_CONTINUE)
    {
        LS_DBG_M("SASL_CONTINUE\n");
        ret = 1;
    }
    else
    {
        LS_ERROR("SASL Error in sasl_server_start: %d\n", ret);
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
        m_authenticated = true;
        char *user = NULL;
        if (sasl_getprop(m_pSaslConn, SASL_USERNAME, (const void **)&user) == SASL_OK)
            LS_DBG_M("SASL user: %s\n", user);
        else
            LS_DBG_M("ERROR getting username!\n");
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

