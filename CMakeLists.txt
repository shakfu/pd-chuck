cmake_minimum_required(VERSION 3.19)

project(pd_chuck)

set(CMAKE_CXX_STANDARD 14)


add_subdirectory(thirdparty/chuck/core)
add_subdirectory(thirdparty/chuck/host)
add_subdirectory(thirdparty/chugins)
add_subdirectory(chuck~)

