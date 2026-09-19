# RIPPLES — Preset Authoring Guide

This is what was learned by measuring the assembled instrument. Following it is
what separates a bank that sounds designed from one that sounds generated.

## The one mistake that matters most

The first bank was written without sight of `MacroEngine`, and it cost us. A
patch would set a sensible filter cutoff, then set `macroDepth` high for
character — and the macro would slam the filter shut below the note's own
fundamental, leaving nothing but the sub audible. A C3 on the flagship preset
came out with **78% of its energy below 90 Hz**.

**The macros multiply what you set. They are not independent.**

### What DEPTH actually does to your cutoff

`macroDepth` multiplies `filtCutoff` by `2^(-2.2 * smootherstep(depth))`, and
`macroGlow` multiplies it back up by `(1 + 1.6 * glow)`.

| macroDepth | cutoff multiplier | a 2000 Hz cutoff becomes |
|---|---|---|
| 0.0 | 1.00 | 2000 Hz |
| 0.3 | 0.79 | 1580 Hz |
| 0.5 | 0.46 | 920 Hz |
| 0.7 | 0.28 | 555 Hz |
| 0.9 | 0.17 | 340 Hz |
| 1.0 | 0.22 → 0.15 | 300 Hz |

So **set `filtCutoff` for how the patch should sound with macros at zero, then
check what DEPTH leaves you.** If you want a dark pad that still speaks, write
`hz(3000.0f)` with `macroDepth` 0.7, not `hz(900.0f)`.

DEPTH and PRESSURE also each add low end by three separate routes (sub gain,
the master low shelf, and the filter's own PRESSURE tilt). Those compound. Do
not additionally push `subLevel` up to "make it deep".

## Level discipline

The sub oscillator is a pure sine with all its energy in one place, while the
main oscillators spread theirs across a harmonic series that the filter then
eats. **A sub that looks 10 dB quieter on paper can still dominate the output.**

- `subLevel` typically sits **14–24 dB below `oscALevel`**. Below `-20 dB` for
  anything that is not explicitly a bass or drone patch.
- A patch's own peak should land roughly **-12 to -4 dBFS** on a single note at
  velocity 0.8. The bank currently spans 22 dB, which is too wide — a user
  should not have to ride the output knob between presets.
- `noiseLevel` above `-18 dB` will dominate a quiet patch. Most patches want
  `-40` to `-24 dB`.

## Target tonal balance

Measured on a single C3 (MIDI 48). Use `tools/lint-presets.py` and the render
loop below to check.

| Patch type | sub 40–90 Hz | fundamental 100–170 | above 400 Hz |
|---|---|---|---|
| Bass, Drone | 25–45% | 15–30% | >2% |
| Pad, Key, Texture | 5–25% | 15–35% | >8% |
| Pluck, Lead, Arp | <15% | 20–40% | >15% |
| FX | anything | anything | >5% |

**Nothing should have less than about -55 dB above 1.5 kHz.** A patch with
nothing up there is not "dark", it is muffled — there is no air, no consonant,
nothing for the ear to locate.

## Category definitions

Each is a promise to the user about how the patch plays.

- **Pad** — slow attack (0.3–5 s), long release, sustains indefinitely. Movement
  from Tide/Current/Drift, not from the envelope.
- **Key** — fast attack (<30 ms), decays over 1–4 s, sustains while held.
  Playable chords. Definition in the 400–2000 Hz region.
- **Pluck** — fast attack, short decay (<1.2 s), low or zero sustain. The mod
  envelope does the filter work. No note should ring past release.
- **Bass** — below C3 territory, mono-ish, strong fundamental, tight envelope,
  high key tracking so it stays even up the neck. Keep the stereo narrow.
- **Lead** — cuts through: present in 1–4 kHz, some glide or vibrato routing,
  usually mono or narrow.
- **Arp** — short, rhythmic, tempo-synced Tide or Delay. Needs a clean transient
  and enough decay contrast to articulate.
- **Texture** — evolving, no clear pitch centre required, heavy modulation.
- **Drone** — static or extremely slow, sustains forever, designed to be held.
- **FX** — risers, impacts, sweeps, non-musical. Free of the balance rules.

## Tags

Only these: `Deep` `Wet` `Dark` `Bright` `Dreamy` `Glassy` `Organic` `Chaotic`
`Calm` `Cinematic` `Submerged` `Surface`. Two to four per preset. Tags must be
true — `Bright` on a patch with nothing above 1 kHz is a broken search result.

## The dry aquatic requirement

A preset marked `Dry::Yes` must head its overrides with `RIPPLES_FX_BYPASSED`
and still sound like water. Its character has to come from somewhere real:

- unison spread and oscillator panning for width
- `Current` / `Drift` at different rates on different destinations for motion
- the droplet engine for surface detail
- the resonator for reflections and body
- noise colour for the medium itself

Objectively it needs **stereo decorrelation above 0.10, or spectral motion
above 4%**. A dry preset that is mono and static has failed.

## Writing a preset that is actually designed

A preset is not five overrides on top of the INIT patch. The bank standard is
**60–120 explicit parameter choices and 5–12 modulation slots**, every one of
them a decision you could defend. What makes `ABYSS BASS` different from
`OCEAN FLOOR` is not a different cutoff — it is that one is mono, key-tracked
and articulated while the other has a five second attack and uses the resonator
as its room.

Before writing, decide: *what does this patch do that no other preset in the
bank does?* If you cannot answer, you are writing a duplicate.

## Verification loop

```bash
# Static check — no build needed, run this constantly
tools/lint-presets.py Source/Presets/Banks/BankYours.cpp

# Once the bank compiles, render and measure
cmake --build build --target RIPPLES_Render -j4
./build/RIPPLES_Render_artefacts/Release/RIPPLES_Render \
    --preset N --note 48 --seconds 3 --out /tmp/p.wav
python3 tools/analyse.py /tmp/p.wav
```

`--dry` bypasses every global effect (use it on your `Dry::Yes` presets),
`--zero-macros` isolates a preset from the macro layer, and
`--set <id>=<normalised>` overrides one parameter without editing the bank.
