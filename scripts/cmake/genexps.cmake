set(is_darwin "$<PLATFORM_ID:Darwin>")
set(is_linux "$<PLATFORM_ID:Linux>")
set(is_windows "$<PLATFORM_ID:Windows>")
set(is_unix "$<OR:$<PLATFORM_ID:Darwin>,$<PLATFORM_ID:Linux>>")

set(is_static "$<BOOL:${STATIC}>")
set(is_shared "$<NOT:$<BOOL:${STATIC}>>")

set(is_unix_static "$<AND:${is_unix},${is_static}>")
set(is_unix_shared "$<AND:${is_unix},${is_shared}>")

set(is_windows_static "$<AND:${is_windows},${is_static}>")
set(is_windows_shared "$<AND:${is_windows},${is_shared}>")


function(debug_genexps target)
    add_custom_command(TARGET ${target} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "is_darwin = ${is_darwin}"
        COMMAND ${CMAKE_COMMAND} -E echo "is_linux = ${is_linux}"
        COMMAND ${CMAKE_COMMAND} -E echo "is_windows = ${is_windows}"
        COMMAND ${CMAKE_COMMAND} -E echo "is_unix = ${is_unix}"
        COMMAND ${CMAKE_COMMAND} -E echo "is_static = ${is_static}"
        COMMAND ${CMAKE_COMMAND} -E echo "is_shared = ${is_shared}"
        COMMAND ${CMAKE_COMMAND} -E echo "is_unix_static = ${is_unix_static}"
        COMMAND ${CMAKE_COMMAND} -E echo "is_windows_static = ${is_windows_static}"
        COMMAND ${CMAKE_COMMAND} -E echo "is_unix_shared = ${is_unix_shared}"
        COMMAND ${CMAKE_COMMAND} -E echo "is_windows_shared = ${is_windows_shared}"
    )
endfunction() # debug_genexps
