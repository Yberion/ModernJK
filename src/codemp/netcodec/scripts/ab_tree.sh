# Sourced by the A/B scripts: the copies of netcodec they build, each in the layout of the tree it comes from. The
# sources are in src/ and the libraries in libs/; a reference commit or copy older than that layout has codemp/ and lib/.
# netcodec's CMakeLists.txt finds LLVM libc at its place beside it, so the copies take it too. A copy without the legacy
# benchmark's sources (codemp/qcommon/msg.cpp) is built with the working tree's, which must have the same layout.

# ab_netcodec <root>: netcodec's folder in a tree, relative to its root
ab_netcodec() { if [ -d "$1/src/codemp/netcodec" ]; then echo src/codemp/netcodec; else echo codemp/netcodec; fi; }

# ab_has_engine <root>: whether a tree holds the legacy benchmark's sources
ab_has_engine() { [ -f "$1/$(dirname "$(ab_netcodec "$1")")/qcommon/msg.cpp" ]; }

# ab_copy <root> <dir>: netcodec and LLVM libc of the tree <root> copied into <dir> at their places (LLVM libc from the
# working tree $R when <root> has none)
ab_copy() {
	local n l c
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
	for p in src/codemp src/shared libs/gsl-lite libs/llvm-libc codemp shared lib/gsl-lite lib/llvm-libc; do
		git -c safe.directory='*' -C "$R" cat-file -e "$1:$p" 2> /dev/null && paths+=("$p")
	done
	git -c safe.directory='*' -C "$R" archive "$1" "${paths[@]}" | tar -x -C "$2"
}
