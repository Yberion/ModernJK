# The linker, after cpp-best-practices/cmake_template (cmake/Linker.cmake): the toolchain's own by default, or the one
# named in modernjk_LINKER (CMake's CMAKE_LINKER_TYPE: LLD, MOLD, GOLD, BFD, MSVC, ...), for every target.
set(modernjk_LINKER "" CACHE STRING "Linker of every target (CMAKE_LINKER_TYPE); empty: the toolchain's own")
if(modernjk_LINKER)
	set(CMAKE_LINKER_TYPE "${modernjk_LINKER}")
endif()
