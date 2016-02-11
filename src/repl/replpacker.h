#ifndef REPLPACKER_H
#define REPLPACKER_H

#include <inttypes.h>

class ReplPacker
{
private:
    ReplPacker();
    ~ReplPacker();
    ReplPacker ( const ReplPacker& other );
    virtual ReplPacker& operator= ( const ReplPacker& other );

    static int s_endian;

public:

    enum
    {
        LITTLE,
        BIG
    };

    static void detectEdian();

    static int pack ( char * pBuf, int bufLen, char Value );
    static int pack ( char * pBuf, int bufLen, int16_t Value );
    static int pack ( char * pBuf, int bufLen, int32_t Value );
    static int pack ( char * pBuf, int bufLen, int64_t Value );
    static int pack ( char * pBuf, int bufLen, const char * pStr, int strLen );
    static int packRawStr ( char * pBuf, int bufLen, const char * pStr, int strLen );
    
    static int pack ( char * pBuf, int bufLen, uint8_t Value );
    static int pack ( char * pBuf, int bufLen, uint16_t Value );
    static int pack ( char * pBuf, int bufLen, uint32_t Value );
    static int pack ( char * pBuf, int bufLen, uint64_t Value );
    
    static int unpack ( char * pBuf, char& Value );
    static int unpack ( char * pBuf, int16_t& Value );
    static int unpack ( char * pBuf, int32_t& Value );
    static int unpack ( char * pBuf, int64_t& Value );
    
    static int unpack ( char * pBuf, uint8_t& Value );
    static int unpack ( char * pBuf, uint16_t& Value );
    static int unpack ( char * pBuf, uint32_t& Value );
    static int unpack ( char * pBuf, uint64_t& Value );    
    
    static int unpack ( char * pBuf, char ** pStr, int strLen );
    static int unpackRawStr ( char * pBuf, char ** pStr, int strLen );
};

#endif // REPLPACKER_H
