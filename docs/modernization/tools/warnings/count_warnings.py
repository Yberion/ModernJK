"""Counts the warnings of build logs per flag (plan step 19), each source location once: a header's warning repeats
in every file that includes it, and MSBuild repeats every warning in its summary.

Usage: python -I count_warnings.py <out.json> <log>... (a log may be gzipped, UTF-8 or UTF-16 as PowerShell's
Tee-Object writes it). Reads clang and GCC lines ('file:line:col: warning: text [-Wflag]'), clang-tidy and cppcheck
lines ('file:line:col: warning: text [check-name]') and MSVC lines ('file(line,col): warning C4242: text').
Prints one line per flag, most frequent first: the locations outside libs/, the count per tree, then one example.
Writes every location to <out.json>.
"""
import gzip
import json
import posixpath
import re
import sys
from collections import defaultdict

GNU = re.compile(r'^(?P<file>.*?):(?P<line>\d+):(?P<col>\d+): (?:warning|style|performance|portability|error): '
	r'(?P<msg>.*) \[(?P<flag>-W[^\],=]+|[A-Za-z][\w.\-]*)[^\]]*\]\s*$')
MSVC = re.compile(r'^(?:\s*\d+>)?(?P<file>.*?)\((?P<line>\d+)(?:,(?P<col>\d+))?\): warning (?P<flag>C\d{4}): '
	r'(?P<msg>.*?)(?: \[[^\]]*\])?\s*$')
TREES = ('libs/', 'src/codemp/netcodec/', 'src/codemp/', 'src/code/', 'src/codejk2/', 'src/shared/', 'test/', 'tools/')


def tree_of(path):
	"""The tree of a file and its path from the repository's root (or from a copy of it), lower case."""
	# one header reached through several relative paths (src/code/client/../qcommon/q_shared.h) is one file
	p = posixpath.normpath(path.replace('\\', '/').lower())
	for root in ('/modernjk_s19measure/', '/modernjk/'):
		i = p.find(root)
		if i >= 0:
			p = p[i + len(root):]
			break
	while p.startswith('../'):
		p = p[3:]
	for t in TREES:
		if p.startswith(t):
			return t.rstrip('/'), p
	return 'other', p


def lines_of(name):
	with open(name, 'rb') as f:
		head = f.read(2)
	if head == b'\x1f\x8b':
		f = gzip.open(name, 'rb')
		data = f.read()
		f.close()
	else:
		with open(name, 'rb') as f:
			data = f.read()
	if data[:2] in (b'\xff\xfe', b'\xfe\xff'):
		text = data.decode('utf-16', errors='replace')
	else:
		text = data.decode('utf-8', errors='replace')
	return text.splitlines()


def main():
	out, logs = sys.argv[1], sys.argv[2:]
	seen = {}
	unread = 0
	for name in logs:
		for line in lines_of(name):
			m = GNU.match(line) or MSVC.match(line)
			if not m:
				# a warning line no pattern reads: two outputs cut into each other, or a format to add
				unread += ': warning' in line or ': warning C' in line
				continue
			tree, rel = tree_of(m['file'])
			key = (rel, int(m['line']), int(m['col'] or 0), m['flag'])
			if key not in seen:
				seen[key] = (tree, m['msg'])
	per = defaultdict(lambda: defaultdict(int))
	example = {}
	for (rel, line, col, flag), (tree, msg) in sorted(seen.items()):
		per[flag][tree] += 1
		if tree != 'libs' and flag not in example:
			example[flag] = f'{rel}:{line}: {msg}'
	rows = sorted(((sum(n for t, n in trees.items() if t != 'libs'), flag, trees) for flag, trees in per.items()),
		key=lambda r: (-r[0], r[1]))
	sys.stdout.reconfigure(encoding='utf-8')
	print(f'{len(seen)} locations; {unread} warning lines not read')
	for ours, flag, trees in rows:
		detail = ' '.join(f'{t}={n}' for t, n in sorted(trees.items()))
		print(f'{ours:7d} {flag:45s} {detail}\n        {example.get(flag, "(libs/ only)")[:220]}')
	with open(out, 'w', encoding='utf-8') as f:
		json.dump([{'file': k[0], 'line': k[1], 'col': k[2], 'flag': k[3], 'tree': v[0], 'msg': v[1]}
			for k, v in sorted(seen.items())], f)


if __name__ == '__main__':
	main()
