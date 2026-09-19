#pragma once

/*
    RIPPLES — Shared choice enumerations.

    Every juce::AudioParameterChoice in the plug-in draws its index space from
    one of the enums below. DSP switches on these; the UI labels them from the
    matching name tables. Order is part of the preset contract — append only,
    never reorder.
*/

#include <juce_core/juce_core.h>
#include <array>

namespace ripples
{

//==============================================================================
/** Oscillator waveform family, shared by TIDE (A) and CURRENT (B). */
enum class OscWave
{
    Sine = 0,
    Triangle,
    Saw,
    Square,
    Pulse,
    Shark,      // asymmetric folded saw — aggressive, sea-glass edge
    Hollow,     // odd-harmonic hollow tone, clarinet-like underwater
    Glass,      // bright inharmonic-leaning partials
    Water,      // soft morphing wave with slow internal motion
    NumWaves
};

inline constexpr std::array<const char*, (size_t) OscWave::NumWaves> oscWaveNames
{
    "Sine", "Triangle", "Saw", "Square", "Pulse", "Shark", "Hollow", "Glass", "Water"
};

//==============================================================================
/** How CURRENT (osc B) interacts with TIDE (osc A). */
enum class InteractionMode
{
    Normal = 0,
    HardSync,
    FM,
    PhaseMod,
    RingMod,
    Crossfade,
    NumModes
};

inline constexpr std::array<const char*, (size_t) InteractionMode::NumModes> interactionNames
{
    "Normal", "Hard Sync", "FM", "Phase Mod", "Ring Mod", "Crossfade"
};

//==============================================================================
/** Sub oscillator waveform. */
enum class SubWave { Sine = 0, Triangle, Square, NumWaves };

inline constexpr std::array<const char*, (size_t) SubWave::NumWaves> subWaveNames
{
    "Sine", "Triangle", "Square"
};

//==============================================================================
/** Water noise colour. */
enum class NoiseType
{
    White = 0,
    Pink,
    Deep,       // heavily low-passed, rumbling
    Surf,       // band-passed wash with slow amplitude motion
    Bubble,     // sparse resonant blips
    Air,        // high, fine hiss
    NumTypes
};

inline constexpr std::array<const char*, (size_t) NoiseType::NumTypes> noiseTypeNames
{
    "White", "Pink", "Deep", "Surf", "Bubble", "Air"
};

//==============================================================================
/** DEPTH filter response. */
enum class FilterMode
{
    LP12 = 0,
    LP24,
    BP12,
    HP12,
    Notch,
    Morph,      // continuously morphs LP -> BP -> HP with the Movement control
    NumModes
};

inline constexpr std::array<const char*, (size_t) FilterMode::NumModes> filterModeNames
{
    "LP12", "LP24", "BP12", "HP12", "Notch", "Morph"
};

//==============================================================================
/** TIDE LFO shape. */
enum class TideShape
{
    Sine = 0,
    Triangle,
    Swell,      // slow rise, fast fall — a wave breaking
    DoubleWave, // two crests per cycle
    Flow,       // smoothed asymmetric contour
    NumShapes
};

inline constexpr std::array<const char*, (size_t) TideShape::NumShapes> tideShapeNames
{
    "Sine", "Triangle", "Swell", "Double Wave", "Flow"
};

//==============================================================================
/** Droplet engine behaviour. */
enum class DropletMode { Atmospheric = 0, Note, NumModes };

inline constexpr std::array<const char*, (size_t) DropletMode::NumModes> dropletModeNames
{
    "Atmospheric", "Note"
};

//==============================================================================
/** What retriggers the RIPPLE modulator. */
enum class RippleTrigger { NoteOn = 0, NoteOff, Droplet, NumTriggers };

inline constexpr std::array<const char*, (size_t) RippleTrigger::NumTriggers> rippleTriggerNames
{
    "Note On", "Note Off", "Droplet"
};

//==============================================================================
/** Portamento behaviour. */
enum class GlideMode { Off = 0, Always, Legato, NumModes };

inline constexpr std::array<const char*, (size_t) GlideMode::NumModes> glideModeNames
{
    "Off", "Always", "Legato"
};

//==============================================================================
/** Tempo-sync divisions, shared by the TIDE LFO and LIQUID DELAY. */
enum class SyncDivision
{
    ThirtySecond = 0, SixteenthT, Sixteenth, SixteenthD,
    EighthT, Eighth, EighthD, QuarterT, Quarter, QuarterD,
    HalfT, Half, HalfD, Whole, TwoBars, FourBars,
    NumDivisions
};

inline constexpr std::array<const char*, (size_t) SyncDivision::NumDivisions> syncDivisionNames
{
    "1/32", "1/16T", "1/16", "1/16.", "1/8T", "1/8", "1/8.", "1/4T",
    "1/4", "1/4.", "1/2T", "1/2", "1/2.", "1/1", "2 Bars", "4 Bars"
};

/** Length of each sync division in beats (quarter notes). */
inline constexpr std::array<double, (size_t) SyncDivision::NumDivisions> syncDivisionBeats
{
    0.125, 1.0 / 6.0, 0.25, 0.375, 1.0 / 3.0, 0.5, 0.75, 2.0 / 3.0,
    1.0, 1.5, 4.0 / 3.0, 2.0, 3.0, 4.0, 8.0, 16.0
};

//==============================================================================
/** Modulation matrix sources. Order is part of the preset contract. */
enum class ModSource
{
    None = 0,
    AmpEnvelope,
    ModEnvelope,
    Tide,
    Current,
    Drift,
    Ripple,
    Velocity,
    Note,
    KeyTrack,
    ModWheel,
    Aftertouch,
    RandomPerNote,
    FluidFieldX,
    FluidFieldY,
    NumSources
};

inline constexpr std::array<const char*, (size_t) ModSource::NumSources> modSourceNames
{
    "—", "Amp Env", "Mod Env", "Tide", "Current", "Drift", "Ripple",
    "Velocity", "Note", "Key Track", "Mod Wheel", "Aftertouch",
    "Random", "Fluid X", "Fluid Y"
};

//==============================================================================
/** Modulation matrix destinations. Order is part of the preset contract. */
enum class ModDest
{
    None = 0,
    // Pitch
    PitchAll,
    PitchA,
    PitchB,
    FineAll,
    // Oscillator shaping
    ShapeA,
    ShapeB,
    LevelA,
    LevelB,
    SubLevel,
    NoiseLevel,
    NoiseTone,
    OscMix,
    InteractionAmount,
    PanA,
    PanB,
    // Filter
    FilterCutoff,
    FilterResonance,
    FilterDrive,
    FilterEnvAmount,
    FilterMovement,
    // Amplitude
    Amplitude,
    // Resonator
    ResonatorAmount,
    ResonatorSize,
    ResonatorDecay,
    ResonatorDamping,
    ResonatorScatter,
    // Droplets
    DropletAmount,
    DropletDensity,
    DropletSize,
    DropletTone,
    // Global effects
    DelayMix,
    DelayFeedback,
    DelayMotion,
    ReverbMix,
    ReverbSize,
    DiffusionAmount,
    ChorusDepth,
    StereoWidth,
    NumDests
};

inline constexpr std::array<const char*, (size_t) ModDest::NumDests> modDestNames
{
    "—",
    "Pitch", "Pitch A", "Pitch B", "Fine Tune",
    "Shape A", "Shape B", "Level A", "Level B", "Sub Level",
    "Noise Level", "Noise Tone", "Osc Mix", "Interaction", "Pan A", "Pan B",
    "Filter Cutoff", "Resonance", "Filter Drive", "Filter Env", "Movement",
    "Amplitude",
    "Reso Amount", "Reso Size", "Reso Decay", "Reso Damping", "Reso Scatter",
    "Drop Amount", "Drop Density", "Drop Size", "Drop Tone",
    "Delay Mix", "Delay Feedback", "Delay Motion",
    "Reverb Mix", "Reverb Size", "Diffusion", "Chorus Depth", "Stereo Width"
};

//==============================================================================
/** True when a destination is applied per voice rather than globally. */
inline constexpr bool isVoiceLevelDest (ModDest d) noexcept
{
    return d > ModDest::None && d <= ModDest::DropletTone;
}

//==============================================================================
/** Helper: build a juce::StringArray from a name table. */
template <typename Table>
inline juce::StringArray toStringArray (const Table& table)
{
    juce::StringArray a;
    for (auto* n : table)
        a.add (juce::String::fromUTF8 (n));
    return a;
}

} // namespace ripples
