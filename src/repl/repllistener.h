

#ifndef SERVERLISTENER_H
#define SERVERLISTENER_H

#include <util/autostr.h>
#include <edio/eventreactor.h>

class GSockAddr;
class Multiplexer;
class ReplConn;

class ReplListener : public EventReactor
{
public: 
	ReplListener();
	~ReplListener();

    virtual int handleEvents( short events );
    
    int Start(int dupFd);
    int Stop();

    int SetListenerAddr( const char * pAddr );
    Multiplexer * getMultiplexer() const    {   return m_pMultiplexer;   }
    void SetMultiplexer( Multiplexer * pMplx )  {   m_pMultiplexer = pMplx;  }
    
    void RecycleConn( ReplConn * pConn );

    const char* getListenerAddr ();    
private:
    AutoStr         m_addrStr;   
    Multiplexer *   m_pMultiplexer;
    
    int SetSockAttr( int fd, GSockAddr &addr );

};

#endif
