
cmake_minimum_required(VERSION 2.8)
cmake_policy(SET CMP0002 OLD)


########### next target ###############

SET(shm_STAT_SRCS
    lsmchashbyuser.cpp
    lsmchashmulti.cpp
    lsmemcache.cpp
    lsmcsasl.cpp
    memcacheconn.cpp
    memcachelistener.cpp
)

add_library(lsmemcache STATIC ${shm_STAT_SRCS} )

#add_library(lsmemcache SHARED ${shm_STAT_SRCS} )

