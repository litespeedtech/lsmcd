
add_subdirectory(lsr)
add_subdirectory(log4cxx)
add_subdirectory(util)
add_subdirectory(lcrepl)
add_subdirectory(repl)
add_subdirectory(edio)
add_subdirectory(shm)
add_subdirectory(socket)
add_subdirectory(memcache)

add_executable(lsmcd
        lsmcdmain.cpp
        lsmcd.cpp
)

# add_executable(ls_shmhashstat
#         memcacheconn.cpp
#         memcachelistener.cpp
#         shm_lscached.cpp
#         shm/ls_shmhashstat.cpp
# )


link_directories()

#target_link_libraries(replicator replicate log4cxx util pthread)
#target_link_libraries(replicator lsshm replicate log4cxx edio util pthread sasl2 rt z)
target_link_libraries(lsmcd lcrepl lsmemcache repl lsshm lsr log4cxx edio socket util pthread sasl2 rt z crypt ssl crypto   )
#target_link_libraries(lsmcd lcrepl lsmemcache repl lsshm lsr log4cxx edio socket util pthread dl db /home/user/proj/cyrus-sasl/lib/.libs/libsasl2.a rt z crypt ssl crypto   )
#target_link_libraries(ls_shmhashstat lsmemcache lsshm replicate lsr log4cxx edio socket util pthread sasl2 rt)

