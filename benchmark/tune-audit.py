#!/usr/bin/env python3
"""Cross-check every deep tuning field across the four places it must appear."""
import re, sys, collections

H = open('ai/deep/deeptuning.h').read()
C = open('ai/deep/deeptuning.c').read()
P = open('benchmark/tuning-params.ini').read()
B = open('benchmark/tuning-blocks.ini').read()

# 1. struct fields
m = re.search(r'struct deep_tuning\s*\{(.*?)\n\};', H, re.S)
body = m.group(1)
fields = []
for line in body.split('\n'):
    line = re.sub(r'/\*.*?\*/', '', line)
    mm = re.match(r'\s*(double|int|bool)\s+([a-z_0-9]+)\s*;', line)
    if mm:
        fields.append((mm.group(2), mm.group(1)))

# 2. loader macros -- skipping the #define lines, whose parameter is
#    literally named "field" and is not a tuning field.
C_uses = '\n'.join(l for l in C.split('\n') if not l.lstrip().startswith('#define'))
loaded = {n: t for t, n in re.findall(r'TUNE_(DOUBLE|INT|BOOL)\((\w+)\)', C_uses)}

# 3. defaults
defaults = set(re.findall(r'^\s*\.(\w+)\s*=', C, re.M))

# 4. params registry
params = {}
for line in P.split('\n'):
    mm = re.match(r'\s*([a-z_0-9]+)\s*=\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*(\w+)', line)
    if mm:
        params[mm.group(1)] = (mm.group(2).strip(), mm.group(3).strip(),
                               mm.group(4).strip(), mm.group(5).strip())

# 5. blocks
blocks = {}
for line in B.split('\n'):
    mm = re.match(r'\s*([A-Z0-9]+)\s*=\s*(.+)$', line)
    if mm:
        blocks[mm.group(1)] = [x.strip() for x in mm.group(2).split(',')]
allblock = set(blocks.get('ALL', []))

print("struct fields: %d | loader macros: %d | params.ini: %d | ALL block: %d\n"
      % (len(fields), len(loaded), len(params), len(allblock)))

bad = collections.defaultdict(list)
for name, ctype in fields:
    if name not in loaded:
        bad['NOT LOADABLE from ini (no TUNE_ macro)'].append(name)
    elif (ctype == 'double') != (loaded[name] == 'DOUBLE'):
        bad['loader type mismatch vs struct'].append(
            '%s (struct %s, macro TUNE_%s)' % (name, ctype, loaded[name]))
    if name not in defaults:
        bad['no compiled default'].append(name)
    if name not in params:
        bad['NOT in tuning-params.ini (tuner cannot reach it)'].append(name)
    elif name not in allblock:
        bad['in params.ini but NOT in ALL block'].append(name)
    if name in params:
        kind = params[name][3]
        if (ctype == 'double') != (kind == 'float'):
            bad['params.ini type mismatch'].append(
                '%s (struct %s, ini %s)' % (name, ctype, kind))

# reverse: registry entries with no struct field
for name in params:
    if name not in dict(fields):
        bad['in params.ini but NO SUCH struct field'].append(name)
for bn, members in blocks.items():
    for name in members:
        if name and name not in dict(fields):
            bad['in block %s but NO SUCH struct field' % bn].append(name)

if not bad:
    print("CLEAN: every field loadable, defaulted, registered and reachable.")
for k in sorted(bad):
    print("== %s (%d)" % (k, len(bad[k])))
    for v in sorted(set(bad[k])):
        print("   %s" % v)
