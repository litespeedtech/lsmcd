
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

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
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

LsmcdImpl::LsmcdImpl()
        : m_iNoCrashGuard( 0 )
        , m_iNoDaemon( 0 )
        , m_pMultiplexer(NULL)
        , m_pUsockClnt(NULL)
        , m_uid(0)
        , m_gid(0)
        , m_gotUid(false)
        , m_deleteCoredumps(false)
        , m_setDeleteCoredumps(false)
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


void LsmcdImpl::needDeleteCoredumps()
{
    char need_str[80];
    m_setDeleteCoredumps = true;
    int fd = open("/proc/sys/kernel/core_uses_pid", O_RDONLY);
    if (fd == -1)
    {
        LS_NOTICE("Unable to determine if excess coredumps need to be deleted: "
                  "Can't open /proc/sys/kernel/core_uses_pid: %s\n", 
                  strerror(errno));
        return;
    }
    ssize_t did_read = read(fd, need_str, sizeof(need_str) - 1);
    if (did_read > 0 && need_str[0] == '1')
    {
        LS_NOTICE("Only the oldest coredump will be preserved\n");
        m_deleteCoredumps = true;
    }
    else
        LS_DBG_M("Excess coredumps will not be deleted\n");
    close(fd);
}


void LsmcdImpl::doDeleteCoredumps()
{
    LS_DBG_M("In doDeleteCoredumps: did set: %d, need delete: %d\n",
             m_setDeleteCoredumps, m_deleteCoredumps);
    if (!m_setDeleteCoredumps)
        needDeleteCoredumps();
    if (!m_deleteCoredumps)
        return;
    DIR *d = opendir(".");
    if (!d)
    {
        LS_DBG_M("Attempt to open default directory failed: %s\n", strerror(errno));
        return;
    }
    struct dirent *ent;
    time_t oldest_time = 0;
    char oldest[256] = { 0 };
    while ((ent = readdir(d)))
    {
        LS_DBG_M("Readdir: %s\n", ent->d_name);
        if (strcmp(ent->d_name, ".") && strcmp(ent->d_name, ".."))
        {
            struct stat st;
            if (!stat(ent->d_name, &st))
            {
                if (S_ISREG(st.st_mode) && !strncmp("core.", ent->d_name, 5))
                {
                    bool new_oldest = oldest_time && st.st_mtime < oldest_time;
                    if (oldest_time)
                    {
                        char *newer = new_oldest ? oldest : ent->d_name;
                        LS_DBG_M("Delete coredump: %s\n", newer);
                        if (remove(newer))
                            LS_NOTICE("Error deleting coredump %s: %s\n", 
                                      oldest, strerror(errno));
                    }
                    if (!oldest_time || new_oldest)
                    {
                        oldest_time = st.st_mtime;
                        strncpy(oldest, ent->d_name, sizeof(oldest));
                    }
                }
                else
                    LS_DBG_M("Not a coredump or regular file: %s\n", ent->d_name);
            }
        }
    }
    closedir(d);
}


void LsmcdImpl::enableCoreDump()
{
    if (!getuid())
    {
        uid_t uid;
        gid_t gid;
        getUid(&uid, &gid);
        if (lchown(getReplConf()->getTmpDir(), uid, gid))
            LS_DBG_M("Error setting owner for core dump directory: %s\n", strerror(errno));
        else
            LS_DBG_M("Set owner for core dump directory to %d:%d\n", uid, gid);
    }
    doDeleteCoredumps();
    struct  rlimit rl;
    if (getrlimit(RLIMIT_CORE, &rl) == -1)
        LS_WARN("getrlimit( RLIMIT_CORE, ...) failed!");
    else
    {
        //LS_DBG_L( "rl.rlim_cur=%d, rl.rlim_max=%d", rl.rlim_cur, rl.rlim_max );
        if ((getuid() == 0) && (rl.rlim_max < 10240000))
            rl.rlim_max = 10240000;
        rl.rlim_cur = rl.rlim_max;
        if (::setrlimit(RLIMIT_CORE, &rl))
            LS_WARN("Error enabling coredump: %s\n", strerror(errno));
    }

    LS_DBG_M("UID: %d PID: %d Enabling coredump at %s\n", getuid(), getpid(), getReplConf()->getTmpDir());
    if (::chdir(getReplConf()->getTmpDir()))
        LS_DBG_M("Error changing to coredump directory: %s\n", strerror(errno));

    if (prctl(PR_SET_DUMPABLE, 1) == -1)
        LS_WARN("prctl: Failed to set dumpable %s, core dump may not be available\n", strerror(errno));

    {
        int dumpable = prctl(PR_GET_DUMPABLE);

        if (dumpable == -1)
            LS_WARN("prctl: get dumpable failed ");
    }
}


