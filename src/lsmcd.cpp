
#include "lsmcd.h"
#include <memcache/memcachelistener.h>
#include <memcache/memcacheconn.h>
#include <memcache/lsmemcache.h>
#include <shm/lsshmhash.h>
#include <repl/repllistener.h>
#include <lcrepl/lcreplconf.h>
#include <lcrepl/lcreplgroup.h>
#include <lcrepl/usockmcd.h>
#include "lcrepl/lcreplsender.h"

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
//#include <fstream>
#include <time.h>

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

#define WAC_VERSION     "1.2"

LsmcdImpl::LsmcdImpl()
        : m_iNoCrashGuard( 0 )
        , m_iNoDaemon( 0 )
        , m_pMultiplexer(NULL)
        , m_pUsockClnt(NULL)
{}


LsmcdImpl::~LsmcdImpl()
{
    m_pMemcacheListener.Stop();
    m_pReplListener.Stop();
    if (m_pUsockClnt != NULL)
        delete m_pUsockClnt;

    if ( m_pMultiplexer )
    {
        delete m_pMultiplexer;
        m_pMultiplexer = NULL;
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
    delUsockFiles();
    
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
    
    LsMemcache::getInstance().setMultiplexer(m_pMultiplexer);
    if (LsMemcache::getInstance().initMcEvents() < 0)
        return LS_FAIL;
    
    m_pReplListener.SetMultiplexer( m_pMultiplexer );
    if (LsMemcache::getConfigReplication())
    {
        m_pReplListener.SetListenerAddr(getReplConf()->getLisenSvrAddr());
        if ( m_pReplListener.Start(-1) != LS_OK )
        {
            LS_ERROR("repl listener failed to start, addr=%s\n", 
                     getReplConf()->getLisenSvrAddr());
            return LS_FAIL;;
        }
    }
    
    m_pMemcacheListener.SetMultiplexer ( m_pMultiplexer );
    m_pMemcacheListener.SetListenerAddr ( getReplConf()->getMemCachedAddr() );
    if ( m_pMemcacheListener.Start() != LS_OK )
    {
        LS_ERROR("Memcache Listener failed to start");
        return LS_FAIL;
    }

    if (LsMemcache::getConfigReplication())
    {
        m_usockLstnr.SetMultiplexer(m_pMultiplexer);
        m_usockLstnr.SetListenerAddr( getReplConf()->getCachedUsPath() );
        if ( m_usockLstnr.Start( 1 + getReplConf()->getCachedProcCnt()) != LS_OK )
        {
            LS_ERROR("Memcache usock listener failed to start");
            return LS_FAIL;
        }    
        LS_DBG_M("Memcache usock listener is up");
    }
    
    if (LsMemcache::getConfigReplication())
    {
        m_fdPassLstnr.SetMultiplexer( m_pMultiplexer );
        m_fdPassLstnr.SetListenerAddr(getReplConf()->getDispatchAddr());
        if ( m_fdPassLstnr.Start(-1, &m_usockLstnr) != LS_OK )
        {
            LS_ERROR(  "repl listener failed to start, addr=%s\n", getReplConf()->getLisenSvrAddr());
            return LS_FAIL;;
        }
    }
//    int cnt = getReplConf()->getCachedProcCnt();
    
//     LsRepl2CacheEvent *pR2cEventPtrs[cnt];
//     if ( !setupR2cEvents(cnt, m_pMultiplexer,pR2cEventPtrs) )
//     {
//         return LS_FAIL;
//     }


#ifdef notdef
    if( m_uRole == R_MASTER )
    {
        LsShmHash *pLsShmHash = LsShmMemcache::getInstance().getHash();
        assert( pLsShmHash != NULL );
        pLsShmHash->setEventNotifier(&m_lsShmEvent);
    }
#endif

    if (LsMemcache::getConfigReplication())
    {
        LcReplGroup::getInstance().setMultiplexer(m_pMultiplexer);
        LcReplGroup::getInstance().initReplConn();
        LcReplGroup::getInstance().setRef(&m_usockLstnr);
    }
    return LS_OK;
}

// bool LsmcdImpl::setupR2cEvents(int cnt, Multiplexer *pMultiplexer, LsRepl2CacheEvent **ppEvent)
// {
//     int procNo = 1;
//     m_pR2cEventArr = new LsRepl2CacheEvent [cnt];
//     LsRepl2CacheEvent *pEvent = m_pR2cEventArr;
//     while (--cnt >= 0)
//     {
//         pEvent->setIdx(procNo);
//         if (pEvent->initNotifier(pMultiplexer))
//         {
//             delete[] m_pR2cEventArr;
//             return NULL;
//         }
//         *ppEvent++ = pEvent;
//         ++pEvent;
//         ++procNo;
//     }
//     return true;
// }

int LsmcdImpl::reinitRepldMpxr(int procNo)
{
    assert(procNo == 0);
    Lsmcd::getInstance().setProcId(procNo);

    m_pReplListener.SetMultiplexer( m_pMultiplexer );
    m_pMultiplexer->add( &m_pReplListener, POLLIN | POLLHUP | POLLERR );

    m_fdPassLstnr.SetMultiplexer( m_pMultiplexer );
    m_pMultiplexer->add( &m_fdPassLstnr, POLLIN | POLLHUP | POLLERR );
    
    m_usockLstnr.SetMultiplexer( m_pMultiplexer );
    m_pMultiplexer->add( &m_usockLstnr, POLLIN | POLLHUP | POLLERR );
    
    // repld send this event(role changing)    
//     LsRepl2CacheEvent *pEvent = m_pR2cEventArr;
//     int cnt = getReplConf()->getCachedProcCnt();
//     while (--cnt >= 0)
//     {
//         m_pMultiplexer->add(pEvent, POLLHUP | POLLERR);
//         ++pEvent;
//     }

    LcReplGroup::getInstance().setMultiplexer(m_pMultiplexer);    
    //drop unused
    //m_pMemcacheListener.Stop();
    LcReplGroup::getInstance().setLstnrProc(true);
    return 0;
}

int LsmcdImpl::reinitCachedMpxr(int procNo)
{
    assert(procNo > 0  && procNo <= getReplConf()->getCachedProcCnt());
    
    m_pMemcacheListener.SetMultiplexer ( m_pMultiplexer );
    m_pMultiplexer->add( &m_pMemcacheListener, POLLIN | POLLHUP | POLLERR );

    //one **matached** cached receives this event
//     LsRepl2CacheEvent *pEvent = m_pR2cEventArr;
//     pEvent += procNo - 1; 
//     m_pMultiplexer->add(pEvent, POLLIN | POLLHUP | POLLERR);
    
    LsMemcache::getInstance().reinitMcConn(m_pMultiplexer);
    
    //no need to drop as multiplexer is renewed
    //m_pReplListener.Stop();
    //m_usockLstnr.Stop();
    
    LsMemcache::getInstance().setMultiplexer(m_pMultiplexer);
    LcReplGroup::getInstance().setLstnrProc(false);
    
    Lsmcd::getInstance().setProcId(procNo);
    connUsockSvr();
    return 0;    
}

    
int LsmcdImpl::reinitMultiplexer()
{
    if (m_pMultiplexer != NULL)
        delete m_pMultiplexer;
    m_pMultiplexer = new epoll();
    if ( m_pMultiplexer->init( 1024 ) == LS_FAIL )
    {
        delete m_pMultiplexer;
        m_pMultiplexer = new Poller();
    }
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

    pw = getpwnam( getReplConf()->getUser() );
    if ( pw == NULL )
    {
        pw = getpwnam( "nobody" );
    }
    gr = getgrnam( getReplConf()->getGroup() );
    if ( gr == NULL )
    {
        gr = getgrnam( "nobody" );
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

int LsmcdImpl::connUsockSvr()
{
    if (LsMemcache::getConfigReplication())
    {
        char pBuf[256];
        m_pUsockClnt = new UsockClnt();
        m_pUsockClnt->SetMultiplexer( m_pMultiplexer );
        snprintf(pBuf, sizeof(pBuf), "%s%d", getReplConf()->getCachedUsPath(),
                 Lsmcd::getInstance().getProcId());
        m_pUsockClnt->setLocalAddr(pBuf);
        m_pUsockClnt->connectTo(getReplConf()->getRepldUsPath());
    }
    return 0;
}

void LsmcdImpl::delUsockFiles()
{
    char pBuf[256];
    if( ::access(getReplConf()->getRepldUsPath(), F_OK ) != -1 )
    {
        ::remove(getReplConf()->getRepldUsPath());
    } 
    for(int i = 1; i <= getReplConf()->getCachedProcCnt() ; ++i)
    {
        snprintf(pBuf, sizeof(pBuf), "%s%d", getReplConf()->getCachedUsPath(), i);
        if( ::access(pBuf, F_OK ) != -1 )
        {
            ::remove(pBuf);
        }         
    }
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
        if ( (event = sEvents ) )
        {
            sEvents = 0;
            if ( event & HS_ALARM )
            {
                alarm( 1 );
                ProcessTimerEvent();
                if (getppid() == 1)
                    break;
            }
            if ( event & HS_STOP )
            {
                break;
            }
        }

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
//     if ( _pReplSvrImpl )
//     {
//         delete _pReplSvrImpl;
//         _pReplSvrImpl = NULL;
//     }
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
        getReplConf()->getSliceCount(),      // number of slices
        (const char **)getReplConf()->getShmFiles(),  // array of pointers to names
        getReplConf()->getShmHashName(), //->_pHashName,
        &parms) < 0)
        return -1;

    if (_pReplSvrImpl->PreEventLoop() < 0)
      return -1;

    //LcReplGroup::getInstance().setR2cEventRef(_pReplSvrImpl->m_pR2cEventArr);
    LcReplGroup::getInstance().setLstnrProc(true);
    if (_pReplSvrImpl->m_iNoCrashGuard)
    {
        setProcId(1);
        _pReplSvrImpl->connUsockSvr(); //cached procId starting with 1 
    }
    else
    {
        CrashGuard cg(this);

        int ret = cg.guardCrash( 1 + getReplConf()->getCachedProcCnt() );
        if (ret == -1)
            return 8;
        for( int i = 0; i< argc; ++i)
        {
            int n = strlen( argv[i]);
            memset(argv[i], 0 , n);
        }
        _pReplSvrImpl->reinitMultiplexer();

        if (ret == 0) //replicator
        {
            snprintf(argv[0], 80, "lsmcd - replicator");
            _pReplSvrImpl->reinitRepldMpxr(ret);
            ::sleep(1);
        }
        else
        {
            LS_DBG_M("Cached pid:%d, procNo:%d", getpid(), ret);
            snprintf(argv[0], 80, "lsmcd - cached #%02d", ret);
            _pReplSvrImpl->reinitCachedMpxr(ret);
        }
        _pReplSvrImpl->ChangeUid();
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
    pParms->m_anonymous = getReplConf()->getAnonymous();
    pParms->m_byUser    = getReplConf()->getByUser();
    pParms->m_nomemfail = getReplConf()->getNomemFail();
    pParms->m_iValMaxSz = getReplConf()->getValMaxSz();
    pParms->m_iMemMaxSz = getReplConf()->getMemMaxSz();
    pParms->m_userSize  = getReplConf()->getUserSize();
    pParms->m_hashSize  = getReplConf()->getHashSize();
    if (pParms->m_iMemMaxSz < 1024)
        pParms->m_iMemMaxSz *= 1024 * 1024; 
    return;
}

uint8_t Lsmcd::getProcId() const
{
    return m_procId;
}

void Lsmcd::setProcId(uint8_t procId)
{
    LS_DBG_M(" Lsmcd::setProcId, procId:%d", procId);
    m_procId = procId;
}

UsockClnt* Lsmcd::getUsockConn() const
{
    return _pReplSvrImpl->m_pUsockClnt;
}
