#include "testreplpack.h"
#include "replshmtid.h"
#include "replshmtidcontainer.h"
#include "repl/replicable.h"
#include <replicate/replcontainer.h>
#include <replicate/replregistry.h>
#include "replicate/replpacket.h"
#include "replicate/replhashstrcontainer.h"
#include "replicate/replsender.h"
#include "replicate/replhashstrnodelist.h"
#include "replicate/repllog.h"
#include "replicate/replpendinglist.h"
#include <replicate/replshmhelper.h>

#include "testsendobj.h"
#include "test/testpackuser.h"
#include "memcache/lsmemcache.h"

#include <string.h>
#include <iostream>
#include <stdio.h>
bool TestReplPack::IsReady = false;
TestReplPack::TestReplPack()
{

}

TestReplPack::TestReplPack ( const TestReplPack& other )
{

}

TestReplPack::~TestReplPack()
{

}

TestReplPack& TestReplPack::operator= ( const TestReplPack& other )
{
    return *this;
}

bool TestReplPack::operator== ( const TestReplPack& other ) const
{
///TODO: return ...;
}


bool TestReplPack::initReplRegistry()
{
    TReplicableAllocator<ReplicableShmTid> * pAllocator1 = new TReplicableAllocator<ReplicableShmTid>();

    ReplRegistry& RegObj = ReplRegistry::getInstance();
    ReplShmTidContainer * pStrContainer1 = new ReplShmTidContainer (
        1, pAllocator1 );
    RegObj.add ( 1, pStrContainer1 );
    
    uint64_t tid = ReplShmHelper::getInstance().readShmDBTid();
    pStrContainer1->setCurrTid(tid);
    ReplShmHelper::getInstance().tidGetDelItems(0, tid);
    LS_DBG_M(  "Finished---bool TestReplPack::initReplRegistry(), latest tid:%d \n", tid );
    return true;
}


bool TestReplPack::sendReplPackTo ( const char * name, ReplConn * pConn )
{
    ReplPeer * pReplNode = GetNode ( name, pConn );

    if ( pReplNode == NULL )
        return false;

    //Create  Replicable objs
    TestPackString* pstr11 = new TestPackString ( "This is string ID1 test" );
    TestPackUser* pUser21 = new TestPackUser ( "Yuan S. Feng", 25 );
    char* pJohnProfile = new char[15000];
    int iProfilelen = 15000;
    memset ( pJohnProfile, 0, iProfilelen );
    strcpy ( pJohnProfile, "This is John's profile" );
    TestPackUser1* pUser31 = new TestPackUser1 ( "John Na", 55, pJohnProfile, iProfilelen );
    LS_DBG_M(  "Finished---Create  Replicable objs \n" );
    ReplSender& SendObj = ReplSender::getInstance ( );
    SendObj.sendReplPack ( 1, pstr11, pReplNode, RT_DATA );
    LS_DBG_M(  "Finished---SendReplPack(1, pstr11, pReplNode = %s) \n", pReplNode->getNodeName() );
    SendObj.sendReplPack ( 2, pUser21, pReplNode, RT_DATA );
    LS_DBG_M(  "Finished---SendReplPack(2, pUser21, pReplNode = %s) \n", pReplNode->getNodeName() );
    SendObj.sendReplPack ( 3, pUser31, pReplNode, RT_DATA );
    LS_DBG_M(  "Finished---SendReplPack(3, pUser31, pReplNode = %s) pJohnProfile=%s\n",
                 pReplNode->getNodeName() , pJohnProfile );
    SendObj.sendReplPack ( 3, pUser31, pReplNode, RT_DATA );
    //delete pstr11;
    //delete pUser21;
    //delete pUser31;
    return true;
}


