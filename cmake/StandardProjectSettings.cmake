# Settings every build of the project gets, after cpp-best-practices/cmake_template (cmake/StandardProjectSettings.cmake).
# Adapted: Release is the default build type (the template's is RelWithDebInfo: -O2, and less inlining with MSVC); no
# colours are forced into the compilers' messages (the logs of the validations are read as plain text); no
# vcvarsall.bat run from CMake (MSVC is built from its own x64 environment, which the scripts load).

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(STATUS "No build type given: Release")
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release)
endif()

# compile_commands.json in the build folder, for clang-tidy, cppcheck and the editors
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
