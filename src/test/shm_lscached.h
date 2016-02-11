

#ifndef SHM_LSCACHED_H
#define SHM_LSCACHED_H

#include "memcachelistener.h"
#include "memcacheconn.h"
#include <memcache/lsmemcache.h>
#include "replicate/lsshmevent.h"

#include <util/guardedapp.h>


class Multiplexer;
class ReplConn;
class GSockAddr;
class ReplServer;
class LsmcdImpl
{
    friend class Lsmcd;
    friend class ReplServer;
public:
    LsmcdImpl()
        : m_pShmEvent ( NULL )
        //, m_iNoCrashGuard ( true )
        , m_iNoCrashGuard ( 0 )
        , m_iNoDaemon ( true )
        , _pServAddr ( NULL )
        , _pShmDirName ( NULL )
        , _pShmName ( NULL )
        , _pHashName ( NULL )
    {
        _McParms.m_usecas = true;
        _McParms.m_usesasl = false;
        _McParms.m_nomemfail = false;    // purge rather than fail if no mem
        _McParms.m_iValMaxSz = 0;        // default
        _McParms.m_iMemMaxSz = 0;        // default
    }

    ~LsmcdImpl();

    int  getServerRootFromExecutablePath ( const char * command );

    void ParseOpt ( int argc, char *argv[] );

    int  ChangeUid();

    int  EventLoop();

    int  ProcessTimerEvent();

    int  reinitMultiplexer();

    //Start---Xuedong Add for test
    bool IsServer;
    //End-----Xuedong Add for test

private:

    Multiplexer        *m_pMultiplexer;
    MemcacheListener    m_pMemcacheListener;
    LsCache2ReplEvent         *m_pShmEvent;

    bool                m_iNoCrashGuard;
    bool                m_iNoDaemon;
    char               *_pServAddr;
    char               *_pShmDirName;
    char               *_pShmName;
    char               *_pHashName;
    LsMcParms           _McParms;
};

class Lsmcd : public GuardedApp
{

public:
    ~Lsmcd();

    static Lsmcd & GetInstance()   {   return _msServer;    }

    Multiplexer * getMultiplexer() const ;

    const GSockAddr * GetServiceAddr( int service ) const;


    void AddActiveConn( int port, ReplConn * pConn );
    void RemoveActiveConn( int port );
    ReplConn * GetActiveConn( int port );

    int Main( int argc, char ** argv );
    int StartClient();

private:
    static Lsmcd   _msServer;
    LsmcdImpl *    _pImpl;

    Lsmcd();
    void ReleaseAll();
    LsCache2ReplEvent *setupEvents(
        int cnt, Multiplexer *pMultiplexer, LsCache2ReplEvent **ppEvent);
    virtual int preFork(int seq);
    virtual int forkError( int seq, int err );
    virtual int postFork( int seq, pid_t pid );
    virtual int childExit( int seq, pid_t pid, int stat );
    
};

#endif
