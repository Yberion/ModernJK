"""The warnings of the three compilers grouped by what they find (plan step 19), each group's locations outside libs/
counted per compiler from count_warnings.py's JSON files. clang's count is the union of its runs (Windows and
Linux). A flag no group names is listed at the end, so that none is forgotten.

Usage: python -I groups.py --clang <json>... --gcc <json> --msvc <json>
"""
import argparse
import json
import posixpath
from collections import defaultdict

GROUPS = [
	('conversions that change a value (integer, float, sign, 64 to 32 bits)',
	 ['-Wimplicit-int-float-conversion', '-Wfloat-conversion', '-Wimplicit-float-conversion', '-Wshorten-64-to-32',
	  '-Wimplicit-int-conversion', '-Wsign-conversion', '-Wimplicit-int-conversion-on-negation', '-Wconversion',
	  '-Warith-conversion', 'C4244', 'C4365', 'C5219', 'C4267', 'C4242', 'C4305', 'C4245', 'C4388', 'C4018',
	  'C4800', 'C4310', '-Wstring-conversion', '-Wtautological-value-range-compare', '-Wsign-compare']),
	('float promoted to double', ['-Wdouble-promotion']),
	('C-style casts', ['-Wold-style-cast']),
	('casts that drop const, raise the alignment or change a function type',
	 ['-Wcast-qual', '-Wcast-align', '-Wcast-function-type-strict', '-Wcast-function-type', 'C4191',
	  '-Wunaligned-access']),
	('a definition without a declaration (internal linkage or a header)',
	 ['-Wmissing-prototypes', '-Wmissing-variable-declarations', '-Wmissing-declarations']),
	('unused parameters, macros, constants, functions',
	 ['-Wunused-parameter', '-Wunused-macros', '-Wunused-const-variable', '-Wused-but-marked-unused', 'C4100',
	  'C4505', 'C5264', '-Wunused-member-function']),
	('a name that hides another', ['-Wshadow', 'C4456', 'C4457', 'C4458', 'C4459']),
	('a switch on an enumeration without every enumerator, or a default',
	 ['-Wswitch-enum', '-Wswitch-default', '-Wcovered-switch-default', 'C4061', 'C4062']),
	('implicit fall-through between cases', ['-Wimplicit-fallthrough', 'C5262']),
	('floats compared with == or !=', ['-Wfloat-equal']),
	('classes: virtual destructor, override, copy and destructor rules',
	 ['-Wnon-virtual-dtor', '-Wsuggest-override', '-Wsuggest-destructor-override', '-Wdeprecated-copy-dtor',
	  'C4265', 'C5204', 'C5267', 'C4263', 'C4264', '-Wdeprecated-copy-with-user-provided-dtor',
	  '-Winconsistent-missing-destructor-override']),
	('the format strings', ['-Wformat', '-Wformat-nonliteral', '-Wformat-signedness', '-Wformat-overflow',
	  '-Wmissing-format-attribute', '-Wsuggest-attribute', '-Wformat-extra-args']),
	('reserved identifiers (a leading underscore and capital, a double underscore)',
	 ['-Wreserved-identifier', '-Wreserved-macro-identifier']),
	('code never executed', ['-Wunreachable-code', '-Wunreachable-code-break', '-Wunreachable-code-return', 'C4702']),
	('the preprocessor: an undefined macro in #if, __DATE__ and __TIME__', ['-Wundef', '-Wdate-time', 'C4668']),
	('initialisation: missing fields and braces, a variable perhaps used uninitialised',
	 ['-Wmissing-field-initializers', '-Wmissing-braces', 'C4701', 'C4703', 'C5246', '-Wconditional-uninitialized']),
	('0 as a null pointer', ['-Wzero-as-null-pointer-constant']),
	('extra semicolons', ['-Wextra-semi', '-Wextra-semi-stmt']),
	('redundant: casts to the same type, declarations repeated', ['-Wuseless-cast', '-Wredundant-decls']),
	('the text: invalid UTF-8, no newline at the end, /* in a comment, trailing spaces',
	 ['-Winvalid-utf8', '-Wnewline-eof', '-Wcomment', '-Wtrailing-whitespace', 'C4125']),
	('qualifiers ignored, empty files, nameless structs, a comma in an index, deprecated functions',
	 ['-Wignored-qualifiers', 'C5266', 'C4206', 'C4201', 'C4709', 'C4996', '-Wpedantic', '-Wdocumentation',
	  '-Wnested-anon-types', '-Wshadow-header', '-Wcomma', '-Wduplicate-enum', '-Wgnu-anonymous-struct',
	  '-Wunique-object-duplication']),
	('found by GCC with the optimiser: null dereference, duplicated branches and conditions, logical operators',
	 ['-Wnull-dereference', '-Wduplicated-branches', '-Wduplicated-cond', '-Wlogical-op']),
	('a template used without its definition (the single player saved games)', ['-Wundefined-func-template']),
	('safe buffers: every pointer arithmetic and indexing, the C library calls',
	 ['-Wunsafe-buffer-usage', '-Wunsafe-buffer-usage-in-libc-call', '-Wunsafe-buffer-usage-in-format-attr-call',
	  '-Wunsafe-buffer-usage-in-container']),
	('global constructors and exit-time destructors, vtables in every file',
	 ['-Wglobal-constructors', '-Wexit-time-destructors', '-Wweak-vtables']),
	("clang 23's lifetime analysis (suggestions)",
	 ['-Wlifetime-safety-intra-tu-suggestions', '-Wlifetime-safety-intra-tu-constructor-suggestions',
	  '-Wlifetime-safety-cross-tu-suggestions', '-Wlifetime-safety-cross-tu-constructor-suggestions',
	  '-Wlifetime-safety-invalidation', '-Wnrvo']),
	('style rules: Effective C++, templates, namespaces, aggregate returns, inlining, final, elaborated types',
	 ['-Weffc++', '-Wtemplates', '-Wnamespaces', '-Waggregate-return', '-Winline', '-Wsuggest-final-methods',
	  '-Wsuggest-final-types', '-Wredundant-tags', '-Wnoexcept', '-Wconditionally-supported', '-Wcatch-value',
	  '-Wctor-dtor-privacy', '-Wmissing-noreturn', '-Wredundant-parens']),
	('MSVC only: relative include paths, implicitly deleted members, extern "C" and exceptions, alignment',
	 ['C4464', 'C4623', 'C4625', 'C4626', 'C5027', 'C5039', 'C5029', 'C4255', 'C4146']),
]


