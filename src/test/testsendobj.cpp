#include "testsendobj.h"
#include "replconn.h"
#include <sys/times.h>
static hash_key_t int_hash ( const void * p )
{
    return ( int ) (long)p;
}
static int  int_comp ( const void * pVal1, const void * pVal2 )
{
    //return ( int ) pVal1 - ( int ) pVal2;
    return ( int )(long)pVal1 - ( int )(long)pVal2;
}

int TestSendObj::m_SendObjlen;
char* TestSendObj::m_pSendObj;
sendList TestSendObj::m_Sendlist;
DataPktMap TestSendObj::m_DataPktMap ( 13, int_hash, int_comp );
uint32_t TestSendObj::m_uiSequenceNum = 0;
TestSendObj::TestSendObj()
{
}

TestSendObj::TestSendObj ( const TestSendObj& other )
{

}

TestSendObj::~TestSendObj()
{

}

TestSendObj& TestSendObj::operator= ( const TestSendObj& other )
{
    return *this;
}

bool TestSendObj::operator== ( const TestSendObj& other ) const
{
///TODO: return ...;
}
void TestSendObj::InitSendObj()
{
    m_pSendObj = NULL;
    m_SendObjlen = 0;
    m_Sendlist.clear();
}
void TestSendObj::testSend ( ReplConn * pConn )
{
    const char* mytest = "Hello-World";
    m_pSendObj = (char*)mytest;
    //pConn->SendEx( "Hello World", 11, 0 );
    pConn->SendEx ( m_pSendObj, 11, 0 );
    m_SendObjlen = 0;
}
int TestSendObj::AddSendObj ( char* SendObj )
{
    DataLstPkt* pDataLstPkt = new DataLstPkt;
    pDataLstPkt->DataPack = SendObj;
    m_Sendlist.push_front ( pDataLstPkt );
    return m_Sendlist.size();
}

int TestSendObj::AddDataSendObj ( char* SendObj, uint32_t uiSequenceNum )
{
    //need to Add Lock here
    DataLstPkt* pDataLstPkt = new DataLstPkt;
    pDataLstPkt->DataPack = SendObj;
    pDataLstPkt->uiSequenceNum = uiSequenceNum;
    m_Sendlist.push_back ( pDataLstPkt );
    return m_Sendlist.size();
}
int TestSendObj::RemoveDataSendObj ( uint32_t uiSequenceNum )
{
    //need to Add Lock here
    DataPktMap::iterator it = m_DataPktMap.find ( ( void* )( long )uiSequenceNum );

    if ( it != m_DataPktMap.end() )
    {
        //m_Sendlist.remove(it.second()->DataPack);
        delete [] ( uint32_t* ) ( it.second()->DataPack );
        delete it.second();
        m_DataPktMap.erase ( it );
    }

    return m_DataPktMap.size();
}
char* TestSendObj::GetSendObj_del ( int* ListSize )
{
    //need to Add Lock here
    int theSize = m_Sendlist.size();

    if ( ListSize )
    {
        *ListSize = theSize;

        if ( *ListSize > 0 )
            ( *ListSize )--;
    }

    if ( theSize > 0 )
    {
        DataLstPkt* pDataLstPkt = m_Sendlist.front();
        m_Sendlist.pop_front();
        char* SendObj = pDataLstPkt->DataPack;

        if ( pDataLstPkt->uiSequenceNum != 0 )
        {
            DataPkt* pDataPkt = new DataPkt;
            pDataPkt->DataPack = SendObj;
            pDataPkt->uiSendCount = 1 + pDataLstPkt->uiSendCount;
            tms tm;
            pDataPkt->uiSendTime = GetTickCount();
            m_DataPktMap.insert ( ( void* )( long )( pDataLstPkt->uiSequenceNum ), pDataPkt );
        }

        delete pDataLstPkt;
        return SendObj;
    }
    else
        return NULL;
}
uint32_t TestSendObj::GetSequenceNum()
{
    m_uiSequenceNum++;

    if ( m_uiSequenceNum == 0 )
        m_uiSequenceNum++;

    return m_uiSequenceNum;
}

int TestSendObj::MonitorDataPkt()
{
    uint32_t Curtime = GetTickCount();
    DataPktMap::iterator itn, it = m_DataPktMap.begin();

    for ( ; it != m_DataPktMap.end(); )
    {
        itn = m_DataPktMap.next ( it );

        if ( it.second()->uiSendCount >= 5 )
        {
            //Stop Sending this Data pky
            delete [] ( uint32_t* ) ( it.second()->DataPack );
            delete it.second();
            m_DataPktMap.erase ( it );
        }
        else
        {
            uint32_t Timespan = Curtime - it.second()->uiSendTime;

            if ( Timespan > 5000 )
            {
                //have Not received ACK for 5 second, resend the Data pack
                DataLstPkt* pDataLstPkt = new DataLstPkt;
                pDataLstPkt->DataPack = it.second()->DataPack;
                pDataLstPkt->uiSequenceNum = ( uint32_t ) (long) it.first();
                pDataLstPkt->uiSendCount = it.second()->uiSendCount;
                m_Sendlist.push_back ( pDataLstPkt );
                delete it.second();
                m_DataPktMap.erase ( it );
            }
        }

        it = itn;
    }

    return m_DataPktMap.size();
}

uint32_t TestSendObj::GetTickCount()
{
    tms tm;
    return ( times ( &tm ) * 1000 ) / CLOCKS_PER_SEC;
}


