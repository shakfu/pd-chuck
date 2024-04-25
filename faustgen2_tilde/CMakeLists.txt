# set(CMAKE_SUPPRESS_REGENERATION true)
set(CMAKE_MACOSX_RPATH Off)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.9)

set(THIRDPARTY ${CMAKE_SOURCE_DIR}/build/thirdparty)
set(FAUST_DIR "${THIRDPARTY}/faust")
set(LIBFAUST_DIR "${THIRDPARTY}/libfaust")
set(FAUST_LIBRARIES_DIR ${LIBFAUST_DIR}/share/faust)

set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(PROJECT_NAME fausten2_tilde)

message(STATUS "FAUST_DIR: ${FAUST_DIR}")

# include_directories(
#     ${FAUST_DIR}/architecture
#     ${FAUST_DIR}/compiler
#     ${FAUST_DIR}/compiler/utils
# )

## On Linux "force" the link with stdc++
if(UNIX AND NOT APPLE)
    set(FAUST_LIBS  "stdc++"  CACHE STRING  "FAUST LIBRARIES" FORCE)
endif()


set(SOURCES 
    ${SOURCE_DIR}/faustgen_tilde.c
    ${SOURCE_DIR}/faust_tilde_ui.h
    ${SOURCE_DIR}/faust_tilde_ui.c
    ${SOURCE_DIR}/faust_tilde_io.h
    ${SOURCE_DIR}/faust_tilde_io.c
    ${SOURCE_DIR}/faust_tilde_options.h
    ${SOURCE_DIR}/faust_tilde_options.c
)

add_library( 
    ${PROJECT_NAME} 
    MODULE
    ${SOURCES}
)

target_compile_definitions(${PROJECT_NAME}
    PUBLIC
    DSPC
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
    ${SOURCE_DIR}
    ${SOURCE_DIR}/pd
    ${FAUST_DIR}/architecture
    ${FAUST_DIR}/compiler
    ${FAUST_DIR}/compiler/utils
    # ${THIRDPARTY}/pure-data/src
    ${THIRDPARTY}/install/include
)

set_target_properties(${PROJECT_NAME}
    PROPERTIES 
    PREFIX ""
    OUTPUT_NAME "faustgen2~"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/external
)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".pd_darwin")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".pd_windows")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".pd_linux")
endif()

target_compile_definitions(${PROJECT_NAME}
    PUBLIC
    $<$<PLATFORM_ID:Linux>:UNIX>
)

target_compile_options(${PROJECT_NAME}
    PUBLIC
    $<$<PLATFORM_ID:Linux>:-fPIC>
    $<$<PLATFORM_ID:Linux>:-fcheck-new>    
)

target_link_options(${PROJECT_NAME}
    PUBLIC
    $<$<PLATFORM_ID:Darwin>:-undefined dynamic_lookup>
    $<$<PLATFORM_ID:Linux>:-rdynamic>
    $<$<PLATFORM_ID:Linux>:-shared>
    $<$<PLATFORM_ID:Linux>:-fPIC>
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
    ${LIBFAUST_DIR}/lib/libfaustwithllvm.a
    ${THIRDPARTY}/install/lib/libsndfile.a
    "$<$<PLATFORM_ID:Darwin>:-framework CoreAudio>"
    "$<$<PLATFORM_ID:Darwin>:-framework CoreMIDI>"
    "$<$<PLATFORM_ID:Darwin>:-framework CoreFoundation>"
    # "$<$<PLATFORM_ID:Darwin>:-framework IOKit>"
    # "$<$<PLATFORM_ID:Darwin>:-framework Carbon>"
    # "$<$<PLATFORM_ID:Darwin>:-framework AppKit>"
    # "$<$<PLATFORM_ID:Darwin>:-framework Foundation>"
    # "$<$<PLATFORM_ID:Darwin>:-F/System/Library/PrivateFrameworks>"
    # "$<$<PLATFORM_ID:Darwin>:-weak_framework MultitouchSupport>"
    $<$<PLATFORM_ID:Darwin>:-lc++>
    $<$<PLATFORM_ID:Linux>:-lstdc++>
    $<$<PLATFORM_ID:Linux>:-lc>
    $<$<PLATFORM_ID:Linux,Darwin>:-lm>
)

