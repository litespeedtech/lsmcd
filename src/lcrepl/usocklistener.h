#ifndef USOCKLISTENER_H
#define USOCKLISTENER_H
#include <socket/gsockaddr.h>
#include <util/autostr.h>
#include <edio/eventreactor.h>
#include <util/ghash.h>
#include "usockconn.h"
class Multiplexer;
class UsockSvr;

#define  MAX_FORK           8

class UsocklListener : public EventReactor
{
    typedef THash< UsockSvr* > SvrMap_t;    
public: 
    UsocklListener();
    ~UsocklListener();
    virtual int handleEvents( short events );
    
    int Start(int procCnt);
    int Stop();

    Multiplexer * getMultiplexer() const    {   return m_pMultiplexer;   }
    void SetMultiplexer( Multiplexer * pMplx )  {   m_pMultiplexer = pMplx;  }
    
    int SetListenerAddr( const char * pAddr );
    int SetSockAttr( int fd );
    int forwardFd(const DispatchData_t &dispData , int fwFd); 
    void repldNotifyRole();
    void repldNotifyConn(int conn);
private:
    int                 m_procCnt;                     
    GSockAddr           m_lstnrAddr;
    Multiplexer         *m_pMultiplexer;
    SvrMap_t            m_svrMap;
};

#endif
