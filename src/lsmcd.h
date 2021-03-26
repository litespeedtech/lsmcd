

#ifndef LSCACHED_H
#define LSCACHED_H
#include <lcrepl/usocklistener.h>
#include <memcache/memcachelistener.h>
#include <repl/replconn.h>
#include <repl/repllistener.h>
#include <util/guardedapp.h>
#include <util/pidfile.h>
#include "lcrepl/fdpasslstnr.h"
#include <sys/types.h>
#include <unistd.h>

class LsmcdImpl;
class Multiplexer;
class ReplConn;
class GSockAddr;
class LsShmHash;
class LsCache2ReplEvent;
class UsockClnt;

typedef struct LsMcParms_s LsMcParms;

enum 
{
    EV_SVRNOTIFIED_CLNTCLOSED = 1
};

class LsmcdImpl
{
    friend class Lsmcd;
public:
    LsmcdImpl();
    virtual ~LsmcdImpl();
    virtual int Init( int argc, char *argv[] );
    virtual void getUid(uid_t *uid, gid_t *gid);
    virtual int SetUDSAddrFile();
    virtual int PreEventLoop();
    virtual int  EventLoop();
    virtual void enableCoreDump();
    
    void RecycleNodeConn( ReplConn * pConn );
    ReplConn * GetNodeConn();
    int ProcessReplEvent();
    
protected:
    int  ParseOpt( int argc, char *argv[] );

    int  ChangeUid();

    int  LoadNodeList();
    void DumpNodeList( std::ostream &os);
    
    int  TryLoadNodeList();

    int  StartNodeListUpdateProcess();

    void ShutdownThreads();

    int  ProcessTimerEvent();

    int  AddNode( char * pNodeIP );
    int  getServerRootFromExecutablePath( const char * command );
    MemcacheListener *getPriCacheLstr();
    
    int  LoadConfig();    
    int  connReplListenSvr();    
    
    int  reinitMultiplexer();
    int  reinitRepldMpxr(int procNo);
    int  reinitCachedMpxr(int procNo);
    int  testRunningServer();
    int  connUsockSvr();
    void delUsockFiles();
    
protected:
    int                 m_iNoCrashGuard;
    int                 m_iNoDaemon;
    uint16_t            m_uRole;
    PidFile             m_pidFile;
    pid_t               m_pid;

    Multiplexer *       m_pMultiplexer;
    ReplListener        m_pReplListener;
    FdPassLstnr         m_fdPassLstnr;
    MemcacheListener    m_pMemcacheListener;
    UsocklListener      m_usockLstnr;
    UsockClnt           *m_pUsockClnt;
    uid_t               m_uid;
    gid_t               m_gid;
    bool                m_gotUid;
    
};

class Lsmcd : public GuardedApp
{

public:
    Lsmcd();
    ~Lsmcd();

    static Lsmcd & getInstance()   {   return _msServer;    }

    Multiplexer * getMultiplexer() const ;

    const GSockAddr * GetServiceAddr( int service ) const;


    void AddActiveConn( int port, ReplConn * pConn );
    void RemoveActiveConn( int port );
    ReplConn * GetActiveConn( int port );

    int Main( int argc, char ** argv );
    
    uint8_t getProcId() const;
    void setProcId(uint8_t procId);
    UsockClnt *getUsockConn() const;
private:
    static Lsmcd   _msServer;
    LsmcdImpl *    _pReplSvrImpl;
    void ReleaseAll();
    void setMcParms(LsMcParms *pParms);
    virtual int preFork(int seq);
    virtual int forkError( int seq, int err );
    virtual int postFork( int seq, pid_t pid );
    virtual int childExit( int seq, pid_t pid, int stat );
    uint8_t             m_procId;                 
};

#endif
