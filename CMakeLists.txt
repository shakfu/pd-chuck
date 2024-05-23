cmake_minimum_required(VERSION 3.19)

project(pd_chuck)

# linux options
option(LINUX_ALSA "enable linux alsa build")
option(LINUX_PULSE "enable linux pulseaudio build")
option(LINUX_JACK "enable linux jack build")
option(LINUX_ALL "enable all linux build")

# macos options
option(ENABLE_HOMEBREW "Build using Homebrew dependencies")
option(BUILD_UNIVERSAL "Build Universal Binaries")

# chugin options
option(ENABLE_WARPBUF "build WarpBuf chugin")
option(ENABLE_FAUCK "build Faust chugin")
option(ENABLE_MP3 "Enable MP3 support for libsndfile")
option(ENABLE_EXTRA_FORMATS "Enable extra file formats for libsndfile")

# dev options
option(DEV_WARNINGS "use additional warning settings durine development")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version" FORCE)

# use ccache if available
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Found ccache in ${CCACHE_PROGRAM}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

set(local_prefix "/usr/local")

if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
	message(STATUS "Platform: Linux")

	if(NOT LINUX_ALSA OR LINUX_PULSE OR LINUX_JACK)
		# message(FATAL_ERROR "select linux option")
		set(LINUX_ALSA "ON" CACHE BOOL "enable alsa as default linux option" FORCE)
	elseif(LINUX_ALL)
		set(LINUX_ALSA "ON" CACHE BOOL "enable alsa linux option" FORCE)
		set(LINUX_PULSE "ON" CACHE BOOL "enable pulseaudio linux option" FORCE)
		set(LINUX_JACK "ON" CACHE BOOL "enable jack linux option" FORCE)
	endif()

	message(STATUS "LINUX_ALSA ENABLED: ${LINUX_ALSA}")
	message(STATUS "LINUX_PULSE ENABLED: ${LINUX_PULSE}")
	message(STATUS "LINUX_JACK ENABLED: ${LINUX_JACK}")

    EXECUTE_PROCESS( COMMAND uname -m COMMAND tr -d '\n' OUTPUT_VARIABLE ARCHITECTURE )
    message(STATUS "Architecture: ${ARCHITECTURE}" )

    if(DEFINED ARCHITECTURE)
        set(LOCAL_LIB /usr/lib/${ARCHITECTURE}-linux-gnu)
        set(LOCAL_INCLUDE /usr/include/${ARCHITECTURE}-linux-gnu)
    endif()
endif()

if (APPLE)
    if (NOT CMAKE_OSX_ARCHITECTURES)
        if(BUILD_UNIVERSAL)
            set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "macOS architecture" FORCE)
        else()
            set(CMAKE_OSX_ARCHITECTURES ${CMAKE_SYSTEM_PROCESSOR} CACHE STRING "macOS architecture" FORCE)
        endif()
        message("CMAKE_OSX_ARCHITECTURES set to ${CMAKE_OSX_ARCHITECTURES}")
    endif()

    execute_process(
        COMMAND brew --prefix
        OUTPUT_VARIABLE homebrew_prefix
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(DEFINED homebrew_prefix AND ENABLE_HOMEBREW)
        set(local_prefix ${homebrew_prefix})
    else()
        set(local_prefix ${CMAKE_SOURCE_DIR}/build/thirdparty/install)
    endif()
        set(LOCAL_LIB ${local_prefix}/lib)
        set(LOCAL_INCLUDE ${local_prefix}/include)
endif()
message(STATUS "LOCAL_LIB: ${LOCAL_LIB}")
message(STATUS "LOCAL_INCLUDE: ${LOCAL_INCLUDE}")


add_subdirectory(thirdparty/chuck/core)
add_subdirectory(thirdparty/chuck/host)
add_subdirectory(thirdparty/chugins)
add_subdirectory(chuck_tilde)

