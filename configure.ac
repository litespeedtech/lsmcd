m4_include(ax_check_zlib.m4)
m4_include(ax_check_openssl.m4)
m4_include(ax_check_sasl.m4)

dnl Process this file with autoconf to produce a configure script.
AC_PREREQ([2.69])
AC_INIT([lsmcd],[1.0.01],[info@litespeedtech.com],[lsmcd],[http://www.litespeedtech.com/])
AM_INIT_AUTOMAKE([1.0 foreign no-define ])

AM_CONFIG_HEADER(src/config.h:src/config.h.in)

dnl NOW change the default installation directory!
AC_PREFIX_DEFAULT('/usr/local/lsmcd')

# General "with" options
# ----------------------------------------------------------------------------
dnl Checks for programs.

AC_PROG_MAKE_SET
AC_PROG_INSTALL

# Checks for programs.
AC_PROG_AWK
AC_PROG_CXX
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB
LT_INIT

# Checks for header files.

AC_CHECK_HEADERS(arpa/inet.h fcntl.h inttypes.h limits.h netdb.h netinet/in.h stddef.h stdlib.h string.h strings.h sys/ioctl.h sys/param.h sys/socket.h sys/time.h unistd.h)

AC_HEADER_ASSERT

CFLAGS="$CFLAGS -fstack-protector $(getconf LFS_CFLAGS) "
CXXFLAGS="$CXXFLAGS -fstack-protector $(getconf LFS_CFLAGS) "



AC_ARG_ENABLE([rpath],
              [AC_HELP_STRING([--disable-rpath], 
                              [Disable rpath (It is 'no' by default)])],
              [OPENLSWS_DISABLE_RPATH=yes              
               echo "OPENLSWS_DISABLE_RPATH=yes"], [OPENLSWS_DISABLE_RPATH=no])                
AC_SUBST([OPENLSWS_DISABLE_RPATH])
               
AC_ARG_WITH([libdir],
            AS_HELP_STRING([--with-libdir],[Set system lib directory. It is lib or lib64 and will be automatically checked by default]),
            [OPENLSWS_LIBDIR="$withval"], [
                OSTYPE=`uname -m`
                OSNAME=`uname -s`
                OPENLSWS_LIBDIR=lib
                if test "$OSNAME" = Linux ; then
                    if test "$OSTYPE" = x86_64 ; then
                        OPENLSWS_LIBDIR=lib64
                    elif test "$OSTYPE" = i686 ; then
               
                        CFLAGS="$CFLAGS -march=i686"
                        CXXFLAGS="$CXXFLAGS -march=i686"
                        elif test "$OSTYPE" = armv7l ; then
                       # Raspberry Pi 2, Scaleway C1, Parallella
                        CFLAGS="$CFLAGS -march=armv7"
                        CXXFLAGS="$CXXFLAGS -march=armv7"
                    elif test "$OSTYPE" = armv6l ; then
                       # Raspberry Pi (not Pi 2)
                        CFLAGS="$CFLAGS -march=armv6 -mfloat-abi=hard -mfpu=vfp"
                        CXXFLAGS="$CXXFLAGS -march=armv6 -mfloat-abi=hard -mfpu=vfp"
                        
                    fi
                fi
             ])
echo "Final CFLAGS='$CFLAGS' CXXFLAGS='$CXXFLAGS'"
             
echo "Default system lib directory = $OPENLSWS_LIBDIR"
AC_SUBST([OPENLSWS_LIBDIR])


AX_CHECK_ZLIB(, AC_MSG_ERROR(Can not find zlib. You must install it before continuing.))
AX_CHECK_OPENSSL(, AC_MSG_ERROR(Can not find openssl. You must install it before continuing.))
AC_CHECK_LIB(crypt,crypt,LIBS="-lcrypt $LIBS")
     
      
# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_UID_T
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT8_T
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_CHECK_TYPES(ptrdiff_t)

# Checks for library functions.
AC_FUNC_CHOWN
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_FUNC_FSEEKO
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
#AC_FUNC_MALLOC disable due to this replacing malloc() with non-existing rpl_malloc() issue on some systems
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_CHECK_FUNCS(malloc bzero dup2 ftruncate getcwd gethostbyaddr gethostbyname gettimeofday inet_ntoa localtime_r memchr memmove memset mkdir munmap select socket strcasecmp strchr strcspn strdup strerror strncasecmp strpbrk strrchr strspn strstr strtol)
CMU_SASL

RT_LIB_OPTION=" -lrt" 
DL_LIB_OPTION=" -ldl"
if test `uname -s` = "Linux"
then
    LDFLAGS="$LDFLAGS -Wl,--export-dynamic"
fi
if test `uname -s` = "FreeBSD"
then
    LDFLAGS="$LDFLAGS -Wl,--export-dynamic"
    DL_LIB_OPTION=
fi

#For Mac OS, add special falgs
if test `uname -s` = "Darwin"
then
    LDFLAGS="$LDFLAGS -Wl,-export_dynamic -pagezero_size 10000 -image_base 100000000"
    RT_LIB_OPTION=
fi

LDFLAGS="$LDFLAGS $PCRE_LDFLAGS $LIB_SASL"
echo "Final LDFLAGS='$LDFLAGS'"
AC_SUBST([DL_LIB_OPTION])
AC_SUBST([RT_LIB_OPTION])

AC_CONFIG_FILES(Makefile
                src/Makefile
                src/lcrepl/Makefile
                src/repl/Makefile
                src/memcache/Makefile
                src/util/sysinfo/Makefile
                src/edio/Makefile
                src/log4cxx/Makefile
                src/socket/Makefile
                src/shm/Makefile
                src/util/Makefile
                src/lsr/Makefile)
AC_OUTPUT

