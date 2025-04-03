set(path "${CMAKE_CURRENT_SOURCE_DIR}")
cmake_path(GET path STEM PARENT_DIR)

# chugin name
set(CHUGIN_NAME ${PARENT_DIR})

# all of the c/cpp files that compose this chugin
file(GLOB PROJECT_SRC
    "*.h"
    "*.c"
    "*.cpp"
)

# where the chuck headers are
set(CK_SRC_PATH
	${CMAKE_SOURCE_DIR}/thirdparty/chugins/chuck/include
)

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

target_compile_options(${CHUGIN_NAME}
	PUBLIC
	-fPIC
	$<IF:$<CONFIG:Debug>,-g,-O3>

)

target_compile_definitions(${CHUGIN_NAME}
	PUBLIC
	HAVE_CONFIG_H
	$<$<PLATFORM_ID:Darwin>:__MACOSX_CORE__>
	$<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:__CK_DLL_STATIC__>
)

target_include_directories(${CHUGIN_NAME}
	PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
	${CK_SRC_PATH}
)

target_link_options(${CHUGIN_NAME}
	PUBLIC
	-shared
	# $<$<PLATFORM_ID:Darwin>:-lc++>
	$<$<PLATFORM_ID:Linux>:-lstdc++>
)

# target_link_libraries(${CHUGIN_NAME}
# 	PUBLIC
# 	-lm
# )
