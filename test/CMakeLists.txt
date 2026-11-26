#============================================================================
# Copyright (C) 2013 - 2018, OpenJK contributors
# Copyright (C) 2026, ModernJK contributors
#
# This file is part of the OpenJK source code.
#
# OpenJK is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.
#============================================================================

# The tests of src/shared/qcommon/safe, without a framework (check.h)
add_executable(modernjk_test_safe main.cpp safe/string.cpp safe/limited_vector.cpp)
target_include_directories(modernjk_test_safe PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(modernjk_test_safe PRIVATE modernjk_safe_string modernjk_options modernjk_warnings
	modernjk_defines vendored::fmt)
if(MINGW)
	target_link_options(modernjk_test_safe PRIVATE -static)
endif()
add_test(NAME modernjk.safe COMMAND modernjk_test_safe)

# The tests of src/shared/qcommon/q_fmt.h and q_fmt.cpp
add_executable(modernjk_test_fmt qcommon/q_fmt.cpp "${PROJECT_SOURCE_DIR}/src/shared/qcommon/q_fmt.cpp")
target_include_directories(modernjk_test_fmt PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(modernjk_test_fmt PRIVATE modernjk_options modernjk_warnings modernjk_defines vendored::fmt)
if(MINGW)
	target_link_options(modernjk_test_fmt PRIVATE -static)
endif()
add_test(NAME modernjk.fmt COMMAND modernjk_test_fmt)

# The tests of src/shared/qcommon/q_rand.h and q_rand.cpp: the streams seeded from one fixed seed
add_executable(modernjk_test_rand qcommon/q_rand.cpp "${PROJECT_SOURCE_DIR}/src/shared/qcommon/q_rand.cpp")
target_include_directories(modernjk_test_rand PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(modernjk_test_rand PRIVATE qlibm modernjk_options modernjk_warnings modernjk_defines vendored::fmt)
if(MINGW)
	target_link_options(modernjk_test_rand PRIVATE -static)
endif()
add_test(NAME modernjk.rand COMMAND modernjk_test_rand)

# The tests of src/shared/sound and of OpenAL Soft as the sound uses it, on the files of sound/data, and of a blade's
# contacts with the world (src/shared/qcommon/q_sound.h)
if(TARGET modernjk_sound AND TARGET vendored::openal)
	add_executable(modernjk_test_sound sound/main.cpp sound/ambient.cpp sound/bank.cpp sound/contacts.cpp sound/decoder.cpp sound/device.cpp sound/stream.cpp
		sound/voices.cpp sound/zones.cpp)
	target_include_directories(modernjk_test_sound PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/src")
	target_link_libraries(modernjk_test_sound PRIVATE modernjk_sound vendored::openal modernjk_options modernjk_warnings
		modernjk_defines vendored::fmt)
	if(MINGW)
		target_link_options(modernjk_test_sound PRIVATE -static)
	endif()
	add_test(NAME modernjk.sound COMMAND modernjk_test_sound "${CMAKE_CURRENT_SOURCE_DIR}/sound/data" "${PROJECT_SOURCE_DIR}/assets")
endif()

# The tests of src/shared/platform and of SDL 3 as it is built for the client, on its dummy video driver (no screen
# needed)
if(TARGET modernjk_platform)
	add_executable(modernjk_test_platform platform/main.cpp platform/input.cpp platform/real.cpp platform/sdl.cpp
		platform/window.cpp)
	target_include_directories(modernjk_test_platform PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/src")
	target_link_libraries(modernjk_test_platform PRIVATE modernjk_platform vendored::sdl modernjk_options modernjk_warnings
		modernjk_defines vendored::fmt)
	if(MINGW)
		target_link_options(modernjk_test_platform PRIVATE -static)
	endif()
	add_test(NAME modernjk.platform COMMAND modernjk_test_platform)
endif()
