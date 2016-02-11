#include <stdio.h>
#include <string.h>
#include "replpacker.h"

int ReplPacker::s_endian = ReplPacker::LITTLE;

void ReplPacker::detectEdian()
{
    int16_t Value = 1;
    char* p = ( char* ) &Value;

    if ( *p == 1 )
        s_endian = LITTLE;
    else
        s_endian = BIG;
}


ReplPacker::ReplPacker()
{

}

ReplPacker::ReplPacker ( const ReplPacker& other )
{

}

ReplPacker::~ReplPacker()
{

}

ReplPacker& ReplPacker::operator= ( const ReplPacker& other )
{
    return *this;
}


int ReplPacker::pack ( char * pBuf, int bufLen, char Value )
{
    if ( (unsigned)bufLen < sizeof ( char ) )
        return -1;

    * pBuf = Value;
    return sizeof ( char );
}

int ReplPacker::pack ( char * pBuf, int bufLen, uint8_t Value )
{
   return pack(pBuf, bufLen, *(char*)(&Value) );
}

int ReplPacker::pack ( char * pBuf, int bufLen, int16_t Value )
{
    if ( (unsigned)bufLen < sizeof ( int16_t ) )
        return -1;

    memcpy ( pBuf, &Value, sizeof ( int16_t ) );
    return sizeof ( int16_t );
}

int ReplPacker::pack ( char * pBuf, int bufLen, uint16_t Value )
{
    return pack(pBuf, bufLen, *(int16_t*)(&Value));     
}

int ReplPacker::pack ( char * pBuf, int bufLen, int32_t Value )
{
    if ( (unsigned)bufLen < sizeof ( int32_t ) )
        return -1;

    memcpy ( pBuf, &Value, sizeof ( int32_t ) );
    return sizeof ( int32_t );
}

int ReplPacker::pack ( char * pBuf, int bufLen, uint32_t Value )
{
    return pack(pBuf, bufLen, *(int32_t*)(&Value));     
}

int ReplPacker::pack ( char * pBuf, int bufLen, int64_t Value )
{
    if ( (unsigned)bufLen < sizeof ( int64_t ) )
        return -1;

    memcpy ( pBuf, &Value, sizeof ( int64_t ) );
    return sizeof ( int64_t );
}

int ReplPacker::pack ( char * pBuf, int bufLen, uint64_t Value )
{
    return pack(pBuf, bufLen, *(int64_t*)(&Value));     
}

int ReplPacker::pack ( char * pBuf, int bufLen, const char * pStr, int strLen )
{
    int32_t PackSize = strLen + sizeof ( int32_t ) + 1;

    if ( PackSize > bufLen )
        return -1;

    memcpy ( pBuf, &PackSize, sizeof ( int32_t ) );
    memcpy ( pBuf + sizeof ( int32_t ), pStr, bufLen );
    pBuf[sizeof ( int32_t ) + bufLen] = '\n';
    return PackSize;
}

int ReplPacker::packRawStr ( char * pBuf, int bufLen, const char * pStr, int strLen )
{
    if ( strLen > bufLen )
        return -1;
    memcpy ( pBuf, pStr, strLen );
    return strLen;
}

int ReplPacker::unpack ( char * pBuf, char& Value )
{
    Value = *pBuf;
    return sizeof ( char );
}

int ReplPacker::unpack ( char * pBuf, uint8_t& Value )
{
    Value = *pBuf;
    return sizeof ( uint8_t ); 
}

int ReplPacker::unpack ( char * pBuf, int16_t& Value )
{
    memcpy ( &Value, pBuf, sizeof ( int16_t ) );
    return sizeof ( int16_t );
}

int ReplPacker::unpack ( char * pBuf, uint16_t& Value )
{
    memcpy ( &Value, pBuf, sizeof ( uint16_t ) );
    return sizeof ( uint16_t );
}

int ReplPacker::unpack ( char * pBuf, int32_t& Value )
{
    memcpy ( &Value, pBuf, sizeof ( int32_t ) );
    return sizeof ( int32_t );
}

int ReplPacker::unpack ( char * pBuf, uint32_t& Value )
{
    memcpy ( &Value, pBuf, sizeof ( uint32_t ) );
    return sizeof ( uint32_t );
}

int ReplPacker::unpack ( char * pBuf, int64_t& Value )
{
    memcpy ( &Value, pBuf, sizeof ( int64_t ) );
    return sizeof ( int64_t );
}

int ReplPacker::unpack ( char * pBuf, uint64_t& Value )
{
    memcpy ( &Value, pBuf, sizeof ( uint64_t ) );
    return sizeof ( uint64_t );
}

int ReplPacker::unpack ( char * pBuf, char ** pStr, int strLen )
{
    int32_t PackSize;

    if ( strLen <= 0 )
        return -1;

    memcpy ( &PackSize, pBuf, sizeof ( int32_t ) );
    PackSize -= sizeof ( int32_t );

    if ( ( PackSize > strLen ) || ( PackSize < 1 ) )
        return -1;

    *pStr = new char[PackSize + 1];
    ( *pStr ) [PackSize] = 0;
    memcpy ( *pStr, pBuf + sizeof ( int32_t ), PackSize );
    return PackSize;
}

int ReplPacker::unpackRawStr ( char * pBuf, char ** pStr, int strLen )
{
    if ( strLen <= 0 )
        return -1;

    memcpy ( *pStr, pBuf , strLen );
    return strLen;
}
