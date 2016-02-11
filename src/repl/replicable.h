#ifndef REPLICABLE_H
#define REPLICABLE_H
#include "replpacker.h"
#include <sys/types.h>
#include <assert.h>
#include <stdlib.h>
#include "util/autobuf.h"
#include <util/refcounter.h>
#include <stdio.h>

class CountBuf : public AutoBuf, public RefCounter
{
public:
    CountBuf();
    CountBuf ( int nbuf, int n = 0 )
        : AutoBuf ( nbuf )
    {
        incRef ( n );
    }
};

class Replicable : public RefCounter
{
public:
    Replicable() :m_pPacket ( NULL ) {};
    Replicable ( const Replicable& other ) {};
    virtual ~Replicable();
    virtual int packObj ( int32_t id, char * pBuf, int bufLen ) = 0;
    virtual int packObj ( int32_t id, AutoBuf* pBuf ) = 0;
    virtual int unpackObj ( char * pBuf, int bufLen ) = 0;
    virtual const char * getKey ( int & keyLen ) = 0;
    virtual int getsize () = 0;

    void setPacketBuf ( CountBuf * pBuf );
    void CleanPacketBuf( );

    CountBuf * getPacketBuf() const
    {
        return m_pPacket;
    }
    
private:
    virtual Replicable& operator= ( const Replicable& other )
    {
        return *this;
    };
    virtual bool operator== ( const Replicable& other ) const
    {
        return true;
    };

    CountBuf * m_pPacket;
    
};

#endif // REPLICABLE_H
