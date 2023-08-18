cmake_minimum_required(VERSION 3.19)

project(pd_chuck)

option(LINUX_ALSA "enable linux alsa build")
option(LINUX_PULSE "enable linux pulseaudio build")
option(LINUX_JACK "enable linux jack build")
option(LINUX_ALL "enable all linux build")


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
endif()


# use ccache if available
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Found ccache in ${CCACHE_PROGRAM}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()



set(CMAKE_CXX_STANDARD 14)

add_subdirectory(thirdparty/chuck/core)

add_subdirectory(thirdparty/chuck/host)

add_subdirectory(thirdparty/chugins)

add_subdirectory(chuck~)
