# AbletonLink Chugin CMakeLists.txt

# Ableton Link SDK configuration
set(LINK_SDK_ROOT ${CMAKE_BINARY_DIR}/thirdparty/link)
set(LINK_BRANCH "Link-3.1.5")

# Auto-fetch Ableton Link SDK if not present
if(NOT EXISTS ${LINK_SDK_ROOT})
    message(STATUS "Fetching Ableton Link SDK (${LINK_BRANCH})...")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/thirdparty
    )
    execute_process(
        COMMAND git clone -b ${LINK_BRANCH} --depth=1 --recursive
                https://github.com/Ableton/link ${LINK_SDK_ROOT}
        RESULT_VARIABLE LINK_CLONE_RESULT
    )
    if(NOT LINK_CLONE_RESULT EQUAL 0)
        message(WARNING "Failed to clone Ableton Link SDK. Skipping AbletonLink chugin.")
        return()
    endif()
endif()

set(LINK_ROOT ${LINK_SDK_ROOT})

# Verify Link headers exist
if(NOT EXISTS ${LINK_ROOT}/include/ableton/Link.hpp)
    message(WARNING "Ableton Link headers not found at ${LINK_ROOT}/include. Skipping AbletonLink chugin.")
    return()
endif()

# Platform-specific configuration
if(APPLE)
    set(LINK_LIBRARIES "-framework CoreFoundation" "-framework CoreServices")
    set(PLATFORM_DEFS "LINK_PLATFORM_MACOSX=1")
elseif(WIN32)
    set(LINK_LIBRARIES "ws2_32" "winmm" "iphlpapi")
    set(PLATFORM_DEFS "LINK_PLATFORM_WINDOWS=1" "_WIN32_WINNT=0x0601")
else()
    set(LINK_LIBRARIES "pthread")
    set(PLATFORM_DEFS "LINK_PLATFORM_LINUX=1")
endif()

# Add the chugin target
add_chugin(
    SOURCES
        AbletonLink.cpp
        abl_link_instance.cpp
    INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/thirdparty/chuck/core
        ${LINK_ROOT}/include
        ${LINK_ROOT}/modules/asio-standalone/asio/include
    COMPILE_DEFINITIONS
        ASIO_STANDALONE
        ASIO_HEADER_ONLY
        ${PLATFORM_DEFS}
    LINK_LIBS
        ${LINK_LIBRARIES}
    COMPILE_OPTIONS
        -w
)
