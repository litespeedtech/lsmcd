/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef ACCESSCONTROL_H
#define ACCESSCONTROL_H

#include <util/accessdef.h>

#include <util/ghash.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <inttypes.h>

/**
  *@author George Wang
  */

class AccessConfig
{
    short   m_access;
    short   m_maxConns;
    int32_t m_bandwidth;
public:
    explicit AccessConfig(int allowed)
        : m_access(allowed)
        , m_maxConns(AC_MAXCONN_UNSET)
        , m_bandwidth(AC_BANDWIDTH_UNSET)
    {}
    ~AccessConfig() {}
    short getAccess() const         {   return m_access;    }
    short getMaxConns() const       {   return m_maxConns;  }
    int32_t getBandwidth() const    {   return m_bandwidth; }

    void setAccess(int allow)     {   m_access = allow;   }
    void setMaxConns(int conns)   {   m_maxConns = conns; }
    void setBandwidth(int band)   {   m_bandwidth = band; }
};


class IPAccessControl : private THash<AccessConfig *>
{
public:
    explicit IPAccessControl(int initSize, GHash::hash_fn hf = NULL,
                             GHash::val_comp vc = NULL)
        : THash<AccessConfig * >(initSize, hf, vc)
    {}

    ~IPAccessControl()
    {   release_objects();   };

    int update(in_addr_t ip, long allow);

    void remove(in_addr_t ip);

    void add(in_addr_t ip, AccessConfig *pConf)
    {   insert((void *)(unsigned long)ip, pConf);     }

    AccessConfig *find(in_addr_t addr) const
    {
        iterator iter = GHash::find((void *)(unsigned long)addr);
        if (iter != end())
            return iter.second();
        return NULL;
    }


    void clear()    {   GHash::clear();   }
    size_t size() const {   return GHash::size();   }
};




class SubNetNode;
class SubNet6Node;
class IP6AccessControl;
class StringList;

class AccessControl
{
private:
    SubNetNode         *m_pRoot;
    IPAccessControl     m_ipCtrl;

    SubNet6Node        *m_pRoot6;
    IP6AccessControl   *m_pIp6Ctrl;

    StringList         *m_pEnvRules;

    int insSubNetControl(in_addr_t subNet,
                         in_addr_t mask,
                         int allowed);

    void removeIPControl(in_addr_t ip)
    {   m_ipCtrl.remove(ip);  }
    int addIPv4(const char *ip, int allowed);
    int parseNetmask(const char *netMask, int max, void *mask);

    int addSubNetControl(in_addr_t ip, in_addr_t netMask, int allowed);

    int addSubNetControl(const in6_addr &subNet,
                         const in6_addr &mask, int allowed);
    int insSubNetControl(const in6_addr &subNet,
                         const in6_addr &mask, int allowed);

public:
    AccessControl();
    ~AccessControl();

    const AccessConfig *getAccessConf(in_addr_t ip) const;
    const AccessConfig *getAccessConf(const in6_addr &ip) const;
    const AccessConfig *getAccessConf(const struct sockaddr *pAddr) const;
    const AccessConfig *getAccessConf(const char *pchIP) const;

    int hasAccess(in_addr_t ip) const
    {
        const AccessConfig *pConf = getAccessConf(ip);
        return pConf ? pConf->getAccess() : 0;
    }
    int hasAccess(const in6_addr &ip) const
    {
        const AccessConfig *pConf = getAccessConf(ip);
        return pConf ? pConf->getAccess() : 0;
    }
    int hasAccess(const struct sockaddr *pAddr) const
    {
        const AccessConfig *pConf = getAccessConf(pAddr);
        return pConf ? pConf->getAccess() : 0;

    }
    int hasAccess(const char *pchIP) const
    {
        const AccessConfig *pConf = getAccessConf(pchIP);
        return pConf ? pConf->getAccess() : 0;
    }


    int addIPControl(const in6_addr &ip, int allowed);
    int addIPControl(const char *pchIP, int allowed);
    int addIPControl(in_addr_t ip, int allowed)
    {   return m_ipCtrl.update(ip, allowed);     }
    void removeIPControl(const char *pchIP);


    int addSubNetControl(const char *ip, const char *netMask, int allowed);
    int addSubNetControl(const char *ip_mask, int allowed);
    void clear();
    int addList(const char *pList, int allow);
    int addList(const char *pList, int size, int allow);
    void addBandControl(char *pBegin, int len, int band);


    int addEnvRule(const char *pEnv, int len, int allow);
    StringList *getEnvRules() const    {   return m_pEnvRules;     }

    int getDefault() const;

};


#endif

