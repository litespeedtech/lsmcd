
#include "shm_lscached.h"

#include "memcachelistener.h"
#include "memcacheconn.h"

#define OS_LINUX

#include <edio/multiplexer.h>
#include <edio/poller.h>
#include <edio/epoll.h>

#include <util/crashguard.h>
#include <util/daemonize.h>
#include <util/pool.h>
#include <util/signalutil.h>
#include <socket/gsockaddr.h>

#include <string>
#include <vector>

#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <fstream>

#ifdef notdef
#include "replicate/repltcptransport.h"
#include "test/testreplpack.h"
#include "replicate/replpendinglist.h"
#include "replicate/replhashstrnodelist.h"
#endif
#include "memcachelistener.h"
#include "memcache/lsmemcache.h"


#define    HS_CHILD     1
#define    HS_ALARM     2
#define    HS_STOP      4
#define    HS_HUP       8
#define    HS_USR1      16

#define USE_MEMCACHE_LISTENER


static const char *shmPaths[] =
{
#ifdef notdef
    "/dev/shm/LiteSpeed/XXXX",
#endif
    "/dev/shm/LiteSpeed/XXX1",
    "/dev/shm/LiteSpeed/XXX2",
    "/tmp/XXX3",
    "/dev/shm/LiteSpeed/XXX4",
};
static int numSlices = (sizeof(shmPaths) / sizeof(shmPaths[0]));

#ifdef notdef
class ReplTcpTransport;
#endif
static int sEvents = 0;

// Pool g_pool;


static void sig_quit ( int sig )
{
    //HttpServer::getInstance().shutdown();
    sEvents |=  HS_STOP;
}

static void sig_pipe ( int sig )
{
}

static void sig_alarm ( int sig )
{
    sEvents |= HS_ALARM ;
}

static void sig_hup ( int sig )
{
    sEvents |= HS_HUP;
}

static void sig_usr1 ( int sig )
{
    sEvents |= HS_STOP;
    sEvents |= HS_USR1 ;
}


#define WAC_VERSION     "1.0"


LsmcdImpl::~LsmcdImpl()
{
    m_pMemcacheListener.Stop();
    if ( m_pShmEvent )
    {
        delete[] m_pShmEvent;
        m_pShmEvent = NULL;
    }
    if ( m_pMultiplexer )
    {
        delete m_pMultiplexer;
        m_pMultiplexer = NULL;
    }
}


void LsmcdImpl::ParseOpt ( int argc, char *argv[] )
{
    const char * opts = "cdnMm:Ss:x:f:t:v";
    int c;
    while ( ( c = getopt ( argc, argv, opts ) ) != EOF )
    {
        switch ( c )
        {
        case 'c':
            m_iNoCrashGuard = true;
            break;
        case 'd':
            m_iNoDaemon = true;
            m_iNoCrashGuard = true;
            break;
        case 'n':
            m_iNoDaemon = true;
            break;
        case 'M':
            _McParms.m_nomemfail = true;
            break;
        case 'm':
            _McParms.m_iMemMaxSz = ((LsShmXSize_t)atoi(optarg)) * 1024 * 1024;
            break;
        case 'S':
            _McParms.m_usesasl = true;
            break;
        case 's':
            _pServAddr = optarg;
            break;
        case 'x':
            _pShmDirName = optarg;
            break;
        case 'f':
            _pShmName = optarg;
            break;
        case 't':
            _pHashName = optarg;
            break;
        case 'v':
            printf ( "De-multiplexer Server %s\n", WAC_VERSION );
            exit ( 0 );
            break;
        case '?':
            break;

        default:
            printf ( "?? getopt returned character code -%o ??\n", c );
        }
    }
}


int LsmcdImpl::ChangeUid()
{
    struct group * gr;
    struct passwd *pw;

    pw = getpwnam ( "nobody" );
    if ( pw == NULL )
    {
        return -1;
    }
    gr = getgrnam ( "nobody" );
    if ( gr == NULL )
    {
        return -1;
    }

    if ( setgid ( gr->gr_gid ) < 0 )
    {
        return -1;
    }

    if ( setuid ( pw->pw_uid ) < 0 )
    {
        return -1;
    }

    return 0;
}


int LsmcdImpl::reinitMultiplexer()
{
    delete m_pMultiplexer;
    m_pMultiplexer = new epoll();
    if ( m_pMultiplexer->init( 1024 ) == LS_FAIL )
    {
        delete m_pMultiplexer;
        m_pMultiplexer = new Poller();
    }

#ifdef USE_REPL_LISTENER
    m_pReplListener.SetMultiplexer( m_pMultiplexer );
    m_pMultiplexer->add( &m_pReplListener, POLLIN | POLLHUP | POLLERR );
#endif

    LsCache2ReplEvent *pEvent = m_pShmEvent;
    int cnt = numSlices;
    while (--cnt >= 0)
    {
        m_pMultiplexer->add(pEvent, POLLIN | POLLHUP | POLLERR);
        ++pEvent;
    }

    m_pMemcacheListener.SetMultiplexer ( m_pMultiplexer );
    m_pMultiplexer->add( &m_pMemcacheListener, POLLIN | POLLHUP | POLLERR );

    return LS_OK;
}


