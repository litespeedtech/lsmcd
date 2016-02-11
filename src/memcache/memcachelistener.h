

#ifndef MEMCACHELISTENER_H
#define MEMCACHELISTENER_H


#include <edio/eventreactor.h>
#include "util/autostr.h"

#include <string>

class GSockAddr;
class Multiplexer;
class MemcacheConn;

class MemcacheListener : public EventReactor
{
public: 
	MemcacheListener();
	~MemcacheListener();

    virtual int handleEvents( short events );
    
    int Start();
    int Stop();

    int SetListenerAddr( const char * pAddr );
    Multiplexer * getMultiplexer() const    {   return m_pMultiplexer;   }
    void SetMultiplexer( Multiplexer * pMplx )  {   m_pMultiplexer = pMplx;  }
    
    void RecycleConn( MemcacheConn * pConn );
    MemcacheConn * GetConn();
    
private:


    AutoStr     _AddrStr;
    Multiplexer *   m_pMultiplexer;
    
    int SetSockAttr( int fd, GSockAddr &addr );

};

#endif
