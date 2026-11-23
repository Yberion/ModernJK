# modernjk.pk3, one per game: the files of assets/jka (the multiplayer and Jedi Academy's single player) and of assets/jk2
# (Jedi Outcast), each at its path in the archive, zipped by CMake itself. Installed in the game's base/, whose pk3 files
# the engine reads in alphabetical order, the last first (the ModernJK folder holds only the modules). The two games'
# menus have the same paths with other contents, so one archive cannot serve both.
function(modernjk_add_pk3 target folder pk3 destination)
	file(GLOB_RECURSE files RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/${folder}" CONFIGURE_DEPENDS "${folder}/*")
	list(TRANSFORM files PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/${folder}/" OUTPUT_VARIABLE sources)
	get_filename_component(pk3_folder "${pk3}" DIRECTORY)
	add_custom_command(OUTPUT "${pk3}"
		COMMAND "${CMAKE_COMMAND}" -E make_directory "${pk3_folder}"
		# a fixed date in the archive (the zip format's first day): the same files give the same pk3
		COMMAND "${CMAKE_COMMAND}" -E tar cf "${pk3}" --format=zip "--mtime=1980-01-01 00:00:00 UTC" -- ${files}
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${folder}"
		DEPENDS ${sources}
		COMMENT "Zipping ${pk3}"
		VERBATIM)
	add_custom_target(${target} ALL DEPENDS "${pk3}")
	install(FILES "${pk3}" DESTINATION "${destination}")
endfunction()

modernjk_add_pk3(modernjk_pk3 jka "${PROJECT_BINARY_DIR}/bin/modernjk.pk3" "${modernjk_INSTALL_JKA}/base")
if(modernjk_BUILD_JK2)
	modernjk_add_pk3(modernjk_pk3_jk2 jk2 "${PROJECT_BINARY_DIR}/bin/jk2/modernjk.pk3" "${modernjk_INSTALL_JK2}/base")
endif()
