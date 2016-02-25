
#include "lsmcd.h"
#include <memcache/memcachelistener.h>
#include <memcache/memcacheconn.h>
#include <memcache/lsmemcache.h>
#include <shm/lsshmhash.h>
#include <memcache/rolememcache.h>
#include <repl/repllistener.h>
#include <lcrepl/lcreplconf.h>
#include <lcrepl/lcreplgroup.h>

#include <log4cxx/logger.h>
#define OS_LINUX

#include <edio/multiplexer.h>
#include <edio/poller.h>
#include <edio/epoll.h>

#include <util/crashguard.h>
#include <util/daemonize.h>
#include <util/pool.h>
#include <util/signalutil.h>
#include <socket/gsockaddr.h>
#include <util/mysleep.h>
#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <error.h>
#include <fstream>

#define    HS_CHILD     1
#define    HS_ALARM     2
#define    HS_STOP      4
#define    HS_HUP       8
#define    HS_USR1      16

#define PID_FILE       "lsmcd.pid"

Pool g_pool;

static int sEvents = 0;

static void sig_quit( int sig )
{
    //HttpServer::getInstance().shutdown();
    sEvents |=  HS_STOP;
}

static void sig_pipe( int sig )
{
}

static void sig_alarm( int sig )
{
    sEvents |= HS_ALARM ;
}

static void sig_hup( int sig )
{
    sEvents |= HS_HUP;
}

static void sig_usr1( int sig )
{
    sEvents |= HS_USR1 ;
}

#define WAC_VERSION     "1.1"

LsmcdImpl::LsmcdImpl()
        : m_ReplMasterSvrConn(NULL)
        , m_pC2rEventArr(NULL)
        , m_iNoCrashGuard( 0 )
        , m_iNoDaemon( 0 )

{}


LsmcdImpl::~LsmcdImpl()
{
    m_pMemcacheListener.Stop();
    m_pReplListener.Stop();
    if ( m_pC2rEventArr )
    {
        delete[] m_pC2rEventArr;
        m_pC2rEventArr = NULL;
    }
    
    if ( m_pR2cEventArr )
    {
        delete[] m_pR2cEventArr;
        m_pR2cEventArr = NULL;
    }
    
    if ( m_pMultiplexer )
    {
        delete m_pMultiplexer;
        m_pMultiplexer = NULL;
    }
    if (m_ReplMasterSvrConn)
    {
        delete m_ReplMasterSvrConn;
        m_ReplMasterSvrConn = NULL;
    }
}

static void perr( const char * pErr )
{
    fprintf( stderr,  "[ERROR] %s\n", pErr );
}

int LsmcdImpl::testRunningServer()
{
    char pBuf[256];
    int count = 0;
    int ret;
    snprintf(pBuf, sizeof(pBuf), "%s/%s", getReplConf()->getTmpDir(), PID_FILE);
    do
    {
        ret = m_pidFile.lockPidFile( pBuf );
        if ( ret )
        {
            if (( ret == -2 )&&( errno == EACCES || errno == EAGAIN ))
            {
                ++count;
                if ( count >= 10 )
                {
                    perr("LiteSpeed cached daemon is running!" );
                    return 2;
                }
                my_sleep( 100 );
            }
            else
            {
                fprintf( stderr, "[ERROR] Failed to write to pid file:%s!, please check the TmpDir entry in config file\n", PID_FILE );
                return ret;
            }
        }
        else
            break;
    }while( true );
    return ret;
}


int LsmcdImpl::Init( int argc, char *argv[] )
{
    if(argc > 1 ){
        if (ParseOpt(argc, argv) != LS_OK)
        {
            fprintf(stderr, "[ERROR] [Lscached]: failed to parse configuration\n");
            return LS_FAIL;
        }
    }
    if ( !m_iNoDaemon )
    {
        if ( Daemonize::daemonize( 1, 1 ) )
            return -3;
    }

    if ( testRunningServer() != 0 )
        return -2;
    m_pid = getpid();
    if ( m_pidFile.writePid( m_pid ) )
        return -2;
        
    setReplGroup(new LcReplGroup());
    //LcReplGroup::getInstance().initReplConn();

    return LS_OK;
}


