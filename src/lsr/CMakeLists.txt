
cmake_minimum_required(VERSION 2.8)
cmake_policy(SET CMP0002 OLD)



########### next target ###############

SET(lsr_STAT_SRCS
    ls_lock.c
    xxhash.c
)

add_library(lsr STATIC ${lsr_STAT_SRCS} )
