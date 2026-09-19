# RIPPLES — Module API Contract

This document is **binding**. Every module is written to the signatures below so
that work done in parallel links together without renegotiation. If you believe a
signature is wrong, implement it as written and note the objection in your
report — do not change it unilaterally, because other modules are already
compiling against it.

## Ground rules

1. **Namespace.** Everything lives in `namespace ripples`.
2. **Includes are relative to `Source/`.** Write `#include "Utilities/MathUtils.h"`,
   never a relative `../../` path.
3. **One header + one .cpp per class**, in your assigned directory. Never create
   or edit a file outside the directory you were assigned.
4. **Realtime safety is absolute.** Inside any `process*` / `advance` method:
   no allocation, no locks, no file or JSON/XML access, no logging, no JUCE
   `Component` calls, no `std::function` construction, no `juce::String`
   construction. Allocate in `prepare()`; only ever reuse afterwards.
5. **`prepare()` may allocate.** It is called from the message thread.
6. **Denormals and NaN.** Feedback paths use `ripples::math::antiDenormal` and/or
   `sanitise`. A module must not emit NaN or Inf for any parameter combination.
7. **Each module owns its own `Params` struct** in natural units (Hz, seconds,
   linear gain, normalised 0..1 or -1..1). The plug-in core maps the APVTS onto
   these. Do **not** read the APVTS from inside a DSP module.
8. **C++20.** Prefer `noexcept` on realtime methods. No exceptions on the audio path.
9. **Verify before reporting done:**
   `tools/syntax-check.sh <each .cpp you wrote>` — must print `ok` for all.

## Sound-design intent

RIPPLES must already sound aquatic **with every global effect bypassed**. Water
character belongs in the oscillators, noise, filter, resonator, droplets and
modulators — not in the reverb. A module that only sounds "watery" once reverb
is added has missed the brief.

---

# DSP module APIs

All headers below are the exact public interface. You may add private members
and extra public helpers, but you must not change or remove what is listed.

## Oscillators — `Source/DSP/Oscillators/`

```cpp
// BandLimitedOscillator.h
#include "Parameters/ParameterEnums.h"
#include "Utilities/RandomGenerator.h"

class BandLimitedOscillator
{
public:
    struct Params
    {
        OscWave wave   = OscWave::Saw;
        float shape    = 0.5f;   // 0..1 — PWM / morph amount, meaning varies per wave
        int   unison   = 1;      // 1..7
        float detune   = 0.0f;   // 0..1
        float stereo   = 0.5f;   // 0..1 — unison stereo spread
        float startPhase = 0.0f; // 0..1, negative means free-running
    };

    void prepare (double sampleRate);
    void reset (RandomGenerator& rng);            // retrigger; honours startPhase
    void setParams (const Params& p) noexcept;
    void setFrequency (float hz) noexcept;

    /** Renders one stereo sample.
        @param phaseMod  added to phase, in 0..1 cycle units (phase modulation)
        @param fmRatio   multiplies frequency this sample (linear FM), 1.0 = none
        @returns true if the centre phase wrapped this sample (hard-sync master) */
    bool processSample (float& outL, float& outR,
                        float phaseMod = 0.0f, float fmRatio = 1.0f) noexcept;

    /** Forces all unison phases back to start — hard-sync slave. */
    void hardSync() noexcept;
};
```

Band-limit Saw / Square / Pulse / Shark with polyBLEP (`math::polyBlep`), and the
Triangle corners with `math::polyBlamp`. Aliasing on a held high note is a defect.
`Hollow`, `Glass` and `Water` are the aquatic waves and should be built from
summed band-limited partials so they stay clean at the top of the keyboard.

