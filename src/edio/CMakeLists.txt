cmake_minimum_required(VERSION 2.8)
cmake_policy(SET CMP0002 OLD)


########### next target ###############

SET(edio_STAT_SRCS
   aiooutputstream.cpp
   bufferedos.cpp
   cacheos.cpp
   devpoller.cpp
   ediostream.cpp
   epoll.cpp
   eventnotifier.cpp
   eventreactor.cpp
   fdindex.cpp
   flowcontrol.cpp
   inputstream.cpp
   iochain.cpp
   kqueuer.cpp
   lookupfd.cpp
   multiplexer.cpp
   multiplexerfactory.cpp
   outputbuf.cpp
   outputstream.cpp
   poller.cpp
   pollfdreactor.cpp
   reactorindex.cpp
   rtsigio.cpp
   sigeventdispatcher.cpp
   sockconnprocessor.cpp
   socketlistener.cpp
   evtcbque.cpp
)

add_library(edio STATIC ${edio_STAT_SRCS})


