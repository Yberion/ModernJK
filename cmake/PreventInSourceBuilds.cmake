# Refuses a build in the source tree: the build files would mix with the sources, and git would see them.
# After cpp-best-practices/cmake_template (cmake/PreventInSourceBuilds.cmake).
function(modernjk_assure_out_of_source_builds)
	# the real paths, so that a symbolic link cannot hide the source tree
	file(REAL_PATH "${CMAKE_SOURCE_DIR}" srcdir)
	file(REAL_PATH "${CMAKE_BINARY_DIR}" bindir)
	if(srcdir STREQUAL bindir)
		message(FATAL_ERROR "In-source builds are disabled: use a preset (cmake --preset <name>) or a build folder")
	endif()
endfunction()

modernjk_assure_out_of_source_builds()
