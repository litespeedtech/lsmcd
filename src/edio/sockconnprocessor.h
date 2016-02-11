//
// C++ Interface: sockconnprocessor
//
// Description:
//
//
// Author: Kevin Fwu <kfwu@litespeedtech.com>, (C) 2015
//
// Copyright: See COPYING file that comes with this distribution
//
//
#ifndef SOCKCONNPROCESSOR_H
#define SOCKCONNPROCESSOR_H

#include <cstddef>

class SocketListener;
class ClientInfo;
class GSockAddr;

struct conn_data
{
    int             fd;
    char            achPeerAddr[24];
    ClientInfo *    pInfo;
};

class SockConnProcessor
{
    SocketListener *m_pListener;
    int m_iType;

    SockConnProcessor( const SockConnProcessor &rhs );
    void operator=( const SockConnProcessor &rhs );
public:
    explicit SockConnProcessor(int type)
        : m_pListener( NULL )
        , m_iType(type)
    {}
    virtual ~SockConnProcessor() {}

    void setListener( SocketListener *pListener )
    {   m_pListener = pListener;    }
    SocketListener *getListener() const
    {   return m_pListener;         }
    void clearListener()
    {   m_pListener = NULL;         }

    int setName( const char *pName );
    const char *getName() const;
    int setBinding( unsigned int b );
    unsigned int getBinding() const;
    virtual void setAddrStr( const char * pAddr );
    virtual const char *getAddrStr() const;

    virtual int checkAccess( struct conn_data *pData ) = 0;
    virtual int addNewConn( struct conn_data *pData, int iFlag ) = 0;
    virtual void releaseUnusedConns( int iCount, void **pConns ) = 0;
    virtual void updateStats( int iCount);



    virtual int startListening( const GSockAddr &addr, int &fd );

    virtual int writeStatusReport( int fd ) = 0;
    int getType() const      {   return m_iType;     }

};

#endif


