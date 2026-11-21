# clang-tidy and cppcheck run on every C++ file while it compiles, after cpp-best-practices/cmake_template
# (cmake/StaticAnalyzers.cmake). They report only: their findings do not stop the build, except clang-tidy's on
# netcodec (modernjk_clang_tidy_as_errors), which is clean under its own checks (src/codemp/netcodec/.clang-tidy).
# The checks of the other files are in the repository's .clang-tidy.

function(modernjk_enable_clang_tidy)
	find_program(modernjk_CLANG_TIDY clang-tidy)
	if(NOT modernjk_CLANG_TIDY)
		message(WARNING "clang-tidy requested but not found")
		return()
	endif()
	# the compiler's flags that clang-tidy's own clang does not know are not errors; SDL2's headers replace its
	# functions by the C library's under the static analyzer, without including the C library's headers
	set(CMAKE_CXX_CLANG_TIDY "${modernjk_CLANG_TIDY}" -extra-arg=-Wno-unknown-warning-option
		-extra-arg=-Wno-ignored-optimization-argument -extra-arg=-Wno-unused-command-line-argument
		-extra-arg=-DSDL_DISABLE_ANALYZE_MACROS PARENT_SCOPE)
endfunction()

# clang-tidy's findings as errors on one target (netcodec's)
function(modernjk_clang_tidy_as_errors target)
	if(CMAKE_CXX_CLANG_TIDY)
		set_target_properties(${target} PROPERTIES CXX_CLANG_TIDY "${CMAKE_CXX_CLANG_TIDY};-warnings-as-errors=*")
	endif()
endfunction()

function(modernjk_enable_cppcheck)
	find_program(modernjk_CPPCHECK cppcheck)
	if(NOT modernjk_CPPCHECK)
		message(WARNING "cppcheck requested but not found")
		return()
	endif()
	# its errors and warnings (style, performance and portability report too much of this old code to be read),
	# without dangerousTypeCast (every C-style cast of a pointer); not the vendored libraries
	# each finding with its severity (the gcc template calls them all warnings)
	set(command "${modernjk_CPPCHECK}" "--template={file}:{line}:{column}: {severity}: {message} [{id}]"
		--enable=warning --inline-suppr --quiet
		--suppress=dangerousTypeCast --suppress=missingIncludeSystem --suppress=missingInclude
		"-i${PROJECT_SOURCE_DIR}/libs")
	# cppcheck does not know the compiler's own macros: without the platform's, q_platform.h stops at its #error
	if(WIN32)
		list(APPEND command --platform=win64 -D_WIN32=1 -D_WIN64=1)
	else()
		list(APPEND command --platform=unix64 -D__linux__=1)
	endif()
	if(MINGW)
		list(APPEND command -D__MINGW32__=1 -D__MINGW64__=1)
	endif()
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|x86_64)$")
		list(APPEND command -D__x86_64__=1)
	endif()
	if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		list(APPEND command -D__clang__=1 -D__GNUC__=4)
	elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		list(APPEND command -D__GNUC__=15)
	endif()
	set(CMAKE_CXX_CPPCHECK ${command} PARENT_SCOPE)
endfunction()
