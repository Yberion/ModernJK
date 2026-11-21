# Sourced by the A/B scripts: the copies of netcodec they build, each in the layout of the tree it comes from.
#  - project: a tree configured from its root (ProjectOptions.cmake at the root): the copy holds the build's files,
#    src/shared and src/codemp (netcodec and the legacy benchmark's sources) and LLVM libc, configured from its root
#    with the games, the tests and the tools off;
#  - netcodec: an older tree, where netcodec is a project of its own (src/codemp/netcodec, or codemp/netcodec before
#    the sources moved under src/; LLVM libc in libs/, or lib/): the copy holds netcodec and LLVM libc, configured
#    from netcodec's folder. A copy without the legacy benchmark's sources is built with the working tree's.

# ab_layout <root>: project or netcodec
ab_layout() { if [ -f "$1/ProjectOptions.cmake" ]; then echo project; else echo netcodec; fi; }

# ab_netcodec <root>: netcodec's folder in a tree, relative to its root
ab_netcodec() { if [ -d "$1/src/codemp/netcodec" ]; then echo src/codemp/netcodec; else echo codemp/netcodec; fi; }

# ab_has_engine <root>: whether a tree holds the legacy benchmark's sources
ab_has_engine() { [ -f "$1/$(dirname "$(ab_netcodec "$1")")/qcommon/msg.cpp" ]; }

AB_PROJECT_PATHS="CMakeLists.txt CMakePresets.json ProjectOptions.cmake Dependencies.cmake cmake src/CMakeLists.txt src/shared src/codemp libs/llvm-libc"

# ab_copy <root> <dir>: the files of the tree <root> an A/B build needs, copied into <dir> at their places (LLVM libc
# from the working tree $R when an older <root> has none)
ab_copy() {
	local n l c p
	if [ "$(ab_layout "$1")" = project ]; then
		for p in $AB_PROJECT_PATHS; do
			mkdir -p "$2/$(dirname "$p")" && cp -r "$1/$p" "$2/$(dirname "$p")/" || return 1
		done
		return 0
	fi
	n=$(ab_netcodec "$1")
	l=lib
	[ "$n" = src/codemp/netcodec ] && l=libs
	c="$1/$l/llvm-libc"
	[ -d "$c" ] || c="$R/libs/llvm-libc"
	mkdir -p "$2/$(dirname "$n")" "$2/$l" && cp -r "$1/$n" "$2/$(dirname "$n")/" && cp -r "$c" "$2/$l/" &&
		rm -rf "$2/$n/build-netcodec"
}

# ab_archive <commit> <dir>: the same from a commit of the repository $R, with its legacy benchmark's sources
ab_archive() {
	local p paths=()
	if git -c safe.directory='*' -C "$R" cat-file -e "$1:ProjectOptions.cmake" 2> /dev/null; then
		paths=($AB_PROJECT_PATHS)
	else
		for p in src/codemp src/shared libs/gsl-lite libs/llvm-libc codemp shared lib/gsl-lite lib/llvm-libc; do
			git -c safe.directory='*' -C "$R" cat-file -e "$1:$p" 2> /dev/null && paths+=("$p")
		done
	fi
	git -c safe.directory='*' -C "$R" archive "$1" "${paths[@]}" | tar -x -C "$2"
}

# ab_configure <copy> <build folder> <tree of the legacy benchmark's sources> [cmake arguments...]: configures the
# copy for netcodec's benchmarks, in its layout
ab_configure() {
	local copy=$1 build=$2 engine=$3
	shift 3
	if [ "$(ab_layout "$copy")" = project ]; then
		cmake -S "$copy" -B "$build" -Dmodernjk_BUILD_MP_CLIENT=OFF -Dmodernjk_BUILD_MP_SERVER=OFF \
			-Dmodernjk_BUILD_MP_GAME=OFF -Dmodernjk_BUILD_SP=OFF -Dmodernjk_BUILD_JK2=OFF -Dmodernjk_BUILD_ASSETS_PK3=OFF \
			-DBUILD_TESTING=OFF -Dmodernjk_BUILD_TOOLS=OFF -Dmodernjk_BUILD_BENCHMARKS=ON \
			-Dnetcodec_ENGINE_DIR="$engine" -Dnetcodec_TRACE_DIR="$R/docs/modernization/data/traces" "$@"
	else
		cmake -S "$copy/$(ab_netcodec "$copy")" -B "$build" \
			-Dnetcodec_MODERNJK_DIR="$engine" -Dnetcodec_TRACE_DIR="$R/docs/modernization/data/traces" "$@"
	fi
}

# ab_bench <copy> <build folder> <benchmark: codec or client>: the benchmark's program
ab_bench() {
	if [ "$(ab_layout "$1")" = project ]; then echo "$2/bin/netcodec_bench_$3"; else echo "$2/bench/netcodec_bench_$3"; fi
}

# ab_library <copy> <build folder>: the library's archive
ab_library() {
	if [ "$(ab_layout "$1")" = project ]; then echo "$2/src/codemp/netcodec/libnetcodec.a"; else echo "$2/libnetcodec.a"; fi
}