int LsmcdImpl::Init( int argc, char *argv[] )
{
    if(argc >= 1 ){
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

    enableCoreDump();
    
    setReplGroup(new LcReplGroup());
    delUsockFiles();
    
    return LS_OK;
}


void LsmcdImpl::getUid(uid_t *uid, gid_t *gid)
{
    if (!m_gotUid)
    {
        m_gotUid = true;
        m_uid = geteuid();
        m_gid = getegid();
        const char *name;
        struct group  *gr = NULL;
        struct passwd *pw = NULL;

        name = getReplConf()->getUser();
        if (name)
            pw = getpwnam(name);
        if ( pw == NULL )
        {
            pw = getpwnam( "nobody" );
        }
        if (pw)
            m_uid = pw->pw_uid;
        
        name = getReplConf()->getGroup();
        if (name)
            gr = getgrnam(name);
        if ( gr == NULL )
        {
            gr = getgrnam( "nobody" );
        }
        if ( gr == NULL )
            gr = getgrnam( "nogroup" );
        if (gr)
            m_gid = gr->gr_gid;
    }
    *uid = m_uid;
    *gid = m_gid;
}


int LsmcdImpl::SetUDSAddrFile()
{
    uid_t uid;
    gid_t gid;
    getUid(&uid, &gid);
    {
        const char *addr = getReplConf()->getMemCachedAddr() + 4; // UDS:
        while ((*addr == '/') && (*(addr + 1) == '/'))
            addr++;
        if (*addr == '/')
        {
            if (lchown(addr, uid, gid) == -1)
            {
                LS_WARN("Error setting UDS file %s to UID: %ld GID: %ld: %s\n", 
                        addr, (long)uid, (long)gid, strerror(errno));
                return -1; // Not that we look at it.
            }
            else 
                LS_DBG_M("Set UDS file %s to UID: %ld, GID: %ld\n", addr, 
                         (long)uid, (long)gid);
        }
    }
    return 0;
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
    
    bool uds = !strncasecmp(getReplConf()->getMemCachedAddr(), "UDS:", 4);
    int mask;
    if ((uds) && (!geteuid()))
        mask = umask(0);
    m_pMemcacheListener.SetMultiplexer ( m_pMultiplexer );
    m_pMemcacheListener.SetListenerAddr ( getReplConf()->getMemCachedAddr() );
    if ( m_pMemcacheListener.Start() != LS_OK )
    {
        if ((uds) && (!geteuid()))
            umask(mask);
        LS_ERROR("Memcache Listener failed to start");
        return LS_FAIL;
    }
    if ((uds) && (!geteuid()))
    {
        SetUDSAddrFile();
        umask(mask);
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
    assert(procNo > 0  && procNo <= getReplConf()->getCachedProcCnt() + 1);
    
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
    bool didConf = false;
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
            didConf = true;
            break;
        case 'v':
            printf( "lsmcd server %s\n", VERSION_TO_LOG );
            exit( 0 );
            break;
        case '?':
            break;

        default:
            printf ("?? getopt returned character code -%o ??\n", c);
        }
    }
    if (!didConf)
    {
        ConfWrapper::getInstance().loadConf(new LcReplConf());
        if (!getReplConf()->parse("/usr/local/lsmcd/conf/node.conf"))
            return LS_FAIL;
    }
    LS_NOTICE("LSMCD/" VERSION_TO_LOG " LiteSpeed Memcached Replacement\n");

    return LS_OK;
}