int LsmcdImpl::ProcessTimerEvent()
{
#ifdef notdef
    ReplPendingList& PendingList = ReplPendingList::getInstance();
    PendingList.monitorObjs(10, 7);
    replHashStrNodeList& NodeList = replHashStrNodeList::getInstance ();
    NodeList.maintainNodes();
    if(!IsServer)
        TestReplPack::runTest();
    ReplConn::s_curTime = time ( NULL );
#endif
    m_pMultiplexer->timerExecute();
    return 0;
}


#define MLTPLX_TIMEOUT 1000

int LsmcdImpl::EventLoop()
{
    register int ret;
    register int event;
    alarm ( 1 );
    MemcacheConn::s_curTime = time ( NULL );
    //LS_NOTICE("Entering shm_lscached event loop\n"); NOT USED
    while ( true )
    {
        ret = m_pMultiplexer->waitAndProcessEvents (
                  MLTPLX_TIMEOUT );
        if ( ( ret == -1 ) && errno )
        {
            if ( ! ( ( errno == EINTR ) || ( errno == EAGAIN ) ) )
            {
                return 1;
            }
        }
        if ( ( event = sEvents ) )
        {
            sEvents = 0;
            if ( event & HS_ALARM )
            {
                alarm ( 1 );
                ProcessTimerEvent();
            }
            if (event & HS_USR1)
                LS_NOTICE("Doing restart (EventLoop)\n");
            if ( event & HS_STOP )
            {
                break;
            }
        }
        if (getppid() == 1)
            break;
    }
    if (event & HS_USR1)
        return HS_USR1;
    return 0;
}


Lsmcd  Lsmcd::_msServer;

Lsmcd::Lsmcd()
    : _pImpl ( NULL )
{
    _pImpl = new LsmcdImpl();
}


Lsmcd::~Lsmcd()
{
    ReleaseAll();
}


void Lsmcd::ReleaseAll()
{
    if ( _pImpl )
    {
        delete _pImpl;
        _pImpl = NULL;
    }
}


Multiplexer * Lsmcd::getMultiplexer() const
{
    return _pImpl->m_pMultiplexer;
}


static void init_signals()
{
    SignalUtil::signal ( SIGTERM, sig_quit );
    SignalUtil::signal ( SIGHUP, sig_hup );
    SignalUtil::signal ( SIGPIPE, sig_pipe );
    SignalUtil::signal ( SIGALRM, sig_alarm );
    SignalUtil::signal ( SIGUSR1, sig_usr1 );
    SignalUtil::signal ( SIGINT, sig_quit );
}


LsCache2ReplEvent::LsCache2ReplEvent() {}
LsCache2ReplEvent::~LsCache2ReplEvent() {}

int LsCache2ReplEvent::onNotified(int count)
{
//     LS_INFO("onNotified: count=%d\n", count);
    return 0;
}


