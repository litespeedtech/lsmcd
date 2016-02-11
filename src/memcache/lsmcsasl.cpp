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
    if (path == NULL)
    {
        const char * const *pp = locations;
        char fname[1024];
        while ((path = (char *)*pp++) != NULL)
        {
            snprintf(fname, sizeof(fname), "%s/%s.conf",
                     path, LsMcSasl::s_pAppName);
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
#ifdef ENABLE_SASL_PWDB
    if ((s_pSaslPwdb = getenv("MEMCACHED_SASL_PWDB")) == NULL)
    {
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

    if (sasl_server_init(saslCallbacks, s_pAppName) != SASL_OK)
    {
        fprintf(stderr, "Error initializing sasl.\n");
        return -1;
    }
    if (verbose)
        fprintf(stderr, "Initialized SASL.\n");
    return 0;
}


int LsMcSasl::listMechs(const char **pResult)
{
    unsigned int len;
    *pResult = NULL;
    return ((getSaslConn() == NULL)
        || (sasl_listmech(m_pSaslConn,
            NULL, "", " ", "", pResult, &len, NULL) != SASL_OK)) ?
        -1 : len;
}


int LsMcSasl::chkAuth(char *pBuf, unsigned int mechLen, unsigned int valLen,
    const char **pResult, unsigned int *pLen)
{
    if (getSaslConn() == NULL)
        return -1;
    int ret;
    char mech[SASLMECH_MAXLEN + 1];
    const char *pVal = ((valLen > 0) ? (pBuf + mechLen) : NULL);
    *pResult = NULL;
    *pLen = 0;
    ::memcpy(mech, pBuf, mechLen);
    mech[mechLen] = '\0';
    ret = sasl_server_start(m_pSaslConn, mech, pVal, valLen, pResult, pLen);
    if (ret == SASL_OK)
    {
        m_authenticated = true;
        ret = 0;
    }
    else if (ret == SASL_CONTINUE)
        ret = 1;
    else
        ret = -1;
    return ret;
}


int LsMcSasl::chkAuthStep(char *pBuf, unsigned int valLen,
    const char **pResult, unsigned int *pLen)
{
    if (getSaslConn() == NULL)
        return -1;
    int ret;
    const char *pVal = ((valLen > 0) ? pBuf : NULL);
    *pResult = NULL;
    *pLen = 0;
    ret = sasl_server_step(m_pSaslConn, pVal, valLen, pResult, pLen);
    if (ret == SASL_OK)
    {
        m_authenticated = true;
        ret = 0;
    }
    else if (ret == SASL_CONTINUE)
        ret = 1;
    else
        ret = -1;
    return ret;
}

#endif  // USE_SASL