bool TestReplPack::CreateObj_ReplicateContainerTo ( char * name, ReplConn * pConn )
{
    ReplPeer * pReplNode = GetNode ( name, pConn );

    if ( pReplNode == NULL )
        return false;

    TestPackUser* pUser22 = new TestPackUser ( "Yuan S. Feng22", 25 );
    TestPackUser* pUser23 = new TestPackUser ( "Yuan S. Fing23", 26 );
    TestPackUser* pUser24 = new TestPackUser ( "Yuan S. Yong24", 27 );
    TestPackUser* pUser25 = new TestPackUser ( "Yuan S. Ming25", 28 );
    ReplRegistry& RegObj = ReplRegistry::getInstance();
    ReplHashStrContainer* pReplHashStrContainer2 = ( ReplHashStrContainer* ) RegObj.get ( 2 );

    if ( pReplHashStrContainer2 == NULL )
        return false;

    pReplHashStrContainer2->addAndUpdateObj ( pUser22 );
    pReplHashStrContainer2->addAndUpdateObj ( pUser23 );

    if ( !pReplHashStrContainer2->createReplObjList() )
        return false;

    LS_DBG_M(  "TestReplPack::CreateObj_ReplicateContainerTo---ListSize=%d, addr=%s\n",
                 pReplHashStrContainer2->getReplObjListSize(), pReplNode->getNodeName() );
    ReplSender& SendObj = ReplSender::getInstance ( );
    SendObj.sendReplPack ( 2, pReplHashStrContainer2->getNextObjFromList(), pReplNode );
    pReplHashStrContainer2->addAndUpdateObj ( pUser24 );
    pReplHashStrContainer2->addAndUpdateObj ( pUser25 );
    Replicable * pReplicable = pReplHashStrContainer2->getNextObjFromList();

    while ( pReplicable != NULL )
    {
        SendObj.sendReplPack ( 2, pReplicable, pReplNode );
        pReplicable = pReplHashStrContainer2->getNextObjFromList();
    }

    return true;
}
bool TestReplPack::AddObjToContainer ( )
{
    ReplRegistry& RegObj = ReplRegistry::getInstance();
    ReplHashStrContainer* pReplHashStrContainer2 = ( ReplHashStrContainer* ) RegObj.get ( 2 );

    if ( pReplHashStrContainer2 == NULL )
        return false;

    replHashStrNodeList& NodeList = replHashStrNodeList::getInstance ();
    DLinkQueue theNodeQueue;
    NodeList.getNodeList ( theNodeQueue );
    pReplHashStrContainer2->addNodeList ( &theNodeQueue );
    TestPackUser* pUser26 = new TestPackUser ( "Yuan S. Feng26", 35 );
    TestPackUser* pUser27 = new TestPackUser ( "Yuan S. Fing27", 36 );
    TestPackUser* pUser28 = new TestPackUser ( "Yuan S. Yong28", 37 );
    TestPackUser* pUser29 = new TestPackUser ( "Yuan S. Ming29", 38 );
    pReplHashStrContainer2->addAndUpdateObj ( pUser26 );
    pReplHashStrContainer2->addAndUpdateObj ( pUser27 );
    pReplHashStrContainer2->addAndUpdateObj ( pUser28 );
    pReplHashStrContainer2->addAndUpdateObj ( pUser29 );
    //pReplHashStrContainer2->GetKeyStrList ();
    //pReplHashStrContainer2->removeNodeList();
    return true;
}


bool TestReplPack::AddShmTidObjToContainer (ReplicableShmTid* pReplicableShmTid, uint64_t tid)
{
    ReplRegistry& RegObj = ReplRegistry::getInstance();
    ReplShmTidContainer* pReplShmTidContainer = ( ReplShmTidContainer* ) RegObj.get ( 1 );

    if ( pReplShmTidContainer == NULL )
        return false;

    //pReplHashStrContainer2->setCurrTid(tid);
    replHashStrNodeList& NodeList = replHashStrNodeList::getInstance ();
    DLinkQueue theNodeQueue;
    NodeList.getNodeList ( theNodeQueue );
    pReplShmTidContainer->addNodeList ( &theNodeQueue );
    pReplShmTidContainer->addAndUpdateObj ( pReplicableShmTid );
}


bool TestReplPack::removeShmTidObjToContainer (ReplicableShmTid* pReplicableShmTid)
{
    ReplRegistry& RegObj = ReplRegistry::getInstance();
    ReplShmTidContainer* pReplShmTidContainer = ( ReplShmTidContainer* ) RegObj.get ( 1 );

    if ( pReplShmTidContainer == NULL )
        return false;

    //pReplHashStrContainer2->setCurrTid(tid);
    replHashStrNodeList& NodeList = replHashStrNodeList::getInstance ();
    DLinkQueue theNodeQueue;
    NodeList.getNodeList ( theNodeQueue );
    pReplShmTidContainer->addNodeList ( &theNodeQueue );
    pReplShmTidContainer->removeObj ( (char*)pReplicableShmTid, pReplicableShmTid->getsize() );
}


