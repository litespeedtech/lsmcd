#include "replicable.h"

Replicable::~Replicable()
{
    if ( m_pPacket != NULL )
    {
        m_pPacket->decRef();
        if ( m_pPacket->getRef() == 0 )
            delete m_pPacket;
    }
};


void Replicable::setPacketBuf ( CountBuf * pBuf )
{
    if ( m_pPacket == pBuf )
        return;
    if ( m_pPacket )
        m_pPacket->decRef();
    m_pPacket = pBuf;
    if ( pBuf )
        pBuf->incRef();
}


void Replicable::CleanPacketBuf( )
{
    if ( m_pPacket )
    {
        int keylen;
        if ( m_pPacket->getRef() <=1 )
        {
            delete m_pPacket;
            m_pPacket = NULL;
        }
        else
            printf ( "CleanPacketBuf--- Can not delete m_pPacket Ref=%d, key=%s\n", m_pPacket->getRef(), getKey ( keylen ) );
    }
}