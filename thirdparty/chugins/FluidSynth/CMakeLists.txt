set(CMAKE_CXX_STANDARD 14)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version" FORCE)

set(path "${CMAKE_CURRENT_SOURCE_DIR}")
cmake_path(GET path STEM PARENT_DIR)

# chugin name
set(CHUGIN_NAME ${PARENT_DIR})

set(THIRDPARTY ${CMAKE_SOURCE_DIR}/build/thirdparty)

# all of the c/cpp files that compose this chugin
file(GLOB PROJECT_SRC
	FluidSynth.cpp
)

# where the chuck headers are
set(CK_SRC_PATH
    ${CMAKE_SOURCE_DIR}/thirdparty/chugins/chuck/include
)

add_library(${CHUGIN_NAME}
    ${PROJECT_SRC}
)

# set_target_properties(${CHUGIN_NAME} PROPERTIES CXX_STANDARD 17)
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
    __APPLE__
    $<$<BOOL:${APPLE}>:__MACOSX_CORE__>
    $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:__CK_DLL_STATIC__>
    HAVE_CONFIG_H
    USE_PTHREADS
)

target_include_directories(${CHUGIN_NAME}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    # ${THIRDPARTY}/install/include
    ${local_prefix}/include
    ${CK_SRC_PATH}
    # /opt/homebrew/opt/fluid-synth/include
)

target_link_options(${CHUGIN_NAME}
    PUBLIC
    -shared
)

target_link_directories(${CHUGIN_NAME}
    PUBLIC
    # ${THIRDPARTY}/install/lib
    ${local_prefix}/lib
)

target_link_libraries(${CHUGIN_NAME}
    PUBLIC
    -liconv
    -lreadline

    # ${THIRDPARTY}/install/lib/libfluidsynth.a
    ${local_prefix}/lib/libfluidsynth.a
    ${local_prefix}/lib/libportaudio.a
    ${local_prefix}/lib/libportaudiocpp.a
    ${local_prefix}/lib/libglib-2.0.a
    ${local_prefix}/lib/libgthread-2.0.a
    ${local_prefix}/lib/libintl.a
    # ${local_prefix}/lib/libgettextlib.a

    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:${local_prefix}/lib/libFLAC.a>
    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:${local_prefix}/lib/libogg.a>
    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:${local_prefix}/lib/libvorbis.a>
    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:${local_prefix}/lib/libvorbisenc.a>
    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:${local_prefix}/lib/libvorbisfile.a>
    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:${local_prefix}/lib/libopus.a>
    $<$<BOOL:${ENABLE_EXTRA_FORMATS}>:${local_prefix}/lib/libmpg123.a>
    $<$<BOOL:${ENABLE_MP3}>:${local_prefix}/lib/libmp3lame.a>
    ${local_prefix}/lib/libsndfile.a
    ${local_prefix}/lib/librubberband.a
    ${local_prefix}/lib/libsamplerate.a
    "$<$<BOOL:${APPLE}>:-framework CoreFoundation>"
    "$<$<BOOL:${APPLE}>:-framework CoreMIDI>"
    "$<$<BOOL:${APPLE}>:-framework CoreServices>"
    "$<$<BOOL:${APPLE}>:-framework CoreAudio>"
    "$<$<BOOL:${APPLE}>:-framework AudioUnit>"
    "$<$<BOOL:${APPLE}>:-framework AudioToolbox>"
    "$<$<BOOL:${APPLE}>:-framework AppKit>"
    "$<$<BOOL:${APPLE}>:-framework Accelerate>"
)


install(
    TARGETS ${CHUGIN_NAME}
    LIBRARY DESTINATION chugins
)

# set (DST ${CMAKE_SOURCE_DIR}/examples/chugins)

# if(APPLE)
# install(
#     CODE "execute_process (COMMAND codesign -vf -s - ${DST}/FluidSynth.chug)" 
# )
# endif()


