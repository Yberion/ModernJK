"""Makes the copy of the code that the warning counts of plan step 19 build: the files of the folders the build reads
(tracked by git, or new and not ignored), with every MSVC '#pragma warning(disable ...)' commented out, so that MSVC reports what they hide (they
are to be reviewed one by one). The copy is not a git repository (the build then names its version vUNKNOWN).

Usage, from the repository's root: python -I docs_rewrite/modernization/tools/warnings/measure_tree.py <copy folder>
The folder must not exist. Next to the repository (E:/Git/jka/ModernJK_s19measure), both VMs see it in their shared
folder.
"""
import pathlib
import re
import subprocess
import sys

# README.md and LICENSE.txt: CPack's resource files (cmake/Modules/InstallConfig.cmake), which it requires
FOLDERS = ['CMakeLists.txt', 'README.md', 'LICENSE.txt', 'cmake', 'code', 'codeJK2', 'codemp', 'shared', 'lib',
	'tests', 'tools', 'docs_rewrite/modernization/tools/warnings']
PRAGMA = re.compile(rb'^([ \t]*)(#[ \t]*pragma[ \t]+warning[ \t]*\([ \t]*disable\b)', re.MULTILINE)


def main():
	dest = pathlib.Path(sys.argv[1])
	if dest.exists():
		sys.exit(f'{dest} exists')
	files = subprocess.run(['git', 'ls-files', '-z', '--cached', '--others', '--exclude-standard', '--', *FOLDERS],
		check=True, capture_output=True).stdout
	copied = changed = pragmas = 0
	for name in files.decode('utf-8').split('\0'):
		if not name:
			continue
		src = pathlib.Path(name)
		data = src.read_bytes()
		if src.suffix.lower() in ('.h', '.hpp', '.c', '.cpp', '.inl') and not name.startswith('lib/'):
			data, n = PRAGMA.subn(rb'\1// step 19 measurement: \2', data)
			if n:
				changed += 1
				pragmas += n
		target = dest / name
		target.parent.mkdir(parents=True, exist_ok=True)
		target.write_bytes(data)
		copied += 1
	print(f'{copied} files copied to {dest}; {pragmas} pragmas commented out in {changed} files')


if __name__ == '__main__':
	main()