int Lsmcd::Main ( int argc, char ** argv )
{
#define MAX_CMD 256
    char cwd[MAX_CMD];
    getcwd(cwd, sizeof(cwd));
    
    if ( argc > 1 )
    {
        _pImpl->ParseOpt ( argc, argv );
    }

    if (LsMemcache::getInstance().initMcShm(
        numSlices,
        (const char **)shmPaths,
        _pImpl->_pHashName,
        &_pImpl->_McParms) < 0)
        return 2;
#ifdef notdef
    if (LsShmMemcache::getInstance().initMcShm(
        _pImpl->_pShmDirName,
        _pImpl->_pShmName,
        _pImpl->_pHashName,
        &_pImpl->_McParms) < 0)
        return 2;
#endif

    if ( !_pImpl->m_iNoDaemon )
    {
        if ( Daemonize::daemonize ( 1, 1 ) )
            return 3;
    }
    _pImpl->m_pMultiplexer = new epoll();
    if ( _pImpl->m_pMultiplexer->init ( 1024 ) == -1 )
    {
        delete _pImpl->m_pMultiplexer;
        _pImpl->m_pMultiplexer = new Poller();
    }

#ifdef USE_MEMCACHE_LISTENER
    if ( _pImpl->_pServAddr == NULL )
        return -1;
    _pImpl->m_pMemcacheListener.SetMultiplexer ( getMultiplexer() );
    _pImpl->m_pMemcacheListener.SetListenerAddr ( _pImpl->_pServAddr );

    if ( _pImpl->m_pMemcacheListener.Start() == -1 )
    {
        return -1;
    }
#endif

#ifdef USE_REPL_LISTENER
    _pImpl->m_pReplListener.SetMultiplexer ( getMultiplexer() );
    //_pImpl->m_pReplListener.SetListenerAddr ( "*:1234" );
    //Xuedong add test code here
    if(argc > 2)
        _pImpl->m_pReplListener.SetListenerAddr ( argv[2] );
    else
        return -1;
    //Xuedong's test code end here
    if ( _pImpl->m_pReplListener.Start() == -1 )
    {
        return -1;
    }
#endif
    LsCache2ReplEvent *pEventPtrs[1024];  // lazy way, assumes slices < 1024
    if ((_pImpl->m_pShmEvent = setupEvents(
        numSlices, _pImpl->m_pMultiplexer, pEventPtrs)) == NULL)
    {
        return 4;
    }
    LsMemcache::getInstance().setMultiplexer(_pImpl->m_pMultiplexer);
    if (LsMemcache::getInstance().initMcEvents(pEventPtrs) < 0)
        return 5;

    if ( !_pImpl->m_iNoCrashGuard )
    {
        CrashGuard cg ( this );
        int ret = cg.guardCrash(2);
        if (ret == -1)
            return 8;
        for( int i = 0; i< argc; ++i)
        {
            int n = strlen( argv[i]);
            memset(argv[i], 0 , n);
        }

        _pImpl->reinitMultiplexer();

        if (ret == 0) //replicator
        {
            //snprintf(argv[0], 80, "lsmcd - replicator", ret);
            _pImpl->m_pMemcacheListener.Stop();
        }
        else
        {
// #ifdef USE_REPL_LISTENER
             _pImpl->m_pReplListener.Stop();
// #endif
            //snprintf(argv[0], 80, "lsmcd - cached #%02d", ret);
        }
    }

    init_signals();

    _pImpl->IsServer = true;
    int rc = _pImpl->EventLoop();

    ReleaseAll();

    if (rc == HS_USR1)
        LS_NOTIFY("Not restarting here\n");
    return 0;
}


LsCache2ReplEvent *Lsmcd::setupEvents(
    int cnt, Multiplexer *pMultiplexer, LsCache2ReplEvent **ppEvent)
{
    LsCache2ReplEvent *pEvents = new LsCache2ReplEvent [cnt];
    LsCache2ReplEvent *pEvent = pEvents;
    while (--cnt >= 0)
    {
        if (pEvent->initNotifier(pMultiplexer))
        {
            delete[] pEvents;
            return NULL;
        }
        *ppEvent++ = pEvent;
        ++pEvent;
    }
    return pEvents;
}


int Lsmcd::StartClient()
{
    init_signals();
    _pImpl->IsServer = false;
#ifdef notdef
    ReplConn clientConn4, clientConn5, clientConn6;
    GSockAddr serverAddr4, serverAddr5, serverAddr6;
    serverAddr4.set( "127.0.0.1:1234", AF_INET );
    _pImpl->m_pMultiplexer = new epoll();
    if ( _pImpl->m_pMultiplexer->init ( 1024 ) == -1 )
    {
        delete _pImpl->m_pMultiplexer;
        _pImpl->m_pMultiplexer = new Poller();
    }

    //_pImpl->m_pReplListener.SetMultiplexer ( getMultiplexer() );
    //_pImpl->m_pReplListener.SetListenerAddr ( "*:1234" );

    clientConn4.SetMultiplexer( getMultiplexer() );
    clientConn4.connectTo( &serverAddr4 );

//     ReplTcpTransport MyReplTcpTransport;
//     MyReplTcpTransport.init_connection(getMultiplexer());
    //init_signals();
    //Test code start
    TestReplPack::initReplRegistry();
    TestReplPack::sendReplPackTo("127.0.0.1:1234", &clientConn4);
    TestReplPack::SetReady();
//      TestReplPack::getReplPackFrom("127.0.0.1:1234", &clientConn4);
//      TestReplPack::DeleteReplPackFrom("127.0.0.1:1234", &clientConn4);
    //Test code end

    _pImpl->EventLoop();

    ReleaseAll();
#endif

    return 0;
}


int Lsmcd::preFork(int seq)
{
    return 0;
}


int Lsmcd::forkError ( int seq, int err )
{
    //LOG_ERR( "Failed to fork: ("<< err << ")" << strerror( err ) );
    return 0;
}


int Lsmcd::postFork ( int seq, pid_t pid )
{

    //LOG_INF( "Child Process with PID: " << pid << " has been forked." );
    return 0;
}

int Lsmcd::childExit ( int seq, pid_t pid, int stat )
{
    //LOG_INF( "Child Process with PID: " << pid << " has exit with status " << stat );
    return 0;
}