```cpp
// SubOscillator.h
class SubOscillator
{
public:
    struct Params { SubWave wave = SubWave::Sine; int octave = -1; };  // octave: -1 or -2

    void prepare (double sampleRate);
    void reset() noexcept;
    void setParams (const Params& p) noexcept;
    void setFrequency (float baseHz) noexcept;   // the *note* frequency; octave applied inside
    float processSample() noexcept;
};

// WaterNoise.h
class WaterNoise
{
public:
    struct Params { NoiseType type = NoiseType::White; float tone = 0.5f; };  // tone 0..1

    void prepare (double sampleRate, uint32_t seed);
    void reset() noexcept;
    void setParams (const Params& p) noexcept;
    void processSample (float& outL, float& outR) noexcept;
};
```

## Filter and envelopes — `Source/DSP/Filters/`, `Source/DSP/Modulation/Envelope.*`

```cpp
// DepthFilter.h
class DepthFilter
{
public:
    struct Params
    {
        FilterMode mode  = FilterMode::LP24;
        float cutoffHz   = 1000.0f;
        float resonance  = 0.2f;   // 0..1, self-oscillating near 1
        float drive      = 0.0f;   // 0..1 pre-filter drive
        float movement   = 0.5f;   // 0..1 morph position (Morph mode)
        float pressure   = 0.0f;   // 0..1 density / low-mid weight
    };

    void prepare (double sampleRate);
    void reset() noexcept;
    void setParams (const Params& p) noexcept;
    void setCutoff (float hz) noexcept;          // per-sample modulated cutoff
    void processSample (float& l, float& r) noexcept;
};
```

Must be a **musically usable** subtractive filter first: clean plucks, round
basses, smooth pads, singing resonance. A zero-delay-feedback ladder or SVF is
expected. It must be stable at every cutoff/resonance/drive combination and at
44.1–192 kHz.

```cpp
// Envelope.h  (used for both the amp and the mod/filter envelope)
class Envelope
{
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };
    struct Params { float attack = 0.01f, decay = 0.3f, sustain = 0.7f, release = 0.5f; };  // seconds, sustain 0..1

    void prepare (double sampleRate);
    void setParams (const Params& p) noexcept;
    void noteOn() noexcept;
    void noteOff() noexcept;
    void reset() noexcept;                 // hard reset to Idle
    float processSample() noexcept;        // returns 0..1
    bool isActive() const noexcept;        // false once fully released
    Stage getStage() const noexcept;
    float getCurrentValue() const noexcept;
};
```

Envelopes must be exponential (musical), click-free, and must handle retrigger
from any stage without a discontinuity.

## Modulators — `Source/DSP/Modulation/`

All modulators run at **control rate**: `advance(numSamples)` steps the modulator
forward by that many samples and returns the new value. Voices call this once per
modulation block (`dsp::kModBlockSize`).

```cpp
// TideLFO.h  — smooth deterministic LFO, output -1..1
class TideLFO
{
public:
    struct Params
    {
        TideShape shape   = TideShape::Sine;
        float rateHz      = 1.0f;
        float phaseOffset = 0.0f;   // 0..1
        float stereoPhase = 0.0f;   // 0..1 offset for the right channel value
    };

    void prepare (double sampleRate);
    void reset (float phase01 = 0.0f) noexcept;
    void setParams (const Params& p) noexcept;
    float advance (int numSamples) noexcept;   // returns new value, -1..1
    float getValue() const noexcept;
    float getValueR() const noexcept;          // stereo-offset value
};

// CurrentModulator.h — smooth correlated randomness. NEVER sample-and-hold.
class CurrentModulator
{
public:
    struct Params
    {
        float rate       = 0.5f;   // Hz-ish, how fast it wanders
        float smoothness = 0.5f;   // 0..1, damping
        float drift      = 0.0f;   // 0..1, slow bias wander
        float stereo     = 0.5f;   // 0..1 decorrelation between L/R values
    };

    void prepare (double sampleRate, uint32_t seed);
    void reset() noexcept;
    void setParams (const Params& p) noexcept;
    float advance (int numSamples) noexcept;   // -1..1, continuous, no steps
    float getValue() const noexcept;
    float getValueR() const noexcept;
};

// DriftModulator.h — extremely slow random motion for pads and drones
class DriftModulator
{
public:
    struct Params { float rate = 0.05f; float stereo = 0.5f; };

    void prepare (double sampleRate, uint32_t seed);
    void reset() noexcept;
    void setParams (const Params& p) noexcept;
    float advance (int numSamples) noexcept;   // -1..1
    float getValue() const noexcept;
    float getValueR() const noexcept;
};

// RippleModulator.h — triggered damped wave: sin(2*pi*f*t) * exp(-decay*t)
class RippleModulator
{
public:
    struct Params
    {
        float rateHz = 4.0f;
        float decay  = 0.5f;   // 0..1, higher = faster decay
        float cycles = 4.0f;   // how many cycles before it is considered finished
        float spread = 0.0f;   // 0..1 stereo phase offset
        bool  invert = false;  // polarity
    };

    void prepare (double sampleRate);
    void reset() noexcept;
    void setParams (const Params& p) noexcept;
    void trigger (float intensity) noexcept;   // intensity 0..1 scales amplitude
    float advance (int numSamples) noexcept;   // -1..1
    float getValue() const noexcept;
    bool isActive() const noexcept;
};
```

