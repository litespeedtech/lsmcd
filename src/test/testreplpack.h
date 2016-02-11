#ifndef TESTREPLPACK_H
#define TESTREPLPACK_H
#include "replshmtid.h"
#include "replicate/replpacket.h"

#include "replconn.h"

bool initReplRegistry2();

class TestReplPack
{

public:
    TestReplPack();
    TestReplPack ( const TestReplPack& other );
    virtual ~TestReplPack();
    virtual TestReplPack& operator= ( const TestReplPack& other );
    virtual bool operator== ( const TestReplPack& other ) const;
    static int runTest();
    static bool initReplRegistry();
    static bool sendReplPackTo ( const char * name, ReplConn * pConn );
    
    static bool getReplPackFrom ( const char * name, ReplConn * pConn );
    static bool DeleteReplPackFrom ( const char * name, ReplConn * pConn );
    static bool getReplPackFromList();
    static bool ReplicateContainerTo ( char * name, ReplConn * pConn, bool update = false );
    static bool ReplicateContainerToM (int32_t MaxSize);
    static bool CreateObj_ReplicateContainerTo ( char * name, ReplConn * pConn );
    static bool AddObjToContainer ( );
    static bool RemoveObjFromContainer ( );
    static void SetReady ( ){IsReady = true;};
    static ReplPeer * GetNode( const char * name, ReplConn * pConn );
    static bool AddUsersToContainer2( const char * Pre_name, int Number );
    static bool RemoveObjFromContainer2 (const char * Pre_name, int Number );
    static bool addObjToContainer2 (const char * Pre_name, int Number );
    
    static bool AddShmTidObjToContainer ( ReplicableShmTid* pReplicableShmTid, uint64_t tid);
    static bool removeShmTidObjToContainer (ReplicableShmTid* pReplicableShmTid);    
private:
    static bool IsReady;
};


#endif // TESTREPLPACK_H
