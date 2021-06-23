cmake_minimum_required(VERSION 2.8)

project(lsmcd)

SET(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories( include )
include_directories( src )

set(CMAKE_BUILD_TYPE "Debug")
set(SANITIZE false)
IF("${CMAKE_BUILD_TYPE}" STREQUAL "")
    SET(CMAKE_BUILD_TYPE Release)
ENDIF()

MESSAGE(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

IF (CMAKE_BUILD_TYPE STREQUAL "Debug")
    if (SANITIZE)
        set(MY_CMAKE_WARNING_FLAG " -g -Wall -Wextra -Wno-unused-parameter -fsanitize=address -fno-omit-frame-pointer")
    else()
        set(MY_CMAKE_WARNING_FLAG " -g")
    endif()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  ${MY_CMAKE_WARNING_FLAG}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  ${MY_CMAKE_WARNING_FLAG}")

    if (SANITIZE)
        set (CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fno-omit-frame-pointer -fsanitize=address -g")
        set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fno-omit-frame-pointer -fsanitize=address -g")
        set (CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -fsanitize=address -fno-omit-frame-pointer -g")
    else()
        set (CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -g")
        set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -g")
        set (CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -g")
    endif()
ELSE()
    set(MY_CMAKE_WARNING_FLAG  " -O3 -Wall -Wextra -Wno-unused-parameter -fno-omit-frame-pointer")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  ${MY_CMAKE_WARNING_FLAG}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  ${MY_CMAKE_WARNING_FLAG}")

    set (CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fno-omit-frame-pointer")

    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fno-omit-frame-pointer")

ENDIF()

add_subdirectory(src)

