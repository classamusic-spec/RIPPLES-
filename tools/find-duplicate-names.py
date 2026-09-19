#!/usr/bin/env python3
"""
RIPPLES — cross-bank duplicate preset names.

Bank files are written independently, so two banks can land on the same
evocative name without either author seeing the clash. This lists every
collision with enough context to decide which one keeps the name.

    tools/find-duplicate-names.py
"""
import re, glob, sys
from collections import defaultdict

PRESET_RE = re.compile(
    r'make\s*\(\s*(k\w+)\s*,\s*"([^"]+)"\s*,\s*"([^"]+)"', re.S)

seen = defaultdict(list)
for path in sorted(glob.glob("Source/Presets/Banks/Bank*.cpp")):
    src = open(path).read()
    for m in PRESET_RE.finditer(src):
        bank, name, cat = m.groups()
        line = src.count('\n', 0, m.start()) + 1
        seen[name.upper()].append((path, line, bank, cat, name))

dups = {k: v for k, v in seen.items() if len(v) > 1}

if not dups:
    print("no duplicate preset names")
    sys.exit(0)

print(f"{len(dups)} duplicated name(s):\n")
for name, entries in sorted(dups.items()):
    print(f"  {name}")
    for path, line, bank, cat, _ in entries:
        print(f"      {path.split('/')[-1]}:{line}  {bank:<12} {cat}")
    print()
sys.exit(1)
