#!/bin/bash
# For ab_msvc.bat when the Windows VM has no git: writes the reference source on the host (this machine, Git Bash;
# netcodec, LLVM libc and the ModernJK sources its legacy benchmark builds with, in the commit's layout: ab_tree.sh)
# into build-netcodec/ab-msvc/ref (shared with the VM) and its commit into build-netcodec/ab-msvc/ref.txt
# ("<full hash> <short hash> <subject>"); ab_msvc.bat refuses a copy whose commit is not HEAD.
# Usage, from anywhere: bash src/codemp/netcodec/scripts/ab_msvc_ref.sh [commit]   (default HEAD)
set -e
R="$(cd "$(dirname "$0")/../../../.." && pwd)"
. "$(dirname "$0")/ab_tree.sh"
C=${1:-HEAD}
D="$R/build-netcodec/ab-msvc"
rm -rf "$D/ref" && mkdir -p "$D/ref"
ab_archive "$C" "$D/ref"
git -C "$R" log -1 --format='%H %h %s' "$C" > "$D/ref.txt"
echo "reference written: $(cat "$D/ref.txt")"