```cpp
// ModulationMatrix.h
/** Current value of every modulation source, indexed by (int) ModSource.
    Unipolar sources (Velocity, ModWheel, envelopes) sit in 0..1;
    bipolar sources (Tide, Current, Drift, Ripple) sit in -1..1. */
struct ModSourceValues
{
    float values[(size_t) ModSource::NumSources] {};

    float operator[] (ModSource s) const noexcept { return values[(size_t) s]; }
    void set (ModSource s, float v) noexcept { values[(size_t) s] = v; }
};

class ModulationMatrix
{
public:
    struct Slot
    {
        ModSource source = ModSource::None;
        ModDest   dest   = ModDest::None;
        float     amount = 0.0f;    // -1..1
        bool      bipolar = false;  // treat a unipolar source as -1..1
    };

    void setSlot (int index, const Slot& slot) noexcept;   // index 0..kNumModSlots-1
    Slot getSlot (int index) const noexcept;
    void clear() noexcept;

    /** Accumulates every active slot into destOffsets, which must have
        (size_t) ModDest::NumDests elements. Clears it first. */
    void process (const ModSourceValues& sources, float* destOffsets) const noexcept;
};
```

## Droplets and resonator — `Source/DSP/Exciters/`, `Source/DSP/Resonators/`

```cpp
// DropletEngine.h
class DropletEngine
{
public:
    struct Params
    {
        float amount = 0.0f, density = 0.3f, size = 0.5f, tone = 0.5f,
              splash = 0.3f, gravity = 0.5f, bounce = 0.3f,
              random = 0.5f, spread = 0.5f;     // all 0..1
        DropletMode mode = DropletMode::Atmospheric;
    };

    void prepare (double sampleRate, uint32_t seed);
    void reset() noexcept;
    void setParams (const Params& p) noexcept;
    void trigger (float noteHz, float velocity) noexcept;   // NOTE mode / note-on
    void processSample (float& outL, float& outR) noexcept;

    /** Pops one pending droplet event for the UI. Returns false when none left. */
    bool consumeDropletEvent (float& intensity, float& pan) noexcept;
};
```

Each droplet is roughly: short excitation (impulse or noise burst) → resonance →
short envelope → optional pitch trajectory → pan. `gravity` and `bounce` should
produce the accelerating repeat of a real drip. Cap concurrent droplets at
`dsp::kMaxDroplets`; allocate that pool in `prepare()`.

