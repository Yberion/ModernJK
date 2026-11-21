# Link-time optimization of every target configured after the call, after cpp-best-practices/cmake_template
# (cmake/InterproceduralOptimization.cmake); refused where the toolchain cannot do it.
macro(modernjk_enable_ipo)
	include(CheckIPOSupported)
	check_ipo_supported(RESULT modernjk_ipo_supported OUTPUT modernjk_ipo_output)
	if(modernjk_ipo_supported)
		set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
	else()
		message(SEND_ERROR "Link-time optimization is not supported: ${modernjk_ipo_output}")
	endif()
endmacro()
