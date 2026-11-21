# modernjk.pk3: the files of this folder, each at its path in the archive, zipped by CMake itself. Installed in base/,
# whose pk3 files the engine reads in alphabetical order, the last first (the ModernJK folder holds only the modules).
set(files
	modernjk.txt)
set(pk3 "${PROJECT_BINARY_DIR}/bin/modernjk.pk3")
add_custom_command(OUTPUT "${pk3}"
	# a fixed date in the archive (the zip format's first day): the same files give the same pk3
	COMMAND "${CMAKE_COMMAND}" -E tar cf "${pk3}" --format=zip "--mtime=1980-01-01 00:00:00 UTC" -- ${files}
	WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
	DEPENDS ${files}
	COMMENT "Zipping modernjk.pk3"
	VERBATIM)
add_custom_target(modernjk_pk3 ALL DEPENDS "${pk3}")
install(FILES "${pk3}" DESTINATION "${modernjk_INSTALL_JKA}/base")
