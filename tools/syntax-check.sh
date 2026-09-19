#!/usr/bin/env bash
# RIPPLES — parallel-safe syntax check.
#
#   tools/syntax-check.sh Source/DSP/Filters/DepthFilter.cpp [more files...]
#
# Compiles with -fsyntax-only, so it writes no object files and several agents
# can run it at once without fighting over the shared build directory.
# Regenerate the flag cache with:  tools/syntax-check.sh --refresh
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FLAGS_FILE="$ROOT/build/.syntax-flags"

refresh_flags() {
    if [[ ! -f "$ROOT/build/compile_commands.json" ]]; then
        echo "syntax-check: configure the project first:" >&2
        echo "  cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \\" >&2
        echo "        -DRIPPLES_JUCE_DIR=/home/user/JUCE -DCMAKE_EXPORT_COMPILE_COMMANDS=ON" >&2
        return 1
    fi
    python3 - "$ROOT" > "$FLAGS_FILE" <<'PY'
import json, shlex, sys, os
root = sys.argv[1]
db = json.load(open(os.path.join(root, 'build', 'compile_commands.json')))
for e in db:
    if e['file'].endswith('PluginProcessor.cpp'):
        parts = shlex.split(e.get('command') or ' '.join(e['arguments']))
        keep = [p for p in parts if p.startswith(('-I', '-D', '-isystem'))]
        # Re-quote so the shell can re-split this identically.
        print(' '.join(shlex.quote(p) for p in keep))
        break
PY
}

if [[ "${1:-}" == "--refresh" ]]; then
    refresh_flags && echo "syntax-check: flags refreshed" && exit 0
    exit 1
fi

[[ -f "$FLAGS_FILE" ]] || refresh_flags || exit 1

if [[ $# -eq 0 ]]; then
    echo "usage: tools/syntax-check.sh <file.cpp> [...]  |  --refresh" >&2
    exit 2
fi

# Re-split the cached flags while honouring the quoting written above.
eval "FLAGS=( $(cat "$FLAGS_FILE") )"

status=0
for f in "$@"; do
    if [[ ! -f "$f" ]]; then
        echo "syntax-check: no such file: $f" >&2
        status=1
        continue
    fi
    out=$(g++ -std=c++20 -fsyntax-only "${FLAGS[@]}" -I"$ROOT/Source" "$f" 2>&1)
    if grep -qE "error:|fatal error:" <<< "$out"; then
        grep -E "error:|fatal error:" <<< "$out" | head -20
        echo "  FAIL  $f" >&2
        status=1
    else
        echo "  ok    $f"
    fi
done
exit $status
