cmake_minimum_required(VERSION 2.8)

cmake_policy(SET CMP0002 OLD)


########### next target ###############

SET(util_STAT_SRCS objarray.cpp
   httpfetchdriver.cpp
   httpfetch.cpp
   ni_fio.c
   linkobjpool.cpp
   pidfile.cpp
   hashdatacache.cpp
   keydata.cpp
   tlinklist.cpp
   mysleep.c
   rlimits.cpp
   duplicable.cpp
   gfactory.cpp
   fdpass.cpp
   pcregex.cpp
   autostr.cpp
   staticobj.cpp
   pool.cpp
   hashstringmap.cpp
   ghash.cpp
   emailsender.cpp
   guardedapp.cpp
   crashguard.cpp
   iconnection.cpp
   dlinkqueue.cpp
   connpool.cpp
   gzipbuf.cpp
   vmembuf.cpp
   blockbuf.cpp
   stringlist.cpp
   semaphore.cpp
   refcounter.cpp
   gpointerlist.cpp
   linkedobj.cpp
   objpool.cpp
   gpath.cpp
   gstring.cpp
   poolalloc.cpp
   env.cpp
   xmlnode.cpp
   iovec.cpp
   accesscontrol.cpp
   signalutil.cpp
   loopbuf.cpp
   stringtool.cpp
   tsingleton.cpp
   base64.cpp
   pcutil.cpp
   daemonize.cpp
   autobuf.cpp
   #logidtracker.cpp
   datetime.cpp
   aho.cpp
   crc64.cpp
   crc16.cpp
   md5.cpp
   profiletime.cpp
   sysinfo/nicdetect.cpp
   sysinfo/systeminfo.cpp
   strmatchrescache.cpp
)

add_library(util STATIC ${util_STAT_SRCS})


