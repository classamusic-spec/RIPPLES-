#!/usr/bin/env python3
"""
RIPPLES — static preset checker.

    tools/lint-presets.py Source/Presets/Banks/BankSurface.cpp [more...]
    tools/lint-presets.py --all

Parses bank files without building anything and flags the mistakes that are
expensive to find later: names that collide, tags outside the vocabulary,
presets that are barely programmed, a filter that DEPTH will shut below the
note, and a sub that will bury the oscillators.

Exit code is non-zero if any ERROR is reported. WARNs are judgement calls.
"""
import sys, os, re, glob, math
from collections import defaultdict

CATEGORIES = {"Pad","Key","Pluck","Bass","Lead","Arp","Texture","Drone","FX"}
TAGS = {"Deep","Wet","Dark","Bright","Dreamy","Glassy","Organic","Chaotic",
        "Calm","Cinematic","Submerged","Surface"}

def smootherstep(t):
    t = max(0.0, min(1.0, t))
    return t*t*t*(t*(t*6-15)+10)

def depth_cutoff_multiplier(depth, glow):
    return (2.0 ** (-2.2 * smootherstep(depth))) * (1.0 + 1.6 * glow)

# make (kBank, "NAME", "Category", { "Tag", ... }, Dry::X,
PRESET_RE = re.compile(
    r'make\s*\(\s*(k\w+)\s*,\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*\{([^}]*)\}\s*,\s*Dry::(\w+)',
    re.S)

def parse(path):
    src = open(path).read()
    out = []
    for m in PRESET_RE.finditer(src):
        bank, name, cat, tagblob, dry = m.groups()
        tags = re.findall(r'"([^"]+)"', tagblob)
        # Body runs to the start of the next make(, or end of file.
        nxt = PRESET_RE.search(src, m.end())
        body = src[m.end(): nxt.start() if nxt else len(src)]
        out.append(dict(path=path, bank=bank, name=name, cat=cat, tags=tags,
                        dry=(dry == "Yes"), body=body,
                        line=src.count('\n', 0, m.start()) + 1))
    return out

def num(body, pid, fn):
    """Finds  { pid::<pid>, <fn>(<value>) }  and returns the value."""
    m = re.search(r'pid::' + pid + r'\s*,\s*' + fn + r'\s*\(\s*(-?[\d.]+)f?\s*\)', body)
    return float(m.group(1)) if m else None

def plain(body, pid):
    m = re.search(r'pid::' + pid + r'\s*,\s*(-?[\d.]+)f?\s*\}', body)
    return float(m.group(1)) if m else None

def check(presets):
    errors = warns = 0
    seen = {}

    for p in presets:
        where = f"{os.path.basename(p['path'])}:{p['line']} {p['name']}"

        def err(msg):
            nonlocal errors; errors += 1; print(f"ERROR  {where}: {msg}")
        def warn(msg):
            nonlocal warns; warns += 1; print(f"WARN   {where}: {msg}")

        key = p['name'].upper()
        if key in seen:
            err(f"duplicate name (also {os.path.basename(seen[key]['path'])}:{seen[key]['line']})")
        else:
            seen[key] = p

        if p['cat'] not in CATEGORIES:
            err(f"category '{p['cat']}' is not in the vocabulary")

        bad = [t for t in p['tags'] if t not in TAGS]
        if bad:
            err(f"tags outside the vocabulary: {bad}")
        if not 2 <= len(p['tags']) <= 4:
            warn(f"{len(p['tags'])} tags (guide says 2-4)")

        isInit = p['name'].upper().startswith('INIT')
        overrides = len(re.findall(r'\{\s*pid::', p['body']))
        mods = len(re.findall(r'ModSource::', p['body']))
        if overrides < 40 and not isInit:
            warn(f"only {overrides} parameter overrides (bank standard is 60-120)")
        if mods and mods < 4:
            warn(f"only {mods} modulation slots (bank standard is 5-12)")
        if mods == 0 and not isInit:
            warn("no modulation slots at all")

        if p['dry'] and 'RIPPLES_FX_BYPASSED' not in p['body']:
            err("marked Dry::Yes but does not use RIPPLES_FX_BYPASSED")

        # --- the expensive mistake: DEPTH shutting the filter below the note ---
        cutoff = num(p['body'], 'filtCutoff', 'hz')
        depth  = plain(p['body'], 'macroDepth') or 0.0
        glow   = plain(p['body'], 'macroGlow') or 0.0

        if cutoff is not None:
            eff = cutoff * depth_cutoff_multiplier(depth, glow)
            if eff < 200.0 and p['cat'] not in ('Bass', 'Drone', 'FX'):
                err(f"filter effectively {eff:.0f} Hz after macroDepth={depth:.2f} "
                    f"(set {cutoff:.0f} Hz) - below the note, only the sub will speak")
            elif eff < 350.0 and p['cat'] in ('Pluck', 'Lead', 'Arp', 'Key'):
                warn(f"filter effectively {eff:.0f} Hz - dull for a {p['cat']}")

        # --- sub burying the oscillators ---
        sub  = num(p['body'], 'subLevel', 'lvl')
        oscA = num(p['body'], 'oscALevel', 'lvl')
        if sub is not None and oscA is not None and sub > -55.0:
            margin = oscA - sub
            if margin < 8.0 and p['cat'] not in ('Bass', 'Drone'):
                err(f"sub only {margin:.1f} dB below oscA (guide says 14-24) - it will dominate")
            elif margin < 12.0 and p['cat'] not in ('Bass', 'Drone'):
                warn(f"sub {margin:.1f} dB below oscA - likely too loud")

        noise = num(p['body'], 'noiseLevel', 'lvl')
        if noise is not None and oscA is not None and noise > oscA + 2.0:
            warn(f"noise {noise - oscA:+.0f} dB relative to oscA - it will lead the patch")

    return errors, warns

if __name__ == "__main__":
    args = sys.argv[1:]
    if not args or args == ["--all"]:
        args = sorted(glob.glob("Source/Presets/Banks/Bank*.cpp"))

    presets = []
    for a in args:
        presets += parse(a)

    if not presets:
        print("no presets found - is the path right?")
        sys.exit(2)

    errors, warns = check(presets)

    by_bank = defaultdict(int); by_cat = defaultdict(int)
    dry = 0
    for p in presets:
        by_bank[p['bank']] += 1; by_cat[p['cat']] += 1; dry += p['dry']

    print(f"\n{len(presets)} presets, {dry} dry-aquatic, {errors} errors, {warns} warnings")
    print("  by bank:     " + ", ".join(f"{k}={v}" for k, v in sorted(by_bank.items())))
    print("  by category: " + ", ".join(f"{k}={v}" for k, v in sorted(by_cat.items())))
    sys.exit(1 if errors else 0)
