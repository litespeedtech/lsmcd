cmake_minimum_required(VERSION 2.8)

project(lsmcd)

SET(CMAKE_INCLUDE_CURRENT_DIR ON)
include_directories( include )
include_directories( src )
set(MY_CMAKE_WARNING_FLAG  " -g3 -Wall -Wextra -Wno-unused-parameter")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  ${MY_CMAKE_WARNING_FLAG}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  ${MY_CMAKE_WARNING_FLAG}")


add_subdirectory(src)