```cpp
// WaterResonator.h — parallel resonator bank placed after the filter
class WaterResonator
{
public:
    struct Params
    {
        float amount = 0.0f, size = 0.5f, decay = 0.5f,
              damping = 0.5f, scatter = 0.3f, motion = 0.2f;   // all 0..1
    };

    void prepare (double sampleRate, uint32_t seed);
    void reset() noexcept;
    void setParams (const Params& p) noexcept;
    void setBaseFrequency (float hz) noexcept;   // tracks the played note
    void processSample (float& l, float& r) noexcept;
};
```

Target timbres: bubbles, glass, bowls, submerged bells, metallic reflections,
cavern resonance. `dsp::kNumResonatorBanks` parallel resonators. **Must not blow
up** at maximum decay with maximum input — clamp feedback and saturate.

## Global effects — `Source/DSP/Effects/`

Every effect is stereo and processes a `juce::AudioBuffer<float>` in place.

```cpp
class StereoCurrent   // slow fluid stereo motion
{
public:
    struct Params { bool enabled = true; float amount = 0.3f, rate = 0.1f, width = 0.5f; };
    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;
    void setParams (const Params& p) noexcept;
    void process (juce::AudioBuffer<float>& buffer) noexcept;
};

class LiquidChorus
{
public:
    struct Params { bool enabled = true; float rate = 0.4f, depth = 0.4f, delayMs = 12.0f,
                                                feedback = 0.15f, width = 0.6f, mix = 0.35f; };
    /* prepare / reset / setParams / process as above */
};

class LiquidDelay
{
public:
    struct Params { bool enabled = true; float timeSeconds = 0.4f, feedback = 0.4f,
                                                motion = 0.3f, spread = 0.4f, damping = 0.5f,
                                                diffusion = 0.3f, mix = 0.3f; };
    /* prepare / reset / setParams / process as above */
};

class DiffusionNetwork   // short all-pass network: blur and mist
{
public:
    struct Params { bool enabled = true; float amount = 0.4f, size = 0.5f, damping = 0.4f, mix = 0.3f; };
    /* prepare / reset / setParams / process as above */
};

class AbyssReverb
{
public:
    struct Params { bool enabled = true; float size = 0.6f, decay = 0.6f, predelayMs = 20.0f,
                                                damping = 0.5f, lowCutHz = 120.0f, highCutHz = 9000.0f,
                                                modulation = 0.3f, mix = 0.3f; };
    /* prepare / reset / setParams / process as above */
};

class MasterShaper   // final EQ / saturation / safety limiter
{
public:
    struct Params { float lowGainDb = 0.0f, midGainDb = 0.0f, highGainDb = 0.0f,
                          drive = 0.0f, ceilingDb = -0.3f, outputDb = 0.0f; };
    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;
    void setParams (const Params& p) noexcept;
    void process (juce::AudioBuffer<float>& buffer) noexcept;
    float getPeakL() const noexcept;    // post-limiter, for the output meter
    float getPeakR() const noexcept;
};
```

The limiter is a **safety** stage: transparent under normal levels, never
allowing the output past the ceiling, and never pumping audibly on pads.

---

# UI component APIs — `Source/UI/`

Views are written against these exact signatures. All colours, radii, spacing and
fonts come from `RippleTheme::get()`. **No Component may hard-code a
`juce::Colour` literal or a magic pixel constant** — add a token to
`RippleTheme` instead.

