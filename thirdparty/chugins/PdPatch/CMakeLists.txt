include(FetchContent)

FetchContent_Declare(
    libpd
    GIT_REPOSITORY https://github.com/libpd/libpd.git
    GIT_TAG        master
    GIT_SUBMODULES "pure-data"
)

# Configure libpd options before making available
set(PD_MULTI ON CACHE BOOL "Enable multiple Pd instances" FORCE)
set(PD_UTILS OFF CACHE BOOL "No ringbuffer/queued utils needed" FORCE)
set(PD_EXTRA ON CACHE BOOL "Include Pd extras" FORCE)
set(LIBPD_STATIC ON CACHE BOOL "Build static libpd" FORCE)
set(LIBPD_SHARED OFF CACHE BOOL "No shared libpd needed" FORCE)

FetchContent_MakeAvailable(libpd)

# libpd_static doesn't link threading/math/dl, so we must do it ourselves
find_package(Threads REQUIRED)

add_chugin(
    SOURCES
        PdPatch.cpp
        PdPatchInternal.cpp
    LINK_LIBS
        libpd_static
        Threads::Threads
        ${CMAKE_DL_LIBS}
    COMPILE_DEFINITIONS
        PD_MULTI
)

# Link math library on Unix (not needed on macOS/Windows)
if(UNIX AND NOT APPLE)
    target_link_libraries(PdPatch PRIVATE m)
endif()
