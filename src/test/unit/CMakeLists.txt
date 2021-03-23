cmake_minimum_required(VERSION 2.8)

SET(unittest_STAT_SRCS
   unittest_main.cpp
)

#link_directories(${PROJECT_SOURCE_DIR}/../thirdparty/lib/ /usr/local/lib /usr/lib64)

add_executable(lsmcd_unittest
    ${unittest_STAT_SRCS}
)

SET( unittestlib
     ${CMAKE_DL_LIBS} )

target_link_libraries(lsmcd_unittest ${unittestlib} )

