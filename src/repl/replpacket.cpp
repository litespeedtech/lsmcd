#include "replpacket.h"
#include <string.h>
#include <stdio.h>

ReplPacketHeader::ReplPacketHeader()
    : uiPackStart(0)
    , uiPackLen(0)
    , uiSequenceNum(0)
    , uiContID(0)
{}

    
void ReplPacketHeader::buildHeader ( uint32_t uiType, uint32_t iContID,  uint32_t uiSequenceNum, int iLength )
{
    uiPackStart = ( uiPackStart & 0x0FFFF00FF ) | 0x00005600;
    setType ( uiType );
    uiSequenceNum = uiSequenceNum;
    uiPackLen = iLength + sizeof ( ReplPacketHeader );
    uiContID = iContID;
}

//return:
//      > 0 : consumed bytes
//      =0  : error
//      < 0 : needed bytes

int ReplPacketHeader::unpack ( const char * pBuf, int bufLen )
{
    if ( (unsigned)bufLen < sizeof ( ReplPacketHeader ) )
        return bufLen - sizeof ( ReplPacketHeader );

    memmove ( this, pBuf, sizeof ( ReplPacketHeader ) );
    int size = checkPack();

    if ( size < 0 )
        return 0;

    if ( bufLen < size )
        return bufLen - size;

    return size;
}

