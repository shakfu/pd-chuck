
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CHUCK_DIR ${CMAKE_SOURCE_DIR}/thirdparty/chuck)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
	execute_process(
		COMMAND xcrun --show-sdk-path
		OUTPUT_VARIABLE MACOS_SDK_PATH
	)
endif()

set(CXX_HOST
	chuck_main.cpp
	chuck_console.cpp
	chuck_audio.cpp
	RtAudio/RtAudio.cpp
)

add_executable(chuck
	${CXX_HOST}
)

target_compile_options(chuck
	PUBLIC	
	$<$<PLATFORM_ID:Darwin>:-isysroot ${MACOS_SDK_PATH}>
	# $<$<PLATFORM_ID:Linux>:-fno-strict-aliasing>
	# -O3
)

target_compile_definitions(chuck
	PUBLIC
	$<$<PLATFORM_ID:Darwin>:__MACOSX_CORE__>
	$<$<PLATFORM_ID:Linux>:__LINUX_ALSA__>	
	$<$<PLATFORM_ID:Linux>:__CK_SNDFILE_NATIVE__>	
	HAVE_CONFIG_H
)



target_include_directories(chuck
	PUBLIC
	${CHUCK_DIR}/core
	${CHUCK_DIR}/host
	${CHUCK_DIR}/host/RtAudio
	${CHUCK_DIR}/host/RtAudio/include
)

target_link_libraries(chuck
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
	"$<$<PLATFORM_ID:Darwin>:-lc++>"
	"$<$<PLATFORM_ID:Linux>:-lstdc++>"
	-lm
	chuck_lib
)