int LsmcdImpl::ChangeUid()
{
    uid_t uid;
    gid_t gid;
    getUid(&uid, &gid);
    if ( setgid(gid) < 0 )
    {
        return -1;
    }

    if ( setuid(uid) < 0 )
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
    for(int i = 1; i <= getReplConf()->getCachedProcCnt() + 1; ++i)
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
    int ret;
    int event;
    alarm( 1 );
    //ReplConn::s_curTime = time( NULL );
    while( true )
    {
        ret = m_pMultiplexer->waitAndProcessEvents(
                    MLTPLX_TIMEOUT );
        int err = errno;
        errno = err;
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
    SignalUtil::signal( SIGPIPE, SIG_IGN );
    SignalUtil::signal( SIGALRM, sig_alarm );
    SignalUtil::signal( SIGUSR1, sig_usr1 );
    SignalUtil::signal( SIGINT, sig_quit );
}

int Lsmcd::Main(int argc, char **argv)
{
#define MAX_CMD 256
    char cwd[MAX_CMD];
    getcwd(cwd, sizeof(cwd));
    
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
        LS_DBG_M("GuardCrash ret: %d\n", ret);
        if (ret == -1)
            return 8;
        if (ret == HS_USR1 && !getuid())
        {
            chdir(cwd);
            return execv(argv[0], argv);
        }
            
        //int zeroLen = strlen(argv[0]);
        for( int i = 1; i< argc; ++i)
        {
            int n = strlen( argv[i]);
            memset(argv[i], 0 , n);
        }
        _pReplSvrImpl->reinitMultiplexer();

        if (ret == 0) //replicator
        {
            //snprintf(argv[0], zeroLen, "lsmcd - replicator");
            _pReplSvrImpl->reinitRepldMpxr(ret);
            ::sleep(1);
        }
        else
        {
            LS_DBG_M("Cached pid:%d, procNo:%d", getpid(), ret);
            //snprintf(argv[0], zeroLen, "lsmcd - cached #%02d", ret);
            _pReplSvrImpl->reinitCachedMpxr(ret);
        }
        _pReplSvrImpl->ChangeUid();
        _pReplSvrImpl->enableCoreDump();
    }
    
    init_signals();

    int rc = _pReplSvrImpl->EventLoop();

    ReleaseAll();

    return rc;
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
    pParms->m_dbgValidate = getReplConf()->getDbgValidate();
    if (pParms->m_dbgValidate)
    {
        LS_DBG_M("Enable Debug Validation in debugging\n");
        LsMemcache::setDbgValidate(pParms->m_dbgValidate);
    }

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


int Lsmcd::childSignaled(int seq, pid_t pid, int signal, int coredump)
{
    if (signal == SIGUSR2)
    {
        char *shmdir = (char *)"/dev/shm/lsmcd";
        LS_NOTICE("Deleting the shared memory database\n");
        DIR *dir = opendir(shmdir);
        if (!dir)
            LS_ERROR("Unable to open the shared memory directory: %s\n", 
                     strerror(errno));
        else 
        {
            struct dirent *ent;
            while ((ent = readdir(dir)))
            {
                if (ent->d_type == DT_REG && !strncmp(ent->d_name, "data", 4))
                {
                    char filename[512];
                    snprintf(filename, sizeof(filename), "%s/%s", shmdir, 
                             ent->d_name);
                    LS_DBG_M("Deleting %s\n", filename);
                    if (unlink(filename))
                        LS_NOTICE("Error deleting %s: %s\n", filename, 
                                  strerror(errno));
                }
            }
            LS_DBG_M("Restarting service with no databases...\n");
            kill(getpid(), SIGUSR1);
        }
    }
    return 0;
}
