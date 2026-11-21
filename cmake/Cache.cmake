# A compiler cache in front of the compilers, after cpp-best-practices/cmake_template (cmake/Cache.cmake). Off by
# default (modernjk_ENABLE_CACHE): no machine of the project has one installed.
function(modernjk_enable_cache)
	set(modernjk_CACHE_PROGRAM ccache CACHE STRING "Compiler cache: ccache or sccache")
	set_property(CACHE modernjk_CACHE_PROGRAM PROPERTY STRINGS ccache sccache)
	find_program(modernjk_CACHE_BINARY NAMES ${modernjk_CACHE_PROGRAM})
	if(modernjk_CACHE_BINARY)
		message(STATUS "Compiler cache: ${modernjk_CACHE_BINARY}")
		set(CMAKE_CXX_COMPILER_LAUNCHER "${modernjk_CACHE_BINARY}" CACHE FILEPATH "C++ compiler cache")
		set(CMAKE_C_COMPILER_LAUNCHER "${modernjk_CACHE_BINARY}" CACHE FILEPATH "C compiler cache")
	else()
		message(WARNING "${modernjk_CACHE_PROGRAM} requested but not found: no compiler cache")
	endif()
endfunction()
