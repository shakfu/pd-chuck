# set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version" FORCE)
set(BUILD_SHARED_LIBS ON)

set(path "${CMAKE_CURRENT_SOURCE_DIR}")
cmake_path(GET path STEM PARENT_DIR)

# chugin name
# set(CHUGIN_NAME ${PARENT_DIR})
set(CHUGIN_NAME "Faust")

if (DEFINED ENV{FAUCK_VERSION})
set (VERSION $ENV{FAUCK_VERSION})
else()
set (VERSION "0.3.0")
endif()

set(THIRDPARTY ${CMAKE_SOURCE_DIR}/build/thirdparty)

# where the chuck headers are
set(CK_SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../chuck/include)
set(CK_SOURCE ${CK_SRC_PATH}/chugin.h)

set(FAUST_DIR "${CMAKE_BINARY_DIR}/thirdparty/faust")
set(LIBFAUST_DIR "${CMAKE_BINARY_DIR}/thirdparty/libfaust")
set(FAUST_LIBRARIES_DIR ${LIBFAUST_DIR}/share/faust)


set(PROJECT_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/Faust.cpp
    ${CK_SOURCE}
)


add_library(${CHUGIN_NAME}
    ${PROJECT_SRC}
)


set_property(TARGET ${CHUGIN_NAME} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${CHUGIN_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)

# add_dependencies(${CHUGIN_NAME} fauck_deps)


if(BUILD_SHARED_LIBS)
    set_target_properties(${CHUGIN_NAME}
        PROPERTIES
        PREFIX ""
        SUFFIX ".chug"
    )
endif()

target_compile_options(${CHUGIN_NAME}
    PRIVATE
    -fPIC
    $<IF:$<CONFIG:Debug>,-g,-O3>
)

target_compile_definitions(${CHUGIN_NAME}
    PRIVATE
    FAUCK_VERSION=\"v${VERSION}\"
    # FAUCK_EXPORTS
    $<$<PLATFORM_ID:Darwin>:__MACOSX_CORE__>
    $<$<PLATFORM_ID:Darwin>:__PLATFORM_APPLE__>
    $<$<PLATFORM_ID:Darwin>:NOBUNDLE>
    $<$<PLATFORM_ID:Linux>:__PLATFORM_LINUX__>
    $<$<PLATFORM_ID:Linux>:_GLIBCXX_USE_CXX11_ABI=0>
    $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:__CK_DLL_STATIC__>
)


target_include_directories(${CHUGIN_NAME}
    PRIVATE
    ${LOCAL_INCLUDE}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${THIRDPARTY}/install/include
    ${CK_SRC_PATH}
    ${FAUST_DIR}/architecture
    ${FAUST_DIR}/compiler
    ${FAUST_DIR}/compiler/utils  
)

target_link_options(${CHUGIN_NAME}
    PRIVATE
    $<$<CONFIG:RELEASE>:-s>
    -shared
    $<$<PLATFORM_ID:Linux>:-lstdc++>
)


target_link_directories(${CHUGIN_NAME}
    PRIVATE
    ${LOCAL_LIB}
    ${LIBFAUST_DIR}/lib
    ${THIRDPARTY}/install/lib
)

if (DYNAMIC_LINKING)
target_link_libraries(${CHUGIN_NAME}
    PRIVATE

    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:-lFLAC>
    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:-logg>
    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:-lvorbis>

    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:-lvorbisenc>
    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:-lvorbisfile>
    -lsndfile
    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:-lopus>
    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:-lmpg123>

    $<$<BOOL:${ENABLE_MP3}>:-lmp3lame>

    ${LIBFAUST_DIR}/lib/libfaustwithllvm.a
    # ${LIBFAUST_DIR}/lib/libfaust.a
    "$<$<PLATFORM_ID:Darwin>:-framework CoreFoundation>"
    "$<$<PLATFORM_ID:Darwin>:-framework CoreMIDI>"
    "$<$<PLATFORM_ID:Darwin>:-framework CoreAudio>"
    "$<$<PLATFORM_ID:Darwin>:-framework Accelerate>"
)
else()
target_link_libraries(${CHUGIN_NAME}
    PRIVATE
    ${LIBFAUST_DIR}/lib/libfaustwithllvm.a
    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:${LOCAL_LIB}/libFLAC.a>
    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:${LOCAL_LIB}/libogg.a>
    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:${LOCAL_LIB}/libvorbis.a>
    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:${LOCAL_LIB}/libvorbisenc.a>
    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:${LOCAL_LIB}/libvorbisfile.a>
    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:${LOCAL_LIB}/libopus.a>
    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:${LOCAL_LIB}/libmpg123.a>
    $<$<BOOL:${ENABLE_MP3}>:${LOCAL_LIB}/libmp3lame.a>
    ${LOCAL_LIB}/libsndfile.a
    "$<$<PLATFORM_ID:Darwin>:-framework CoreFoundation>"
    "$<$<PLATFORM_ID:Darwin>:-framework CoreMIDI>"
    "$<$<PLATFORM_ID:Darwin>:-framework CoreAudio>"
    "$<$<PLATFORM_ID:Darwin>:-framework Accelerate>"
)
endif()

install(
    TARGETS ${CHUGIN_NAME}
    LIBRARY DESTINATION ${CMAKE_SOURCE_DIR}/examples/chugins
)

set (DST ${CMAKE_SOURCE_DIR}/examples/chugins)