#ifndef REPLPACKET_H
#define REPLPACKET_H
#include "util/crc16.h"
#include <inttypes.h>

enum
{
    RT_DATA_UPDATE      = 1,
    RT_BULK_UPDATE      = 2,
    RT_ACK              = 3,
    RT_REJECT           = 4,
    RT_REPL_PING        = 5,
    RT_REPL_ECHO        = 6,
    RT_C2M_FRPEL_BOTHDONE = 7,
    
    RT_M2C_OUTSYNC,
    RT_C2M_UPDATE_CNT,    //tell master the latest container hash count      
    RT_M2C_FREPL_NEED,
    RT_C2M_FREPL_START,
    RT_C2M_FREPL_STEPREQ, //MD5HASH list, will invoke full repl
    RT_M2C_FREPL_STEPDONE,
    RT_C2M_FRPEL_CLNTDONE,
    RT_C2M_SELF_RTHASH,
    RT_M2C_BREPL_PUSHTM
};


class ReplPacketHeader
{
public:
    ReplPacketHeader();
    ~ReplPacketHeader()
    {}
    void initPack()
    {
        uiPackStart = ( uiPackStart & 0xFFFFFF ) | 0x12000000;
        uiSequenceNum = 0;
    }
    int checkPack()
    {
        if ( ( ( uiPackStart & 0x12000000 ) != 0x12000000 ) || ( !checkCRC16() ) )
            return -1;
        else
            return uiPackLen;
    }
    int checkCRC16()
    {
        uint16_t *pCrc = ( uint16_t* ) &uiPackStart;

        if ( *pCrc == crc16_ccitt ( ( ( char* ) &uiPackStart ) + 2, ( sizeof ( ReplPacketHeader ) - 2 ) ) )
            return 1;
        else
            return 0;
    }
    uint16_t calCRC16()
    {
        uint16_t *pCrc = ( uint16_t* ) &uiPackStart;
        *pCrc = crc16_ccitt ( ( ( char* ) &uiPackStart ) + 2, ( sizeof ( ReplPacketHeader ) - 2 ) );
        return *pCrc;
    }
    void setType ( uint32_t uiType )
    {
        ( ( char* ) &uiPackStart ) [2] = ( ( char* ) &uiType ) [0];
    }
    uint32_t getType()
    {
        uint32_t uiType = 0;
        ( ( char* ) &uiType ) [0] = ( ( char* ) &uiPackStart ) [2];
        return uiType;
    }

    void buildHeader ( uint32_t uiType, uint32_t iContID,  uint32_t uiSequenceNum, int iLength );
    int  unpack ( const char * pBuf, int bufLen );
    uint32_t getSequenceNum()
    {
        return uiSequenceNum;
    }
    void setSequenceNum ( uint32_t SequenceNum )
    {
        uiSequenceNum = SequenceNum;
    }
    uint32_t getPackLen()
    {
        return uiPackLen;
    }
    void setPackLen ( uint32_t PackLen )
    {
        uiPackLen = PackLen;
        calCRC16();
    }
    void setContID ( uint32_t ContID )
    {
        uiContID = ContID;
    }
    uint32_t getContID()
    {
        return uiContID;
    }

private:
    uint32_t    uiPackStart;
    uint32_t    uiPackLen;
    uint32_t    uiSequenceNum;
    uint32_t    uiContID;
    
};

#endif // REPLPACKET_H
