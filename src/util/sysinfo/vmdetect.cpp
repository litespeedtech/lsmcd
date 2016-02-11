
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define VM_OPENVZ   1
#define VM_XEN      2
#define VM_VMWARE   3
#define VM_KVM      4
#define VM_HYPERV   5
#define VM_USERMODELINUX   6


static int  readFile(char *pBuf, int bufLen, const char *pName,
                     const char *pBase)
{
    char achFilePath[512];
    int fd;
    fd = open(achFilePath, O_RDONLY, 0644);
    if (fd == -1)
        return -1;
    int ret = read(fd, pBuf, bufLen);
    close(fd);
    return ret;
}

static int detectVZ()
{
    int ret = 0;
#if defined(linux) || defined(__linux) || defined(__linux__)
    struct stat st;
    if (stat("/proc/vz/", &st) == 0)
    {
        if ((stat("/proc/vz/vestat", &st) == 0) ||
            (stat("/proc/vz/veinfo", &st) == 0))
        {
            if (stat("/proc/vz/vzquota", &st) == -1)
                return VM_OPENVZ;
        }
        int s = system("dmidecode 2>&1 | grep \"/dev/mem\" | grep -e \"\\(denied\\|not permitted\\)\" > /dev/null");
        if (WEXITSTATUS(s) == 0)
            return VM_OPENVZ;
    }
#endif
    return ret;
}


#include <setjmp.h>
#include <signal.h>
#if defined(macintosh) || defined(__APPLE__) || defined(__APPLE_CC__)
static int isXEN()
{
    return 0;
}
#else
#if defined(__i386__)||defined( __x86_64 )||defined( __x86_64__ )
static unsigned int getcpuid(unsigned int eax, char *sig)
{
    unsigned int *sig32 = (unsigned int *) sig;

    asm volatile(
        "xchgl %%ebx,%1; xor %%ebx,%%ebx; cpuid; xchgl %%ebx,%1"
        : "=a"(eax), "+r"(sig32[0]), "=c"(sig32[1]), "=d"(sig32[2])
        : "0"(eax));
    sig[12] = 0;

    return eax;
}

static int get_vm_type_by_cpuid(const char *cpuid)
{
    if (strncmp("XenVMMXenVMM", cpuid, 12) == 0)
        return VM_XEN;
    else if (strncmp("VMwareVMware", cpuid, 12) == 0)
        return VM_VMWARE;
    else if (strncmp("KVMKVMKVM", cpuid, 9) == 0)
        return VM_KVM;
    else if (strncmp("Microsoft Hv", cpuid, 12) == 0)
        return VM_HYPERV;

}

static int vm_by_cpuid(void)
{
    int ret;
    char sig[13];
    unsigned int base = 0x40000000, leaf = base;
    unsigned int max_entries;

    memset(sig, 0, sizeof sig);
    max_entries = getcpuid(leaf, sig);
    ret = get_vm_type_by_cpuid(sig);
    if (ret)
        return ret;
    if (max_entries > 3 && max_entries < 0x10000)
    {
        for (leaf = base + 0x100; leaf <= base + max_entries; leaf += 0x100)
        {
            memset(sig, 0, sizeof sig);
            getcpuid(leaf, sig);
            ret = get_vm_type_by_cpuid(sig);
            if (ret)
                return ret;
        }
    }
    return 0;
}


static int detect_XEN_domU()
{
    int ret = 0;
    struct stat st;
    if (stat("/proc/xen/capabilities", &st) == 0)
    {
        char achBuf[4096];
        int len = readFile(achBuf, 4096, "/proc/xen/capabilities", "");
        if (strncmp(achBuf, "control_d", 9) != 0)
            ret = VM_XEN;
    }
    return ret;
}
#endif //defined(__i386__)||defined( __x86_64 )||defined( __x86_64__ )
#endif

static jmp_buf no_vmware;

static void handler(int sig)
{
    longjmp(no_vmware, 1);
}

#define VMWARE_MAGIC        0x564D5868  // Backdoor magic number
#define VMWARE_PORT         0x5658      // Backdoor port number
#define VMCMD_GET_VERSION   0x0a        // Get version number


static int _isVMWare = 0;

#if defined(__i386__)||defined( __x86_64 )||defined( __x86_64__ )
static int isVMware()
{

    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    int ret = 0;
#ifdef __FreeBSD__
#   define ERROR_SIGNAL SIGBUS
#else
#   define ERROR_SIGNAL SIGSEGV
#endif
    signal(ERROR_SIGNAL, handler);
    if (setjmp(no_vmware) == 0)
    {
        __asm__ volatile("inl (%%dx)" :
                         "=a"(eax), "=c"(ecx), "=d"(edx), "=S"(ebx) :
                         "0"(VMWARE_MAGIC), "1"(VMCMD_GET_VERSION),
                         "2"(VMWARE_PORT) : "memory");
        ret = VM_VMWARE;
    }
    signal(ERROR_SIGNAL, SIG_DFL);

    return ret;
}
#endif

/*
void detectVMWare()
{
#if defined(__i386__)||defined( __x86_64 )||defined( __x86_64__ )
//#ifdef NDEBUG
    int pid = fork();
    if ( pid < 0)
       perror( "fork() failed" );
    else if ( pid == 0 )
    {
        struct  rlimit rl;
        rl.rlim_cur = rl.rlim_max = 0 ;
        ::setrlimit( RLIMIT_CORE, &rl );

        exit( isVMware() );
    }
    else
    {
        int  stat;
        for( int i = 0; i < 10; ++i )
        {
            usleep( 100000 );
            if ( waitpid( pid, &stat, WNOHANG ) == pid )
            {
                if ( WIFEXITED( stat ) )
                {
                    if ( WEXITSTATUS( stat ) == 1 )
                        _isVMWare = 3;
                }
                return;
            }
        }
        kill( pid, SIGKILL );
    }


//#endif
#endif
}
*/

static int isUserModeLinuxOrKvm()
{
    int ret = 0;
#if defined(linux) || defined(__linux) || defined(__linux__)
    char achBuf[4096];
    int len = readFile(achBuf, 4095, "/proc/cpuinfo", "");
    achBuf[len] = 0;
    if (strstr(achBuf, "User Mode Linux"))
        ret = VM_USERMODELINUX;
    else if (strstr(achBuf, "QEMU Virtual CPU"))
        ret = VM_KVM;
#endif
    return ret;
}

static int isHyperV()
{
#if defined(linux) || defined(__linux) || defined(__linux__)
    char achBuf[4096];
    int len = readFile(achBuf, 4096, "/proc/ide/hdc/model", "");
    if ((len > 0) && (strncmp(achBuf, "Virtual ", 8) == 0))
    {
        len = readFile(achBuf, 4096, "/proc/acpi/fadt", "");
        if (len > 0)
        {
            char *pEnd = &achBuf[len];
            char *p = achBuf;
            while (p < pEnd)
            {
                p = (char *)memchr(p, 'V', pEnd - p);
                if (p)
                {
                    if (strncmp(p + 1, "RTUAL", 5) == 0)
                        return VM_HYPERV;
                    ++p;
                }
                else
                    break;
            }
        }
    }
#endif
    return 0;
}


static int detectVPS()
{
    int ret = 0;
    ret = vm_by_cpuid();
    if (!ret)
        ret = detectVZ();
    if (!ret)
        ret = isVMware();
    if (!ret)
        ret = isUserModeLinuxOrKvm();
    if (!ret)
        ret = isHyperV();
    return ret;
}

