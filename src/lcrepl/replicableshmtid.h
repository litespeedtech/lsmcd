#ifndef REPLICABLESHMTID_H
#define REPLICABLESHMTID_H

#include <repl/replicable.h>
#include <util/autobuf.h>
#include <util/gpointerlist.h>

class TidPacket
{
public:  
    uint32_t    m_iSize; 
    uint8_t     m_data[0];
};

class ReplicableShmTid : public Replicable
{
public:
    ReplicableShmTid();
    virtual ~ReplicableShmTid();
    int addTidPacket(TidPacket* pTidPacket);
    int getTidPacketNum();
    virtual int getsize ();
    virtual int packObj ( int32_t id, char * pBuf, int bufLen );
    virtual int unpackObj ( char * pBuf, int bufLen );
    virtual int packObj ( int32_t id, AutoBuf* pBuf );
    virtual const char * getKey ( int & keyLen );

private:
    TPointerList<TidPacket>  m_tidPacketList;
};

#endif 
