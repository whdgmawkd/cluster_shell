cmake_minimum_required(VERSION 3.20)
project(ClusterShell)

set(CMAKE_CXX_STANDARD 11)

file(GLOB_RECURSE CLSH_SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "cluster_shell/*")

add_executable(clsh ${CLSH_SOURCES})

file(GLOB_RECURSE SINGLE_NODE_SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "single_node/*")

add_executable(single_node ${SINGLE_NODE_SOURCES})

find_library(LIB_RT rt)
if(LIB_RT)
    target_link_libraries(single_node ${LIB_RT})
endif()
