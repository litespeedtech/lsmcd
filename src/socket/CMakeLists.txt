cmake_minimum_required(VERSION 2.8)
cmake_policy(SET CMP0002 OLD)

########### next target ###############

SET(socket_STAT_SRCS
   gsockaddr.cpp
   coresocket.cpp
)

add_library(socket STATIC ${socket_STAT_SRCS})


