

#ifndef LSCACHED_H
#define LSCACHED_H

#include <memcache/memcachelistener.h>
#include <repl/replconn.h>
#include <repl/repllistener.h>
#include <util/guardedapp.h>
#include <util/pidfile.h>
#include <lcrepl/lcshmevent.h>
#include <sys/types.h>
#include <unistd.h>

class LsmcdImpl;
class Multiplexer;
class ReplConn;
class GSockAddr;
class LsShmHash;
class LsCache2ReplEvent;


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
    virtual int PreEventLoop();
    virtual int  EventLoop();
    
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

    int  LoadConfig();    
    int  connReplListenSvr();    
    
    bool setupC2rEvents(int cnt, Multiplexer *pMultiplexer, LsCache2ReplEvent **ppEvent);
    bool setupR2cEvents(int cnt, Multiplexer *pMultiplexer, LsRepl2CacheEvent **ppEvent);
    int  reinitMultiplexer(int procNo);
    int  testRunningServer();
protected:
    Multiplexer *       m_pMultiplexer;
    ReplListener        m_pReplListener;
    ReplConn *          m_ReplMasterSvrConn; //only used by slave
    MemcacheListener    m_pMemcacheListener;
    
    LsRepl2CacheEvent *        m_pR2cEventArr; 
    LsCache2ReplEvent *        m_pC2rEventArr;     
    int                 m_iNoCrashGuard;
    int                 m_iNoDaemon;
    uint16_t            m_uRole;
    PidFile             m_pidFile;
    pid_t               m_pid;    
    
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
//     void notifyMainLoop(const EventStack *pEvStatck);
//     const EventStack * readEventStack() const ; 

private:
    static Lsmcd   _msServer;
    LsmcdImpl *    _pReplSvrImpl;
    void ReleaseAll();
    void setMcParms(LsMcParms *pParms);
    virtual int preFork(int seq);
    virtual int forkError( int seq, int err );
    virtual int postFork( int seq, pid_t pid );
    virtual int childExit( int seq, pid_t pid, int stat );
 
};

#endif
