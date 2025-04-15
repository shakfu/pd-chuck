# set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version" FORCE)

set(path "${CMAKE_CURRENT_SOURCE_DIR}")
cmake_path(GET path STEM PARENT_DIR)

# chugin name
set(CHUGIN_NAME ${PARENT_DIR})

set(THIRDPARTY ${CMAKE_SOURCE_DIR}/build/thirdparty)

# all of the c/cpp files that compose this chugin
file(GLOB PROJECT_SRC
    src/AbletonClipInfo.h
    src/WarpBufChugin.h
    src/WarpBufChugin.cpp
    src/WarpBufChuginDLL.cpp
)

# where the chuck headers are
set(CK_SRC_PATH
    ${CMAKE_SOURCE_DIR}/thirdparty/chugins/chuck/include
)

add_library(${CHUGIN_NAME}
    ${PROJECT_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/portable_endian/include/portable_endian.h
)

# add_dependencies(${CHUGIN_NAME} warpbuf_deps)

set_target_properties(${CHUGIN_NAME} PROPERTIES CXX_STANDARD 17)
set_target_properties(${CHUGIN_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(BUILD_SHARED_LIBS)
    set_target_properties(${CHUGIN_NAME}
        PROPERTIES
        PREFIX ""
        SUFFIX ".chug"
    )
endif()

target_compile_options(${CHUGIN_NAME}
    PUBLIC
    -fPIC
    $<IF:$<CONFIG:Debug>,-g,-O3>
)

target_compile_definitions(${CHUGIN_NAME}
    PUBLIC
    $<$<PLATFORM_ID:Darwin>:__MACOSX_CORE__>
    $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:__CK_DLL_STATIC__>
    HAVE_CONFIG_H
    HAVE_LIBSAMPLERATE
    HAVE_VDSP
    USE_PTHREADS
)

target_include_directories(${CHUGIN_NAME}
    PUBLIC
    ${LOCAL_INCLUDE}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${THIRDPARTY}/install/include
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/portable_endian/include
    ${CK_SRC_PATH}
)

target_link_options(${CHUGIN_NAME}
    PUBLIC
    -shared
    $<$<PLATFORM_ID:Linux>:-lstdc++>
)

target_link_directories(${CHUGIN_NAME}
    PUBLIC
    ${LOCAL_LIB}
    ${THIRDPARTY}/install/lib
)


if (DYNAMIC_LINKING)
target_link_libraries(${CHUGIN_NAME}
    PUBLIC
    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:-lFLAC>
    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:-logg>
    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:-lvorbis>
    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:-lvorbisenc>
    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:-lvorbisfile>
    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:-lopus>
    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:-lmpg123>
    $<$<BOOL:${ENABLE_MP3}>:-lmp3lame>
    -lsndfile
    -lrubberband
    -lsamplerate
    "$<$<PLATFORM_ID:Darwin>:-framework CoreFoundation>"
    "$<$<PLATFORM_ID:Darwin>:-framework CoreMIDI>"
    "$<$<PLATFORM_ID:Darwin>:-framework CoreAudio>"
    "$<$<PLATFORM_ID:Darwin>:-framework Accelerate>"
)
else()
target_link_libraries(${CHUGIN_NAME}
    PUBLIC
    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:${LOCAL_LIB}/libFLAC.a>
    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:${LOCAL_LIB}/libogg.a>
    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:${LOCAL_LIB}/libvorbis.a>
    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:${LOCAL_LIB}/libvorbisenc.a>
    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:${LOCAL_LIB}/libvorbisfile.a>
    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:${LOCAL_LIB}/libopus.a>
    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:${LOCAL_LIB}/libmpg123.a>
    $<$<BOOL:${ENABLE_MP3}>:${LOCAL_LIB}/libmp3lame.a>
    ${LOCAL_LIB}/libsndfile.a
    ${LOCAL_LIB}/librubberband.a
    ${LOCAL_LIB}/libsamplerate.a
    "$<$<PLATFORM_ID:Darwin>:-framework CoreFoundation>"
    "$<$<PLATFORM_ID:Darwin>:-framework CoreMIDI>"
    "$<$<PLATFORM_ID:Darwin>:-framework CoreAudio>"
    "$<$<PLATFORM_ID:Darwin>:-framework Accelerate>"
)
endif()


# set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/examples")
install(
    TARGETS ${CHUGIN_NAME}
    LIBRARY DESTINATION chugins
    # LIBRARY DESTINATION ${CMAKE_SOURCE_DIR}/examples/chugins
)

# set (DST ${CMAKE_SOURCE_DIR}/externals/chuck~.mxo/Contents/Resources/chugins)
# set (DST ${CMAKE_SOURCE_DIR}/examples/chugins)

# install(
#     CODE "execute_process (COMMAND codesign -vf -s - ${DST}/WarpBuf.chug)" 
# )


