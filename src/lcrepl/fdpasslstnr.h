#ifndef FDPASSRLSTNR_H
#define FDPASSRLSTNR_H

#include <util/autostr.h>
#include <edio/eventreactor.h>
#include "usockconn.h"
class GSockAddr;
class Multiplexer;
class ReplConn;


class UsocklListener;
class FdPassLstnr : public EventReactor
{
public: 
    FdPassLstnr();
    ~FdPassLstnr();

    virtual int handleEvents( short events );
    
    int Start(int dupFd, UsocklListener*pLstnr);
    int Stop();

    int SetListenerAddr( const char * pAddr );
    Multiplexer * getMultiplexer() const    {   return m_pMultiplexer;   }
    void SetMultiplexer( Multiplexer * pMplx )  {   m_pMultiplexer = pMplx;  }
    
    const char* getListenerAddr ();    
private:
    AutoStr         m_addrStr;   
    Multiplexer *   m_pMultiplexer;
    
    int SetSockAttr( int fd, GSockAddr &addr );
    UsocklListener *m_pusockLstnr;

};

#endif