bool TestReplPack::RemoveObjFromContainer ( )
{
    ReplRegistry& RegObj = ReplRegistry::getInstance();
    ReplHashStrContainer* pReplHashStrContainer2 = ( ReplHashStrContainer* ) RegObj.get ( 2 );

    if ( pReplHashStrContainer2 == NULL )
        return false;

    replHashStrNodeList& NodeList = replHashStrNodeList::getInstance ();
    DLinkQueue theNodeQueue;
    NodeList.getNodeList ( theNodeQueue );
    pReplHashStrContainer2->addNodeList ( &theNodeQueue );
    Replicable* pUser26 = pReplHashStrContainer2->removeObj ( "Yuan S. Feng26", 0 );
    Replicable* pUser27 = pReplHashStrContainer2->removeObj ( "Yuan S. Fing27", 0 );
    Replicable* pUser28 = pReplHashStrContainer2->removeObj ( "Yuan S. Yong28", 0 );
    Replicable* pUser29 = pReplHashStrContainer2->removeObj ( "Yuan S. Ming29", 0 );
    pReplHashStrContainer2->removeNodeList();
    pReplHashStrContainer2->addAndUpdateObj ( pUser26 );
    pReplHashStrContainer2->addAndUpdateObj ( pUser27 );
    pReplHashStrContainer2->addAndUpdateObj ( pUser28 );
    pReplHashStrContainer2->addAndUpdateObj ( pUser29 );
    StringList theStringList;
    theStringList.add ( "Yuan S. Feng26" );
    theStringList.add ( "Yuan S. Fing27" );
    theStringList.add ( "Yuan S. Yong28" );
    theStringList.add ( "Yuan S. Ming29" );
    ReplSender& SendObj = ReplSender::getInstance ( );
    SendObj.sendMutiReplPack ( 2, &theStringList, &theNodeQueue, 70 );
    return true;
}
bool TestReplPack::ReplicateContainerTo ( char * name, ReplConn * pConn, bool update )
{
    ReplPeer * pReplNode = GetNode ( name, pConn );

    if ( pReplNode == NULL )
        return false;

    ReplRegistry& RegObj = ReplRegistry::getInstance();
    ReplHashStrContainer* pReplHashStrContainer2 = ( ReplHashStrContainer* ) RegObj.get ( 2 );

    if ( pReplHashStrContainer2 == NULL )
        return false;

    if ( !pReplHashStrContainer2->createReplObjList() )
        return false;

    LS_DBG_M(  "TestReplPack::ReplicateContainerTo---ListSize=%d, addr=%s\n",
                 pReplHashStrContainer2->getReplObjListSize(), pReplNode->getNodeName() );
    ReplSender& SendObj = ReplSender::getInstance ( );
    bool IsUpdate;
    Replicable * pReplicable = pReplHashStrContainer2->getNextObjFromList();
    int SendSize = 0;

    while ( pReplicable != NULL )
    {
        if ( update )
            SendObj.sendReplPack ( 2, pReplicable, pReplNode, RT_DATA_UPDATE );
        else
            SendObj.sendReplPack ( 2, pReplicable, pReplNode, RT_DATA );
        pReplicable = pReplHashStrContainer2->getNextObjFromList();
        SendSize ++;
    }

    printf ( "TestReplPack::ReplicateContainerTo SendSize=%d\n", SendSize );
    return true;
}

bool TestReplPack::ReplicateContainerToM ( int32_t MaxSize )
{
    StringList theStringList;

    ReplRegistry& RegObj = ReplRegistry::getInstance();
    ReplHashStrContainer* pReplHashStrContainer2 = ( ReplHashStrContainer* ) RegObj.get ( 2 );

    if ( pReplHashStrContainer2 == NULL )
        return false;

    int ListSize = pReplHashStrContainer2->getKeyStrList ( theStringList );

    if ( ListSize <= 0 )
        return true;

    replHashStrNodeList& NodeList = replHashStrNodeList::getInstance ();
    DLinkQueue theNodeQueue;
    NodeList.getNodeList ( theNodeQueue );
    ReplSender& SendObj = ReplSender::getInstance ( );
    int ProcessedNum = 0;

    for ( ; theStringList.size() > 0; )
    {
        ProcessedNum = SendObj.sendMutiReplPack ( 2, &theStringList, &theNodeQueue, MaxSize );
        theStringList.removeFromFront ( ProcessedNum );
    }

    return true;
}
bool TestReplPack::getReplPackFrom ( const char * name, ReplConn * pConn )
{

    ReplPeer * pReplNode = GetNode ( name, pConn );

    if ( pReplNode == NULL )
        return false;

    ReplSender& SendObj = ReplSender::getInstance ( );
    /*SendObj.sendReplQuery ( 1, "This is string ID1 test", pReplNode );
    LS_DBG_M(  "Finished---SendReplQuery(1, --This is string ID1 test--, pReplNode = %s) \n", pReplNode->getNodeName() ) );
    SendObj.sendReplQuery ( 2, "Yuan S. Feng", pReplNode );
    LS_DBG_M(  "Finished---SendReplQuery(2, --Yuan S. Feng--, pReplNode = %s) \n", pReplNode->getNodeName() ) );
    SendObj.sendReplQuery ( 3, "John Na", pReplNode );
    LS_DBG_M(  "Finished---SendReplQuery(3, --John Na--, pReplNode = %s) \n", pReplNode->getNodeName() ) );
    */
    return true;
}