int LsmcdImpl::PreEventLoop()
{
    //LcReplConf &replConf = LcReplConf::getInstance();
    m_pMultiplexer = new epoll();
    if ( m_pMultiplexer->init( 1024 ) == LS_FAIL )
    {
        delete m_pMultiplexer;
        m_pMultiplexer = new Poller();
    }
    m_pReplListener.SetMultiplexer( m_pMultiplexer );

    m_pReplListener.SetListenerAddr(getReplConf()->getLisenSvrAddr());

    if ( m_pReplListener.Start(-1) == LS_FAIL )
    {
        LS_ERROR(  "listener failed to start, addr=%s\n", getReplConf()->getLisenSvrAddr());
        return LS_FAIL;;
    }

    m_pMemcacheListener.SetMultiplexer ( m_pMultiplexer );

    m_pMemcacheListener.SetListenerAddr ( getReplConf()->getMemCachedAddr() );

    if ( m_pMemcacheListener.Start() == LS_FAIL )
        return LS_FAIL;

    int cnt = getReplConf()->getSubFileNum();
    LsCache2ReplEvent *pC2rEventPtrs[cnt];
    LsRepl2CacheEvent *pR2cEventPtrs[cnt];
    if ( !setupC2rEvents(cnt, m_pMultiplexer,pC2rEventPtrs) )
    {
        return LS_FAIL;
    }
    if ( !setupR2cEvents(cnt, m_pMultiplexer,pR2cEventPtrs) )
    {
        return LS_FAIL;
    }

    LsMemcache::getInstance().setMultiplexer(m_pMultiplexer);
    if (LsMemcache::getInstance().initMcEvents(pC2rEventPtrs) < 0)
        return LS_FAIL;

#ifdef notdef
    if( m_uRole == R_MASTER )
    {
        LsShmHash *pLsShmHash = LsShmMemcache::getInstance().getHash();
        assert( pLsShmHash != NULL );
        pLsShmHash->setEventNotifier(&m_lsShmEvent);
    }
#endif
    LcReplGroup::getInstance().setMultiplexer(m_pMultiplexer);
    LcReplGroup::getInstance().initReplConn();
    return LS_OK;
}

bool LsmcdImpl::setupC2rEvents(int cnt, Multiplexer *pMultiplexer, LsCache2ReplEvent **ppEvent)
{
    int idx = 0;
    m_pC2rEventArr = new LsCache2ReplEvent [cnt];
    LsCache2ReplEvent *pEvent = m_pC2rEventArr;
    while (--cnt >= 0)
    {
        if (pEvent->init(idx, pMultiplexer))
        {
            delete[] m_pC2rEventArr;
            return NULL;
        }
        *ppEvent++ = pEvent;
        ++pEvent;
        ++idx;
    }
    return true;
}

bool LsmcdImpl::setupR2cEvents(int cnt, Multiplexer *pMultiplexer, LsRepl2CacheEvent **ppEvent)
{
    int idx = 0;
    m_pR2cEventArr = new LsRepl2CacheEvent [cnt];
    LsRepl2CacheEvent *pEvent = m_pR2cEventArr;
    while (--cnt >= 0)
    {
        pEvent->setIdx(idx++);
        if (pEvent->initNotifier(pMultiplexer))
        {
            delete[] m_pR2cEventArr;
            return NULL;
        }
        *ppEvent++ = pEvent;
        ++pEvent;
    }
    return true;
}

