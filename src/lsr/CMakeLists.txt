
cmake_minimum_required(VERSION 2.8)
cmake_policy(SET CMP0002 OLD)



########### next target ###############

SET(lsr_STAT_SRCS
    ls_lock.c
    xxhash.c
    ls_hash.c
    ls_xpool.c
    ls_pool.c
)

add_library(lsr STATIC ${lsr_STAT_SRCS} )
