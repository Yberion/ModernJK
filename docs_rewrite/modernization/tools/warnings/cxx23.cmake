# Plan step 19, measurements only: given as CMAKE_PROJECT_INCLUDE, it makes CMake emit C++23's flag where the
# targets ask C++20 (CMAKE_CXX_STANDARD 20, cxx_std_20), until the project itself moves to C++23.
if(MSVC)
	set(CMAKE_CXX20_STANDARD_COMPILE_OPTION "-std:c++23")
	set(CMAKE_CXX20_EXTENSION_COMPILE_OPTION "-std:c++23")
else()
	set(CMAKE_CXX20_STANDARD_COMPILE_OPTION "-std=c++23")
	set(CMAKE_CXX20_EXTENSION_COMPILE_OPTION "-std=gnu++23")
endif()