bool TestReplPack::getReplPackFromList()
{
    replHashStrNodeList& NodeList = replHashStrNodeList::getInstance ();
    DLinkQueue theNodeQueue;
    NodeList.getNodeList ( theNodeQueue );

    if ( theNodeQueue.empty() )
        return false;

    ReplSender& SendObj = ReplSender::getInstance ( );
    /*SendObj.sendReplQueryToList ( 1, "This is string ID1 test", &theNodeQueue );
    SendObj.sendReplQueryToList ( 3, "John Na", &theNodeQueue );
    SendObj.sendReplQueryToList ( 2, "Yuan S. Feng", &theNodeQueue );
    */
    return true;
}

bool TestReplPack::DeleteReplPackFrom ( const char * name, ReplConn * pConn )
{

    ReplPeer * pReplNode = GetNode ( name, pConn );

    if ( pReplNode == NULL )
        return false;

    ReplSender& SendObj = ReplSender::getInstance ( );
    /*SendObj.sendReplDelete ( 1, "This is string ID1 test", pReplNode );
    LS_DBG_M(  "Finished---SendReplQuery(1, --This is string ID1 test--, pReplNode = %s) \n",
                 pReplNode->getNodeName() ) );
    SendObj.sendReplDelete ( 2, "Yuan S. Feng", pReplNode );
    LS_DBG_M(  "Finished---SendReplQuery(2, --Yuan S. Feng--, pReplNode = %s) \n", pReplNode->getNodeName() ) );
    SendObj.sendReplDelete ( 3, "John Na", pReplNode );
    LS_DBG_M(  "Finished---SendReplQuery(3, --John Na--, pReplNode = %s) \n", pReplNode->getNodeName() ) );
    */return true;
}
bool TestReplPack::AddUsersToContainer2 ( const char * Pre_name, int Number )
{
    static char NameBuff[200];
    TestPackUser* pUser2;
    ReplRegistry& RegObj = ReplRegistry::getInstance();
    ReplHashStrContainer* pReplHashStrContainer2 = ( ReplHashStrContainer* ) RegObj.get ( 2 );

    for ( int i = 0; i < Number; i++ )
    {
        NameBuff[0] = 0;
        sprintf ( NameBuff, "Con2_%s_%5d", Pre_name, i );
        pUser2 = new TestPackUser ( NameBuff, ( i % 50 + 10 ) );
        pReplHashStrContainer2->addAndUpdateObj ( pUser2 );
    }
}
bool TestReplPack::RemoveObjFromContainer2 ( const char * Pre_name, int Number )
{
    ReplRegistry& RegObj = ReplRegistry::getInstance();
    ReplHashStrContainer* pReplHashStrContainer2 = ( ReplHashStrContainer* ) RegObj.get ( 2 );

    if ( pReplHashStrContainer2 == NULL )
        return false;

    replHashStrNodeList& NodeList = replHashStrNodeList::getInstance ();
    DLinkQueue theNodeQueue;
    NodeList.getNodeList ( theNodeQueue );
    pReplHashStrContainer2->addNodeList ( &theNodeQueue );
    static char NameBuff[200];
    Replicable* pUser2;

    for ( int i = 0; i < Number; i++ )
    {
        NameBuff[0] = 0;
        sprintf ( NameBuff, "Con2_%s_%5d", Pre_name, i );
        pUser2 = pReplHashStrContainer2->removeObj ( NameBuff, 0 );

        if ( pUser2 != NULL )
            ;

        delete pUser2;
    }

    pReplHashStrContainer2->removeNodeList();

    return true;
}

