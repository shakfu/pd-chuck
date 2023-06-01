cmake_minimum_required(VERSION 3.19)

project(pd_chuck)

set(CMAKE_CXX_STANDARD 14)


add_subdirectory(core)
add_subdirectory(host)
add_subdirectory(chugins)
add_subdirectory(chuck~)

