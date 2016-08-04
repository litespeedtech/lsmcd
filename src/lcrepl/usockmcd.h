
#ifndef __USOCKMCD_H
#define __USOCKMCD_H

#include "usockconn.h"
#include <util/autostr.h>
#include <edio/eventreactor.h>
#include <socket/gsockaddr.h>
#include <util/hashstringmap.h>

#include <sys/stat.h>
#include <string>
#include "usockdata.h"
struct sockaddr;
class Multiplexer;
class MemcacheListener;
class MemcacheConn;
class UsockClnt : public UsockConn
{
    typedef THash< MemcacheConn* > ClntMap_t;    
public:
    UsockClnt();
    virtual ~UsockClnt();
    int connectTo(const char *pAddr);
    void cachedNotifyData(uint8_t procId, uint8_t sliceId);
protected:
    virtual void recycle();
    virtual int onRead();
    int  recvRoleChange();
    int  recvConnChange();
    int  recvPassingFd();
private:
    bool        m_bNotifyRepld;
    void acceptRepldFwFd(uint8_t sliceId, int fd, struct sockaddr * pSockAddr);
    ClntMap_t   m_clntMap;    
};

class UsockSvr : public UsockConn
{
public:
    UsockSvr();
    virtual ~UsockSvr();
    void setTidTracker(TidTracker *pTidTracker);
    int  svrInitConn( int fd, const AutoStr &remoteAddr  );
    int  sendPassingFd(const char *pData, int len, int fwFd);
    void initTidTracker(int sliceCnt);
   
protected:
    virtual void recycle();
    virtual int onRead();
    int  recvCachedNotify();
protected:
    static TidTracker     *m_pTidTracker ; //only exists in repld     
};


#endif
