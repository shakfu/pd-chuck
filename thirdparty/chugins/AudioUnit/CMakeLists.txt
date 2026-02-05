# AudioUnit Chugin CMakeLists.txt
# macOS only

if(NOT APPLE)
    message(STATUS "AudioUnit chugin is macOS only. Skipping.")
    return()
endif()

add_chugin(
    LINK_LIBS
        "-framework CoreFoundation"
        "-framework CoreMIDI"
        "-framework AudioToolbox"
)