```cpp
// Components/GlassPanel.h
class GlassPanel : public juce::Component
{
public:
    GlassPanel();
    explicit GlassPanel (const juce::String& title);
    void setTitle (const juce::String& title);
    void setAccent (juce::Colour accent);
    void setContentInset (int inset);
    juce::Rectangle<int> getContentBounds() const;
};

// Components/RippleKnob.h
class RippleKnob : public juce::Component
{
public:
    enum class Size { Large, Medium, Small };
    explicit RippleKnob (const juce::String& label, Size size = Size::Medium);
    void attach (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID);
    void setAccent (juce::Colour accent);
    void setLabel (const juce::String& label);
    void setTooltipText (const juce::String& tip);
    void setModulationAmount (float amount);   // -1..1, draws the outer mod ring
    juce::Slider& getSlider() noexcept;
};

// Components/RippleSelector.h
class RippleSelector : public juce::Component
{
public:
    RippleSelector();
    void setItems (const juce::StringArray& items);
    void attach (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID);
    void setAccent (juce::Colour accent);
    juce::ComboBox& getComboBox() noexcept;
};

// Components/RippleToggle.h
class RippleToggle : public juce::Component
{
public:
    explicit RippleToggle (const juce::String& text = {});
    void attach (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID);
    void setAccent (juce::Colour accent);
    juce::ToggleButton& getButton() noexcept;
};

// Components/RippleButton.h
class RippleButton : public juce::TextButton
{
public:
    explicit RippleButton (const juce::String& text = {});
    void setAccent (juce::Colour accent);
    void setIsPrimary (bool shouldBePrimary);
};

// Components/SectionHeader.h
class SectionHeader : public juce::Component
{
public:
    explicit SectionHeader (const juce::String& title, const juce::String& subtitle = {});
    void setAccent (juce::Colour accent);
    void setTitle (const juce::String& title);
    void setSubtitle (const juce::String& subtitle);
};

// Components/WaveformView.h
class WaveformView : public juce::Component
{
public:
    WaveformView();
    void setWave (OscWave wave, float shape);
    void setAccent (juce::Colour accent);
    void setAnimated (bool shouldAnimate);
};

// Components/EnvelopeView.h
class EnvelopeView : public juce::Component
{
public:
    EnvelopeView();
    void setADSR (float attack, float decay, float sustain, float release);  // secs / 0..1
    void setAccent (juce::Colour accent);
    void setPlayhead (float normalisedPosition);   // <0 hides it
};

// Components/FilterResponseView.h
class FilterResponseView : public juce::Component
{
public:
    FilterResponseView();
    void setFilter (FilterMode mode, float cutoffHz, float resonance);
    void setAccent (juce::Colour accent);
};

// Components/ModulationMeter.h
class ModulationMeter : public juce::Component
{
public:
    ModulationMeter();
    void setValue (float bipolarValue);    // -1..1
    void setAccent (juce::Colour accent);
};

// Components/LFOView.h — draws the selected modulator's shape
class LFOView : public juce::Component
{
public:
    LFOView();
    void setShape (TideShape shape, float depth);
    void setPhase (float phase01);
    void setAccent (juce::Colour accent);
};
```

Knobs must support drag, fine adjustment with a modifier, double-click reset and
the mouse wheel, and must show their value as text. Construction: dark circular
body, subtle radial shading, thin outer ring, bright cyan value arc, small
indicator, mild inner highlight, mild shadow. Modern and tactile — not a 2004
emboss.

## Views — `Source/UI/Views/`

```cpp
class FluidField : public juce::Component
{
public:
    FluidField (juce::AudioProcessorValueTreeState& apvts, VisualizationState& vis);
    ~FluidField() override;
};
```

`FluidField` is the visual centrepiece and must be **entirely procedural** —
concentric ellipses, wave rings, gradients, radial light, subtle particles,
ripple pulses. No pre-rendered image. It owns its own timer, drags the centre
node against `pid::fluidX` / `pid::fluidY`, shows the SURFACE / DEPTH / CALM /
CHAOS labels, resets to default on double-click, and fine-adjusts with Shift.

---

# Realtime-safety checklist

Before reporting done, confirm for every `process*` / `advance` method you wrote:

- [ ] no `new`, `delete`, `malloc`, `std::vector::resize`, `push_back`
- [ ] no `juce::String`, `std::string`, `std::function` construction
- [ ] no locks, no `std::mutex`, no file or network access
- [ ] no `DBG` / `printf` / `std::cout`
- [ ] every buffer it touches was sized in `prepare()`
- [ ] feedback paths are denormal-guarded and cannot produce NaN or Inf
- [ ] output stays finite for every extreme parameter combination