int LsmcdImpl::reinitMultiplexer(int procNo)
{
    delete m_pMultiplexer;
    m_pMultiplexer = new epoll();
    if ( m_pMultiplexer->init( 1024 ) == LS_FAIL )
    {
        delete m_pMultiplexer;
        m_pMultiplexer = new Poller();
    }
    m_pReplListener.SetMultiplexer( m_pMultiplexer );
    m_pMultiplexer->add( &m_pReplListener, POLLIN | POLLHUP | POLLERR );

    LsCache2ReplEvent *pEvent = m_pC2rEventArr;
    int cnt = getReplConf()->getSubFileNum();
    while (--cnt >= 0)
    {
        if (procNo == 0) //replicator
            m_pMultiplexer->add(pEvent, POLLIN | POLLHUP | POLLERR);
        else
            m_pMultiplexer->add(pEvent, POLLHUP | POLLERR);
        ++pEvent;
    }

    LsRepl2CacheEvent *pEvent2 = m_pR2cEventArr;
    cnt = getReplConf()->getSubFileNum();
    while (--cnt >= 0)
    {
        if (procNo == 0) //replicator
            m_pMultiplexer->add(pEvent2, POLLHUP | POLLERR);
        else
            m_pMultiplexer->add(pEvent2, POLLIN | POLLHUP | POLLERR);
        ++pEvent2;
    }

    m_pMemcacheListener.SetMultiplexer ( m_pMultiplexer );
    m_pMultiplexer->add( &m_pMemcacheListener, POLLIN | POLLHUP | POLLERR );


    return LS_OK;

}

int LsmcdImpl::ParseOpt( int argc, char *argv[] )
{
    const char * opts = "cdnvlf:";
    int c;
    while ( ( c = getopt( argc, argv, opts )) != EOF )
    {
        switch (c) {
        case 'c':
            m_iNoCrashGuard = 1;
            break;
        case 'd':
            m_iNoDaemon = 1;
            m_iNoCrashGuard = 1;
            break;
        case 'n':
            m_iNoDaemon = 1;
            break;
        case 'f':
            ConfWrapper::getInstance().loadConf(new LcReplConf());
            if (!getReplConf()->parse(optarg))
                return LS_FAIL;
            break;
        case 'v':
            printf( "lsmcd server %s\n", WAC_VERSION );
            exit( 0 );
            break;
        case '?':
            break;

        default:
            printf ("?? getopt returned character code -%o ??\n", c);
        }
    }
    return LS_OK;
}

int LsmcdImpl::ChangeUid()
{
    struct group * gr;
    struct passwd *pw;

    pw = getpwnam( "nobody" );
    if ( pw == NULL )
    {
            return -1;
    }
    gr = getgrnam( "nobody" );
    if ( gr == NULL )
    {
            return -1;
    }

    if ( setgid( gr->gr_gid ) < 0 )
    {
        return -1;
    }

    if ( setuid( pw->pw_uid ) < 0 )
    {
        return -1;
    }

    return 0;
}


int LsmcdImpl::ProcessTimerEvent()
{
    m_pMultiplexer->timerExecute();
    //FIXME
    LcReplGroup::getInstance().onTimer1s();
    LsMemcache::getInstance().onTimer();
    return 0;
}

#define MLTPLX_TIMEOUT 1000

int LsmcdImpl::EventLoop()
{
    register int ret;
    register int event;
    alarm( 1 );
    //ReplConn::s_curTime = time( NULL );
    while( true )
    {
        ret = m_pMultiplexer->waitAndProcessEvents(
                    MLTPLX_TIMEOUT );
        if (( ret == -1 )&& errno )
        {
            if (!((errno == EINTR )||(errno == EAGAIN)))
            {
                return 1;
            }
        }
        ProcessTimerEvent();
        if ( (event = sEvents ) )
        {
            sEvents = 0;
            if ( event & HS_ALARM )
            {
                alarm( 1 );
//                 ProcessTimerEvent();
            }
            if ( event & HS_STOP )
            {
                break;
            }
        }
        if (getppid() == 1)
            break;

    }
    return 0;
}

Lsmcd  Lsmcd::_msServer;

Lsmcd::Lsmcd()
    : _pReplSvrImpl(NULL)
{
    //_pReplSvrImpl        = new ReplServerImpl();
    //_pCacacheImpl = new LscachedImpl();
}


Lsmcd::~Lsmcd()
{
    ReleaseAll();
}


