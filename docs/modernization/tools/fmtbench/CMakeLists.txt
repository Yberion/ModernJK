# The formatting benchmark of plan step 22 (fmtbench.cpp) and the size probe (size_probe.cpp), on libs/fmt built as
# the project builds it: a static library from src/format.cc. Run by run.sh (llvm-mingw clang, GCC) and run.bat (MSVC).
#   cmake -S <this folder> -B <build> -G Ninja -DCMAKE_BUILD_TYPE=Release -DREPO=<repository root> [-DFMT_REF=<copy>]
cmake_minimum_required(VERSION 4.5.0)
project(fmtbench CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(REPO "" CACHE PATH "repository root")
if(MINGW)
	add_link_options(-static) # no runtime DLL to find
endif()
if(MSVC)
	add_compile_options(/utf-8 /EHsc /permissive-)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

add_library(fmt STATIC "${REPO}/libs/fmt/src/format.cc")
target_include_directories(fmt SYSTEM PUBLIC "${REPO}/libs/fmt/include")

add_executable(fmtbench fmtbench.cpp)
target_link_libraries(fmtbench PRIVATE fmt)

# An unpatched {fmt} (the same release without libs/fmt/patches) to compare libs/fmt's patches with: fmtbench_ref
set(FMT_REF "" CACHE PATH "a copy of libs/fmt without its patches")
if(FMT_REF)
	add_library(fmt_ref STATIC "${FMT_REF}/src/format.cc")
	target_include_directories(fmt_ref SYSTEM PUBLIC "${FMT_REF}/include")
	add_executable(fmtbench_ref fmtbench.cpp)
	target_link_libraries(fmtbench_ref PRIVATE fmt_ref)
endif()

add_executable(size_printf size_probe.cpp)
target_compile_definitions(size_printf PRIVATE SIZE_PRINTF)
add_executable(size_fmt size_probe.cpp)
target_compile_definitions(size_fmt PRIVATE SIZE_FMT)
target_link_libraries(size_fmt PRIVATE fmt)
