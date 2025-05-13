function(add_chugin)
    set(options
        DEBUG
    )
    set(oneValueArgs 
        PROJECT_NAME
    )
    set(multiValueArgs
        PROJECT_SOURCE
        INCLUDE_DIRS
        LINK_LIBS
        LINK_DIRS
        COMPILE_DEFINITIONS
        COMPILE_OPTIONS
        LINK_OPTIONS
    )

    cmake_parse_arguments(
        CHUGIN
        "${options}"
        "${oneValueArgs}"
        "${multiValueArgs}"
        ${ARGN}
    )

    set(path "${CMAKE_CURRENT_SOURCE_DIR}")
    cmake_path(GET path STEM PARENT_DIR)
    set(CHUGIN_NAME ${PARENT_DIR})
    set(CK_SRC_PATH "${CMAKE_SOURCE_DIR}/thirdparty/chugins/chuck/include")
    set(DEPS_DIR "${CMAKE_SOURCE_DIR}/build/install")

    if(CHUGIN_PROJECT_SOURCE)
        set(PROJECT_SRC ${CHUGIN_PROJECT_SOURCE})
    else()
        file(GLOB PROJECT_SRC
            "*.h"
            "*.c"
            "*.cpp"
        )
    endif()

    message(STATUS "CHUGIN_NAME: ${CHUGIN_NAME}")
    if(DEBUG)
        message(STATUS "PROJECT_SOURCE: ${PROJECT_SRC}")
        message(STATUS "CHUGIN_INCLUDE_DIRS: ${CHUGIN_INCLUDE_DIRS}")
        message(STATUS "CHUGIN_COMPILE_DEFINITIONS: ${CHUGIN_COMPILE_DEFINITIONS}")
        message(STATUS "CHUGIN_COMPILE_OPTIONS: ${CHUGIN_COMPILE_OPTIONS}")
        message(STATUS "CHUGIN_LINK_LIBS: ${CHUGIN_LINK_LIBS}")
        message(STATUS "CHUGIN_LINK_DIRS: ${CHUGIN_LINK_DIRS}")
        message(STATUS "CHUGIN_LINK_OPTIONS: ${CHUGIN_LINK_OPTIONS}")
    endif()

    add_library(${CHUGIN_NAME}
        ${PROJECT_SRC}
    )

    if(BUILD_SHARED_LIBS)
        set_target_properties(${CHUGIN_NAME}
            PROPERTIES
            PREFIX ""
            SUFFIX ".chug"
        )
    endif()


    target_compile_options(
        ${CHUGIN_NAME}
        PRIVATE
        -fPIC
        $<IF:$<CONFIG:Debug>,-g,-O3>
        ${CHUGIN_COMPILE_OPTIONS}

    )

    target_compile_definitions(
        ${CHUGIN_NAME}
        PRIVATE
        HAVE_CONFIG_H
        $<$<PLATFORM_ID:Darwin>:__MACOSX_CORE__>
        $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:__CK_DLL_STATIC__>
        ${CHUGIN_COMPILE_DEFINITIONS}
    )

    target_include_directories(
        ${CHUGIN_NAME}
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CK_SRC_PATH}
        ${CHUGIN_INCLUDE_DIRS}
    )

    target_link_options(
        ${CHUGIN_NAME}
        PRIVATE
        -shared
        $<$<PLATFORM_ID:Linux>:-lstdc++>
        ${CHUGIN_LINK_OPTIONS}
    )

    target_link_directories(
        ${CHUGIN_NAME} 
        PRIVATE
        ${CHUGIN_LINK_DIRS}
    )

    target_link_libraries(
        ${CHUGIN_NAME} 
        PRIVATE
        "${CHUGIN_LINK_LIBS}"
    )
endfunction()
