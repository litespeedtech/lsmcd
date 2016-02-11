bin_PROGRAMS = lsmcd
lsmcd_SOURCES = lsmcd.cpp lsmcdmain.cpp

lsmcd_LDADD =  $(top_builddir)/src/memcache/liblsmemcache.a \
        $(top_builddir)/src/lcrepl/liblcrepl.a \
        $(top_builddir)/src/repl/librepl.a   \
        $(top_builddir)/src/log4cxx/liblog4cxx.a \
        $(top_builddir)/src/edio/libedio.a \
        $(top_builddir)/src/util/sysinfo/libsysinfo.a \
        $(top_builddir)/src/socket/libsocket.a \
        $(top_builddir)/src/shm/liblsshm.a \
        $(top_builddir)/src/util/libutil.a \
        $(top_builddir)/src/lsr/liblsr.a \
	$(EXPAT_LIBS) $(OPENSSL_LIBS) $(PCRE_LIBS) -lz -lexpat -lpthread $(DL_LIB_OPTION) $(RT_LIB_OPTION)



AM_CPPFLAGS = $(PCRE_CFLAGS) $(OPENSSL_INCLUDES) $(EXPAT_CFLAGS)
AM_LDFLAGS = -L/usr/lib $(OPENSSL_LDFLAGS)
INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/src
SUBDIRS = log4cxx edio memcache lcrepl repl socket shm util lsr


# the library search path.
lsmcd_LDFLAGS = $(all_libraries)
