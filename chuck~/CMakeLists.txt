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


target_include_directories(${PROJECT_NAME}
    PUBLIC
    ${CHUCK_INCLUDE}
)

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".pd_darwin")
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "chuck~")
set_target_properties(${PROJECT_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})



target_link_libraries(${PROJECT_NAME}
    PUBLIC
    "-framework CoreAudio"
    "-framework CoreMIDI"
    "-framework CoreFoundation"
    "-framework IOKit"
    "-framework Carbon"
    "-framework AppKit"
    "-framework Foundation"
    "-F/System/Library/PrivateFrameworks"
    "-weak_framework MultitouchSupport"
    -lc++
    -lm
    chuck_lib
)

