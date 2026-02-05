# CLAP Chugin CMakeLists.txt

add_chugin(
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/clap-headers/include
    LINK_LIBS
        "$<$<PLATFORM_ID:Darwin>:-framework CoreFoundation>"
        "$<$<PLATFORM_ID:Darwin>:-framework CoreMIDI>"
        "$<$<PLATFORM_ID:Darwin>:-framework AudioToolbox>"
)
