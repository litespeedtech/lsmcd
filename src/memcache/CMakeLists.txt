
cmake_minimum_required(VERSION 2.8)
cmake_policy(SET CMP0002 OLD)


########### next target ###############
#SET(shm_STAT_SRCS
#    lsmchashmulti.cpp
#    lsmemcache.cpp
#    lsmcsasl.cpp
#    memcacheconn.cpp
#    memcachelistener.cpp
#    lsmchashbyuser.cpp
#)

add_library(lsmemcache STATIC lsmchashmulti.cpp lsmemcache.cpp lsmcsasl.cpp memcacheconn.cpp memcachelistener.cpp lsmchashbyuser.cpp )

#target_sources(lsmemcache lsmchashmulti.cpp lsmemcache.cpp lsmcsasl.cpp memcacheconn.cpp memcachelistener.cpp lsmchashbyuser.cpp)

#add_library(lsmemcache SHARED ${shm_STAT_SRCS} )

