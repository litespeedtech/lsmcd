#ifndef TESTSENDOBJ_H
#define TESTSENDOBJ_H
#include <sys/timeb.h>
#include <list>
#include <stddef.h>
#include <stdint.h>
#include <util/ghash.h>
struct DataPkt
{
    uint32_t    uiSendTime;
    uint32_t    uiSendCount;
    char*       DataPack;
};
struct DataLstPkt
{
    DataLstPkt()
    {
        uiSequenceNum = 0;
        uiSendCount = 0;
    }
    uint32_t    uiSequenceNum;
    uint32_t    uiSendCount;
    char*       DataPack;
};

//DO not use any STL class, use DLinkQueue
typedef std::list<DataLstPkt*> sendList;
typedef THash< DataPkt* > DataPktMap;


class ReplConn;



class TestSendObj
{
private:
    static int m_SendObjlen;
    static char* m_pSendObj;
    static sendList m_Sendlist;
    static DataPktMap m_DataPktMap;
    static uint32_t m_uiSequenceNum;
    TestSendObj();
    TestSendObj ( const TestSendObj& other );
    virtual ~TestSendObj();
    virtual TestSendObj& operator= ( const TestSendObj& other );
    virtual bool operator== ( const TestSendObj& other ) const;
public:
    static void InitSendObj();
    static void testSend ( ReplConn * pConn );
    static int AddSendObj ( char* SendObj );
    static int AddDataSendObj ( char* SendObj, uint32_t uiSequenceNum );
    static int RemoveDataSendObj ( uint32_t uiSequenceNum );
    static char* GetSendObj_del ( int* ListSize = NULL );
    static uint32_t GetSequenceNum();
    static int MonitorDataPkt();
    static uint32_t GetTickCount();
};
#endif // TESTSENDOBJ_H