bool TestReplPack::addObjToContainer2 ( const char * Pre_name, int Number )
{
    ReplRegistry& RegObj = ReplRegistry::getInstance();
    ReplHashStrContainer* pReplHashStrContainer2 = ( ReplHashStrContainer* ) RegObj.get ( 2 );

    if ( pReplHashStrContainer2 == NULL )
        return false;

    replHashStrNodeList& NodeList = replHashStrNodeList::getInstance ();
    DLinkQueue theNodeQueue;
    NodeList.getNodeList ( theNodeQueue );
    pReplHashStrContainer2->addNodeList ( &theNodeQueue );
    static char NameBuff[200];
    TestPackUser* pUser2;

    for ( int i = 0; i < Number; i++ )
    {
        NameBuff[0] = 0;
        sprintf ( NameBuff, "Con2_%s_%5d", Pre_name, i );
        pUser2 = new TestPackUser ( NameBuff, ( i % 50 + 10 ) );
        pReplHashStrContainer2->addAndUpdateObj ( pUser2 );
    }

    pReplHashStrContainer2->removeNodeList();

    return true;
}
int TestReplPack::runTest()
{
    if ( !IsReady )
        return 0;

    static int TestCase = 0;
    ReplPendingList& PendingList = ReplPendingList::getInstance();
    ReplRegistry& RegObj = ReplRegistry::getInstance();

    switch ( TestCase )
    {
    case 0:

        TestCase++;
        break;
    case 1:

        if ( PendingList.empty() )
        {
            /*               getReplPackFrom("127.0.0.1:1234", NULL);
                            getReplPackFromList();
                            CreateObj_ReplicateContainerTo("127.0.0.1:1234", NULL);
                            ReplicateContainerTo("127.0.0.1:1235", NULL);
                            ReplicateContainerTo("127.0.0.1:1236", NULL);
                            ReplicateContainerTo("127.0.0.1:1236", NULL, true);
                            AddObjToContainer ( );
                            RemoveObjFromContainer ( ); */
            AddUsersToContainer2 ( "Lilee", 1000 );
            //ReplicateContainerTo("127.0.0.1:1235", NULL);
            ReplicateContainerToM ( 20000 );
            RemoveObjFromContainer2 ( "Lilee", 2 );
            addObjToContainer2 ( "Lilee", 1 );
            TestCase++;
        }

        break;
    case 2:

        if ( PendingList.empty() )
        {
            /*               ReplHashStrContainer* pContainer1 = (ReplHashStrContainer*)RegObj.get(1);
                            ReplHashStrContainer* pContainer2 = (ReplHashStrContainer*)RegObj.get(2);
                            ReplHashStrContainer* pContainer3 = (ReplHashStrContainer*)RegObj.get(3);
                            CountBuf PacketBuff(2000);
                            pContainer1->FillKeyStrListToBuff(&PacketBuff);
                            PrintBuffStrList("Container1B", PacketBuff.begin(), PacketBuff.size());
                            PacketBuff.clear();
                            pContainer2->FillKeyStrListToBuff(&PacketBuff);
                            PrintBuffStrList("Container2B", PacketBuff.begin(), PacketBuff.size());
                            PacketBuff.clear();
                            pContainer3->FillKeyStrListToBuff(&PacketBuff);
                            PrintBuffStrList("Container3B", PacketBuff.begin(), PacketBuff.size());
                            PacketBuff.clear(); */

            replHashStrNodeList& NodeList = replHashStrNodeList::getInstance ();
            DLinkQueue theNodeQueue;
            NodeList.getNodeList ( theNodeQueue );
            ReplSender& SendObj = ReplSender::getInstance ( );
            SendObj.sendReplQueryListToList ( 1, NULL, 0, &theNodeQueue );
            SendObj.sendReplQueryListToList ( 2, NULL, 0, &theNodeQueue );
            SendObj.sendReplQueryListToList ( 3, NULL, 0, &theNodeQueue );
            TestCase++;
        }

        break;
    case 3:
        TestCase++;
        break;
    case 4:
        TestCase++;
        break;
    default:
        break;
    }

    return TestCase;
}
ReplPeer * TestReplPack::GetNode ( const char * name, ReplConn * pConn )
{
    ReplPeer * pReplNode = NULL;

    if ( pConn == NULL )
    {
        replHashStrNodeList& NodeList = replHashStrNodeList::getInstance ();
        pReplNode = NodeList.getNode ( name );

        if ( pReplNode == NULL )
            return NULL;
    }
    else
    {
        pReplNode = pConn->getNode();

        if ( pReplNode == NULL )
        {
            //Create receiving Node:
            pReplNode = new ReplPeer ( name, pConn );
            LS_DBG_M(  "Finished---Create receiving Node \n" );
            pConn->setNode ( pReplNode );
            LS_DBG_M(  "Finished---pConn->setNode( pReplNode ) \n" ) ;
            pReplNode->setPeerAddr ( name,  AF_INET );
            replHashStrNodeList& NodeList = replHashStrNodeList::getInstance ();
            NodeList.addNode ( pReplNode );
        }
    }

    return pReplNode;
}
