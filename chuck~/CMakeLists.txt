set(CMAKE_MACOSX_RPATH Off)
set(CMAKE_OSX_DEPLOYMENT_TARGET 12.6)

set(CHUCK_DIR ${CMAKE_SOURCE_DIR}/thirdparty/chuck)
set(CHUCK_INCLUDE ${CHUCK_DIR}/core)


file(GLOB PROJECT_SRC
    "*.h"
    "*.c"
    "*.cpp"
)

add_library( 
    ${PROJECT_NAME} 
    MODULE
    ${PROJECT_SRC}
)

# target_compile_options(${PROJECT_NAME}
#     PUBLIC
#     -fPIC
#  )

target_include_directories(${PROJECT_NAME}
    PUBLIC
    ${CHUCK_INCLUDE}
)

set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".pd_darwin")
elseif(MAKE_SYSTEM_NAME STREQUAL "Windows")
set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".pd_windows")
elseif(MAKE_SYSTEM_NAME STREQUAL "Linux")
set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".pd_linux")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "chuck~")
set_target_properties(${PROJECT_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})



target_link_libraries(${PROJECT_NAME}
    PUBLIC
    "$<$<PLATFORM_ID:Darwin>:-framework CoreAudio>"
    "$<$<PLATFORM_ID:Darwin>:-framework CoreMIDI>"
    "$<$<PLATFORM_ID:Darwin>:-framework CoreFoundation>"    
    "$<$<PLATFORM_ID:Darwin>:-framework IOKit>"
    "$<$<PLATFORM_ID:Darwin>:-framework Carbon>"
    "$<$<PLATFORM_ID:Darwin>:-framework AppKit>"
    "$<$<PLATFORM_ID:Darwin>:-framework Foundation>"
    "$<$<PLATFORM_ID:Darwin>:-F/System/Library/PrivateFrameworks>"
    "$<$<PLATFORM_ID:Darwin>:-weak_framework MultitouchSupport>"
    $<$<PLATFORM_ID:Darwin>:-lc++>
    $<$<PLATFORM_ID:Linux>:-lstdc++>
    -lm
    chuck_lib
)



