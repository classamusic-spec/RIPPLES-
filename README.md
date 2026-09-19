<div align="center">

# R I P P L E S

**DIVE INTO SOUND**

A premium polyphonic subtractive synthesizer built around water —
currents, pressure, droplets, reflections and the deep ocean.

</div>

---

## What it is

RIPPLES is a commercial-grade software synthesizer (VST3 / AU / Standalone) whose
core is a conventional, great-sounding subtractive engine — oscillators, mixer,
filter, amplifier — with an aquatic identity layered through every stage rather
than painted on at the end with reverb.

The design rule the whole project is held to: **RIPPLES must already sound
characteristically itself with every global effect bypassed.** Water lives in the
oscillators, the noise, the Depth filter, the modulators, the droplet exciter and
the resonator bank. The Abyss reverb is the room, not the instrument.

## Signal path

```
 TIDE osc A ─┐
 CURRENT B ──┤
 SUB ────────┼── LIQUID MIXER ── PRE-FILTER DRIVE ── DEPTH FILTER
 NOISE ──────┤                                            │
 DROPLETS ───┘                                   WATER RESONATOR
                                                          │
                                        AMP ENVELOPE ── PAN ── VOICE SUM
                                                                  │
   STEREO CURRENT ── SATURATION ── CHORUS ── LIQUID DELAY ─────────┘
        │
        └── DIFFUSION ── ABYSS REVERB ── MASTER EQ ── SAFETY LIMITER ── OUT
```

## Building

Requires a C++20 compiler and CMake 3.22+. JUCE 9.0.2 is fetched automatically,
or you can point at a local checkout.

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

To use a JUCE checkout you already have:

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DRIPPLES_JUCE_DIR=/path/to/JUCE
```

Artefacts land in `build/RIPPLES_artefacts/Release/` — `VST3/`, `Standalone/`,
and `AU/` on macOS.

### Linux build dependencies

```bash
sudo apt-get install -y libasound2-dev libx11-dev libxext-dev libxrandr-dev \
    libxinerama-dev libxcursor-dev libxcomposite-dev libxi-dev \
    libfreetype6-dev libfontconfig1-dev libgl1-mesa-dev
```

## Development tooling

| Command | What it does |
|---|---|
| `tools/syntax-check.sh <file.cpp>` | `-fsyntax-only` check of one file. Safe to run concurrently — it writes no objects, so it never contends with a running build. |
| `tools/analyse.py <file.wav>` | Measures level, DC, stereo width, spectral centroid and inharmonic (aliasing) energy of a render. |
| `build/.../RIPPLES_Render --preset N --out x.wav` | Renders a factory preset offline. `--dry` bypasses all global effects for the dry aquatic test. |

Verifying a patch end to end:

```bash
cmake --build build --target RIPPLES_Render -j
./build/RIPPLES_Render_artefacts/Release/RIPPLES_Render \
    --preset 0 --note 48 --seconds 4 --out /tmp/init_deep_saw.wav
python3 tools/analyse.py /tmp/init_deep_saw.wav --fundamental 130.81
```

## Architecture

```
Source/
  Core/          PluginProcessor, PluginEditor
  DSP/
    RippleSynth, RippleVoice, MacroEngine
    Oscillators/ BandLimitedOscillator, SubOscillator, WaterNoise
    Filters/     DepthFilter
    Exciters/    DropletEngine
    Resonators/  WaterResonator
    Modulation/  Envelope, TideLFO, CurrentModulator, DriftModulator,
                 RippleModulator, ModulationMatrix
    Effects/     StereoCurrent, LiquidChorus, LiquidDelay,
                 DiffusionNetwork, AbyssReverb, MasterShaper
  Parameters/    ParameterIDs, ParameterEnums, ParameterLayout, ParameterCache
  Presets/       PresetManager, PresetSerializer, FactoryPresets
  UI/
    Theme/       RippleTheme (design tokens), RippleLookAndFeel
    Components/  RippleKnob, GlassPanel, WaveformView, EnvelopeView, ...
    Views/       MainView, Header, OscillatorPanel, FluidField, MacroStrip,
                 SynthPage, ModulationPage, EffectsPage, PresetBrowser
  Utilities/     MathUtils, DSPConstants, RandomGenerator, VisualizationState
```

`docs/CONTRACT.md` holds the binding module API contract — every module's public
interface, and the realtime-safety rules all of them are held to.

### Realtime discipline

The audio thread never allocates, locks, touches the filesystem, parses anything,
or calls into the GUI. Parameters reach the DSP through `ParameterCache`, which
caches raw atomic pointers once and converts to natural units per block. State
flows back to the editor through `VisualizationState`, a lock-free structure the
audio thread only ever writes and the GUI only ever reads.

### Interface

Drawn entirely in code — `juce::Graphics`, `Path` and `ColourGradient`, no raster
skins or pre-rendered knob strips, so it stays crisp on Retina and at 125/150/200%
Windows scaling. The visual language is roughly 70% clean flat UI, 20%
glassmorphism and 10% tactile depth, with every colour, radius, spacing value and
font drawn from the `RippleTheme` token struct.

## The eight macros

`DEPTH` `WET` `RIPPLE` `CURRENT` `DROPS` `PRESSURE` `SPACE` `GLOW`

Depth and Pressure are deliberately different axes: Depth moves the listener from
the surface toward the ocean floor (brightness falls, damping rises, the image
narrows, movement slows, the sub grows), while Pressure is density (drive,
low-mid weight, resonance, saturation, resonator intensity). Neither is a
rebadged filter cutoff — see `Source/DSP/MacroEngine.h`.

## Licence

Copyright © Classa Music. All rights reserved.
