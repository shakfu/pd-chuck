cmake_minimum_required(VERSION 3.19)

project(pd_chuck)


# use ccache if available
# find_program(CCACHE_PROGRAM ccache)
# if(CCACHE_PROGRAM)
#     message(STATUS "Found ccache in ${CCACHE_PROGRAM}")
#     set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
# endif()


set(CMAKE_CXX_STANDARD 14)

add_subdirectory(thirdparty/chuck/core)

add_subdirectory(thirdparty/chuck/host)

add_subdirectory(thirdparty/chugins)

add_subdirectory(chuck~)
