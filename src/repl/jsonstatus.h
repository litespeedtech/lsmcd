#ifndef JSONSTATUS_H
#define JSONSTATUS_H
#include "lsdef.h"
#include <util/dlinkqueue.h>

class AutoBuf;
class NodeInfo;

class JsonStatus
{
public:
    JsonStatus();
    virtual ~JsonStatus();
    void setNode(const char *ip, bool isLocal, bool isLeader, bool isSyncing, bool isSync
    , const NodeInfo *pNodeInfo);
    virtual void mkJson(AutoBuf &rAutoBuf) const;
    int  writeStatusFile(AutoBuf &rAutoBuf) const;
    void removeStatusFile() const;
protected:
    void mkJsonFirstUp(AutoBuf &rAutoBuf) const;
    void mkJsonStatus(AutoBuf &rAutoBuf) const;
    void mkJsonFRepl(AutoBuf &rAutoBuf) const;
    virtual void mkJsonContainers(AutoBuf &rAutoBuf) const = 0;
protected:
    const char         *m_ip;
    bool                m_isLocal;
    bool                m_isLeader;
    bool                m_isSyncing;
    bool                m_isSync;
    const NodeInfo     *m_pNodeInfo;
};

#endif