def norm(p):
	return posixpath.normpath(p.replace('\\', '/').lower())


def load(names):
	locs = set()
	for name in names:
		for e in json.load(open(name, encoding='utf-8')):
			if e['tree'] != 'libs':
				locs.add((norm(e['file']), e['line'], e['col'], e['flag']))
	per = defaultdict(int)
	for loc in locs:
		per[loc[3]] += 1
	return per


def main():
	ap = argparse.ArgumentParser()
	ap.add_argument('--clang', nargs='+', required=True)
	ap.add_argument('--gcc', nargs='+', required=True)
	ap.add_argument('--msvc', nargs='+', required=True)
	a = ap.parse_args()
	comp = {'clang': load(a.clang), 'GCC': load(a.gcc), 'MSVC': load(a.msvc)}
	named = set()
	print('| # | what the warnings find | clang | GCC | MSVC | flags |')
	print('|---|---|---|---|---|---|')
	for i, (title, flags) in enumerate(GROUPS, 1):
		named.update(flags)
		cells = []
		for c in ('clang', 'GCC', 'MSVC'):
			n = sum(comp[c].get(f, 0) for f in flags)
			cells.append(f'{n:,}' if n else '')
		present = [f'`{f}`' for f in flags if any(comp[c].get(f, 0) for c in comp)]
		print(f'| {i} | {title} | ' + ' | '.join(cells) + ' | ' + ', '.join(present) + ' |')
	left = sorted({f for c in comp.values() for f in c} - named)
	if left:
		print('\nin no group:', ', '.join(f'{f} ' + '/'.join(str(comp[c].get(f, 0)) for c in comp) for f in left))


if __name__ == '__main__':
	main()
