//
// C++ Interface: socketlistener
//
// Description:
//
//
// Author: George Wang <gwang@litespeedtech.com>, (C) 2011
//
// Copyright: See COPYING file that comes with this distribution
//
//
#ifndef SOCKETLISTENER_H
#define SOCKETLISTENER_H

#include <edio/eventreactor.h>
#include <util/autostr.h>
/**
    @author George Wang <gwang@litespeedtech.com>
*/
#include <inttypes.h>

class SockConnProcessor;
class GSockAddr;

#define SLCONN_BATCH_SIZE 10

class SocketListener : public EventReactor
{
    int addConns( struct conn_data * pBegin, struct conn_data *pEnd,
                  int *iCount );
public:
    SocketListener( const char * pName, const char *pAddr );
    SocketListener();
    ~SocketListener();
    void setName( const char * pName )  {   m_sName.setStr( pName );        }
    const char *getName() const         {   return m_sName.c_str();         }
    void setAddrStr( const char *pAddr )    {   m_sAddr.setStr( pAddr );    }
    const char * getAddrStr() const         {   return m_sAddr.c_str();     }
    void setBinding( uint64_t b )       {   m_iBinding = b;                 }
    uint64_t getBinding() const         {   return m_iBinding;              }
    int getType() const;
    void setConnProc( SockConnProcessor *pProcessor, int iType );
    SockConnProcessor *getConnProc() const;
    void clearConnProc();
    int connProcIsSet() const           {   return (m_pProcessor != NULL);  }

    int getPort() const;
    int setSockAttr( int fd, GSockAddr &addr );

    int assign( int fd, struct sockaddr *pAddr );
    int start();
    int handleEvents( short event );
    int stop();
private:

    AutoStr             m_sName;
    AutoStr             m_sAddr;
    int                 m_iPort;
    uint64_t            m_iBinding;
    SockConnProcessor  *m_pProcessor;
};


#endif