void Lsmcd::ReleaseAll()
{
    if ( _pReplSvrImpl )
    {
        delete _pReplSvrImpl;
        _pReplSvrImpl = NULL;
    }
}

Multiplexer * Lsmcd::getMultiplexer() const
{
    return _pReplSvrImpl->m_pMultiplexer;
}

static void init_signals()
{
    SignalUtil::signal( SIGTERM, sig_quit );
    SignalUtil::signal( SIGHUP, sig_hup );
    SignalUtil::signal( SIGPIPE, sig_pipe );
    SignalUtil::signal( SIGALRM, sig_alarm );
    SignalUtil::signal( SIGUSR1, sig_usr1 );
    SignalUtil::signal( SIGINT, sig_quit );
}

int Lsmcd::Main(int argc, char **argv)
{
    _pReplSvrImpl = new LsmcdImpl();

    if (_pReplSvrImpl->Init(argc, argv) < 0)
        return -1;

    //LcReplConf& replConf = LcReplConf::getInstance();

    LsMcParms parms;
    setMcParms(&parms);
    if (LsMemcache::getInstance().initMcShm(
        getReplConf()->getSubFileNum(),      // number of slices
        (const char **)getReplConf()->getShmFiles(),  // array of pointers to names
        getReplConf()->getShmHashName(), //->_pHashName,
        &parms) < 0)
        return -1;

    if (RoleMemCache::getInstance().initShm(getReplConf()->getShmDir()) < 0)
        return -1;

    if (_pReplSvrImpl->PreEventLoop() < 0)
      return -1;

    LcReplGroup::getInstance().setR2cEventRef(_pReplSvrImpl->m_pR2cEventArr);
    LcReplGroup::getInstance().setLstnrProc(true);
    if (!_pReplSvrImpl->m_iNoCrashGuard)
    {
        CrashGuard cg(this);

        int ret = cg.guardCrash(2);
        if (ret == -1)
            return 8;

        for( int i = 0; i< argc; ++i)
        {
            int n = strlen( argv[i]);
            memset(argv[i], 0 , n);
        }

        _pReplSvrImpl->reinitMultiplexer(ret);

        if (ret == 0) //replicator
        {
            snprintf(argv[0], 80, "lsmcd - replicator");
            _pReplSvrImpl->m_pMemcacheListener.Stop();
            LcReplGroup::getInstance().setMultiplexer(getMultiplexer());
            ::sleep(1);
        }
        else
        {
            snprintf(argv[0], 80, "lsmcd - cached #%02d", ret);
            _pReplSvrImpl->m_pReplListener.Stop();
            LsMemcache::getInstance().setMultiplexer(getMultiplexer());
            LcReplGroup::getInstance().setR2cEventRef(NULL);
            LcReplGroup::getInstance().setLstnrProc(false);
        }
    }

    init_signals();

    _pReplSvrImpl->EventLoop();

    ReleaseAll();

    return 0;
}


int Lsmcd::preFork(int seq)
{
    return 0;
}


int Lsmcd::forkError(int seq, int err)
{
    //LOG_ERR("Failed to fork: ("<< err << ")" << strerror(err));
    return 0;
}


int Lsmcd::postFork(int seq, pid_t pid)
{
    //LOG_INF("Child Process with PID: " << pid << " has been forked.");
    return 0;
}


int Lsmcd::childExit(int seq, pid_t pid, int stat)
{
    //LOG_INF("Child Process with PID: " << pid << " has exit with status " << stat);
    return 0;
}


void Lsmcd::setMcParms(LsMcParms *pParms)
{
    //LcReplConf& replConf = LcReplConf::getInstance();
    pParms->m_usecas    = getReplConf()->getUseCas();
    pParms->m_usesasl   = getReplConf()->getUseSasl();
    pParms->m_nomemfail = getReplConf()->getNomemFail();
    pParms->m_iValMaxSz = getReplConf()->getValMaxSz();
    pParms->m_iMemMaxSz = getReplConf()->getMemMaxSz() * 1024 * 1024;
    return;
}

