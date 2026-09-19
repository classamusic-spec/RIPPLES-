#pragma once

/*
    RIPPLES — preset authoring helpers.

    Everything a bank file needs to express a preset in natural units. These
    were factory-internal while the bank was one file; they live here now so
    each bank can be written and reviewed on its own.

    Every value in a preset is NORMALISED (0..1) — the position of the control,
    not the Hz or the seconds behind it. That is what lets a preset survive a
    later change to a parameter's range or skew. Writing raw 0..1 numbers by
    hand would be unreadable, so every value goes through a named helper:
    hz(820.0f), dec(1.8f), lvl(-6.0f), semi(-12), wv(OscWave::Water). The
    helpers are the ONLY place a natural unit becomes a normalised one, and
    they mirror ParameterLayout.cpp exactly — same bounds, same skew centres.
*/

#include "Presets/PresetManager.h"
#include "Presets/FactoryPresets.h"
#include "Parameters/ParameterIDs.h"
#include "Parameters/ParameterEnums.h"
#include "Parameters/ParameterLayout.h"
#include "Utilities/DSPConstants.h"

#include <cmath>
#include <map>
#include <utility>
#include <vector>

namespace ripples::factory
{


constexpr float on  = 1.0f;
constexpr float off = 0.0f;

inline float clamp01 (float v) noexcept { return juce::jlimit (0.0f, 1.0f, v); }

//==============================================================================
// Range mirrors — see the note above. lo/hi/centre match ParameterLayout.cpp.

/** juce::NormalisableRange (lo, hi) with no skew. */
inline float linNorm (float value, float lo, float hi)
{
    return clamp01 ((value - lo) / (hi - lo));
}

/** juce::NormalisableRange (lo, hi) after setSkewForCentre (centre). */
inline float skewNorm (float value, float lo, float hi, float centre)
{
    const float proportion = clamp01 ((value - lo) / (hi - lo));

    if (proportion <= 0.0f)
        return 0.0f;

    const float skew = std::log (0.5f) / std::log ((centre - lo) / (hi - lo));
    return clamp01 (std::pow (proportion, skew));
}

//==============================================================================
// Choice helpers. Every one funnels through choiceToNormalised with the enum's
// own NumXxx sentinel, so a preset can never silently select the wrong choice.
inline float wv  (OscWave w)         { return choiceToNormalised ((int) w, (int) OscWave::NumWaves); }
inline float im  (InteractionMode m) { return choiceToNormalised ((int) m, (int) InteractionMode::NumModes); }
inline float sw  (SubWave w)         { return choiceToNormalised ((int) w, (int) SubWave::NumWaves); }
inline float nz  (NoiseType t)       { return choiceToNormalised ((int) t, (int) NoiseType::NumTypes); }
inline float fm  (FilterMode m)      { return choiceToNormalised ((int) m, (int) FilterMode::NumModes); }
inline float ts  (TideShape s)       { return choiceToNormalised ((int) s, (int) TideShape::NumShapes); }
inline float dm  (DropletMode m)     { return choiceToNormalised ((int) m, (int) DropletMode::NumModes); }
inline float rtg (RippleTrigger t)   { return choiceToNormalised ((int) t, (int) RippleTrigger::NumTriggers); }
inline float gmd (GlideMode m)       { return choiceToNormalised ((int) m, (int) GlideMode::NumModes); }
inline float sd  (SyncDivision d)    { return choiceToNormalised ((int) d, (int) SyncDivision::NumDivisions); }

//==============================================================================
// Levels — oscillators, sub and noise are dB parameters, NOT percentages.
// kMinLevelDb reads as silence.
constexpr float kSilentDb = kMinLevelDb;

inline float lvl (float db) { return skewNorm (db, kMinLevelDb, kMaxLevelDb, -12.0f); }

//==============================================================================
// Frequencies.
inline float hz      (float f) { return skewNorm (f, dsp::kMinCutoffHz, dsp::kMaxCutoffHz, 1000.0f); }
inline float vLowHz  (float f) { return skewNorm (f, 20.0f, 1000.0f, 150.0f); }
inline float vHighHz (float f) { return skewNorm (f, 1000.0f, dsp::kMaxCutoffHz, 6000.0f); }

// Modulator / effect rates — each has its own span in the layout.
inline float tideHz   (float f) { return skewNorm (f, 0.01f,  40.0f, 2.0f); }
inline float currHz   (float f) { return skewNorm (f, 0.01f,  20.0f, 1.0f); }
inline float driftHz  (float f) { return skewNorm (f, 0.001f,  2.0f, 0.05f); }
inline float rippleHz (float f) { return skewNorm (f, 0.1f,   50.0f, 4.0f); }
inline float scurHz   (float f) { return skewNorm (f, 0.005f,  5.0f, 0.15f); }
inline float chorHz   (float f) { return skewNorm (f, 0.01f,  10.0f, 0.5f); }

//==============================================================================
// Times.
inline float att   (float s)  { return skewNorm (s, dsp::kMinEnvTimeSec, dsp::kMaxAttackSec,  0.15f); }
inline float dec   (float s)  { return skewNorm (s, dsp::kMinEnvTimeSec, dsp::kMaxDecaySec,   0.50f); }
inline float rel   (float s)  { return skewNorm (s, dsp::kMinEnvTimeSec, dsp::kMaxReleaseSec, 0.60f); }
inline float dtime (float s)  { return skewNorm (s, 0.001f, dsp::kMaxDelaySeconds, 0.35f); }
inline float gtime (float s)  { return skewNorm (s, 0.0f, 2.0f, 0.15f); }
inline float chdel (float ms) { return skewNorm (ms, 1.0f, 50.0f, 12.0f); }
inline float pre   (float ms) { return skewNorm (ms, 0.0f, dsp::kMaxPredelaySeconds * 1000.0f, 40.0f); }

//==============================================================================
// Pitch, counts and bipolar controls.

/** A -1..1 control (pan, fluid field, mod amount, filter env amount) as 0..1. */
inline float bip (float value) { return linNorm (value, -1.0f, 1.0f); }

inline float oct    (int octaves)   { return linNorm ((float) octaves, -3.0f, 3.0f); }
inline float semi   (int semitones) { return linNorm ((float) semitones, -12.0f, 12.0f); }
inline float cents  (float c)       { return linNorm (c, -100.0f, 100.0f); }
inline float uni    (int voices)    { return linNorm ((float) voices, 1.0f, (float) dsp::kMaxUnison); }
inline float vox    (int voices)    { return linNorm ((float) voices, 1.0f, (float) dsp::kMaxVoices); }
inline float bend   (int semitones) { return linNorm ((float) semitones, 0.0f, 24.0f); }
inline float subOct (int octave)    { return linNorm ((float) octave, -2.0f, -1.0f); }
inline float cycles (float n)       { return skewNorm (n, 1.0f, 32.0f, 6.0f); }

/** Oscillator start phase: a negative value means free-running. */
inline float phaseFree()             { return linNorm (-1.0f, -1.0f, 1.0f); }
inline float startPhase (float p01)  { return linNorm (juce::jlimit (0.0f, 1.0f, p01), -1.0f, 1.0f); }

//==============================================================================
// Master section.
inline float eqdb   (float db) { return linNorm (db, -12.0f, 12.0f); }
inline float ceildb (float db) { return linNorm (db, -12.0f, 0.0f); }
inline float outdb  (float db) { return linNorm (db, kMinOutputDb, kMaxOutputDb); }

//==============================================================================
using PV = std::pair<const char*, float>;

struct ModSpec
{
    ModSource src;
    ModDest   dst;
    float     amount;          // -1..1
    bool      bipolar = false; // treat a unipolar source as -1..1
};

enum class Dry { No, Yes };

//==============================================================================
/** The INIT patch, complete: every parameter in ParameterIDs.h has a value.

    These are deliberately the same values as the defaults in
    ParameterLayout.cpp, so INIT DEEP SAW and "the plug-in with nothing loaded"
    are the same sound — two detuned saws, a quiet sine sub, an LP24 ladder
    under the mod envelope, and every water extra parked at zero.

    Every factory preset starts from this and overrides what it cares about, so
    each preset carries a FULL parameter map: no preset can inherit a stray
    setting from whatever was loaded before it, and the source stays readable
    because only the deliberate choices are written out. */
inline std::map<juce::String, float> initValues()
{
    std::map<juce::String, float> v
    {
        // --- OSC A "TIDE" ----------------------------------------------------
        { pid::oscAWave,      wv (OscWave::Saw) },
        { pid::oscAShape,     0.50f },
        { pid::oscAOctave,    oct (0) },
        { pid::oscASemitone,  semi (0) },
        { pid::oscAFine,      cents (0.0f) },
        { pid::oscAPhase,     phaseFree() },
        { pid::oscAUnison,    uni (1) },
        { pid::oscADetune,    0.25f },
        { pid::oscAStereo,    0.50f },
        { pid::oscAPan,       bip (0.0f) },
        { pid::oscALevel,     lvl (0.0f) },

        // --- OSC B "CURRENT" -------------------------------------------------
        { pid::oscBWave,      wv (OscWave::Saw) },
        { pid::oscBShape,     0.50f },
        { pid::oscBOctave,    oct (0) },
        { pid::oscBSemitone,  semi (0) },
        { pid::oscBFine,      cents (7.0f) },
        { pid::oscBPhase,     phaseFree() },
        { pid::oscBUnison,    uni (1) },
        { pid::oscBDetune,    0.25f },
        { pid::oscBStereo,    0.50f },
        { pid::oscBPan,       bip (0.0f) },
        { pid::oscBLevel,     lvl (-1.5f) },
        { pid::oscBInterMode, im (InteractionMode::Normal) },
        { pid::oscBInterAmt,  0.50f },

        // --- SUB / NOISE -----------------------------------------------------
        { pid::subWave,       sw (SubWave::Sine) },
        { pid::subOctave,     subOct (-1) },
        { pid::subLevel,      lvl (-12.0f) },
        { pid::noiseType,     nz (NoiseType::White) },
        { pid::noiseLevel,    lvl (kSilentDb) },
        { pid::noiseTone,     0.50f },

        // --- DEPTH FILTER ----------------------------------------------------
        { pid::filtMode,      fm (FilterMode::LP24) },
        { pid::filtCutoff,    hz (1200.0f) },
        { pid::filtReso,      0.15f },
        { pid::filtDrive,     0.12f },
        { pid::filtKeyTrack,  0.50f },
        { pid::filtEnvAmt,    bip (0.35f) },
        { pid::filtMovement,  0.50f },
        { pid::filtPressure,  0.00f },

        // --- ENVELOPES -------------------------------------------------------
        { pid::fenvAttack,    att (0.002f) },
        { pid::fenvDecay,     dec (0.60f) },
        { pid::fenvSustain,   0.30f },
        { pid::fenvRelease,   rel (0.40f) },
        { pid::fenvVelocity,  0.30f },

        { pid::aenvAttack,    att (0.005f) },
        { pid::aenvDecay,     dec (1.00f) },
        { pid::aenvSustain,   0.80f },
        { pid::aenvRelease,   rel (0.35f) },
        { pid::aenvVelocity,  0.50f },

        // --- MACROS ----------------------------------------------------------
        { pid::macroDepth,    0.00f },
        { pid::macroWet,      0.00f },
        { pid::macroRipple,   0.00f },
        { pid::macroCurrent,  0.00f },
        { pid::macroDrops,    0.00f },
        { pid::macroPressure, 0.00f },
        { pid::macroSpace,    0.00f },
        { pid::macroGlow,     0.00f },

        // --- FLUID FIELD -----------------------------------------------------
        { pid::fluidX,        bip (0.0f) },
        { pid::fluidY,        bip (0.0f) },
        { pid::fluidXAmount,  0.50f },
        { pid::fluidYAmount,  0.50f },

        // --- TIDE LFO --------------------------------------------------------
        { pid::tideShape,     ts (TideShape::Sine) },
        { pid::tideRate,      tideHz (1.0f) },
        { pid::tideSync,      off },
        { pid::tideSyncRate,  sd (SyncDivision::Quarter) },
        { pid::tidePhase,     0.00f },
        { pid::tideDepth,     0.50f },
        { pid::tideStereo,    0.00f },

        // --- CURRENT ---------------------------------------------------------
        { pid::currRate,      currHz (0.5f) },
        { pid::currAmount,    0.35f },
        { pid::currSmooth,    0.50f },
        { pid::currDrift,     0.20f },
        { pid::currStereo,    0.50f },

        // --- DRIFT -----------------------------------------------------------
        { pid::driftRate,     driftHz (0.05f) },
        { pid::driftAmount,   0.30f },
        { pid::driftStereo,   0.50f },

        // --- RIPPLE ----------------------------------------------------------
        { pid::rippleRate,     rippleHz (4.0f) },
        { pid::rippleDecay,    0.50f },
        { pid::rippleDepth,    0.50f },
        { pid::rippleCycles,   cycles (4.0f) },
        { pid::rippleSpread,   0.00f },
        { pid::ripplePolarity, off },
        { pid::rippleTrigger,  rtg (RippleTrigger::NoteOn) },

        // --- DROPLETS --------------------------------------------------------
        { pid::dropAmount,    0.00f },
        { pid::dropDensity,   0.30f },
        { pid::dropSize,      0.50f },
        { pid::dropTone,      0.50f },
        { pid::dropSplash,    0.30f },
        { pid::dropGravity,   0.50f },
        { pid::dropBounce,    0.30f },
        { pid::dropRandom,    0.50f },
        { pid::dropSpread,    0.50f },
        { pid::dropMode,      dm (DropletMode::Atmospheric) },

        // --- RESONATOR -------------------------------------------------------
        { pid::resoAmount,    0.00f },
        { pid::resoSize,      0.50f },
        { pid::resoDecay,     0.50f },
        { pid::resoDamping,   0.50f },
        { pid::resoScatter,   0.30f },
        { pid::resoMotion,    0.20f },

        // --- STEREO CURRENT --------------------------------------------------
        { pid::scurEnable,    off },
        { pid::scurAmount,    0.30f },
        { pid::scurRate,      scurHz (0.10f) },
        { pid::scurWidth,     0.50f },

        // --- LIQUID CHORUS ---------------------------------------------------
        { pid::chorEnable,    off },
        { pid::chorRate,      chorHz (0.40f) },
        { pid::chorDepth,     0.40f },
        { pid::chorDelay,     chdel (12.0f) },
        { pid::chorFeedback,  0.15f },
        { pid::chorWidth,     0.60f },
        { pid::chorMix,       0.35f },

        // --- LIQUID DELAY ----------------------------------------------------
        { pid::dlyEnable,     off },
        { pid::dlyTime,       dtime (0.40f) },
        { pid::dlySync,       off },
        { pid::dlySyncTime,   sd (SyncDivision::Eighth) },
        { pid::dlyFeedback,   0.40f },
        { pid::dlyMotion,     0.30f },
        { pid::dlySpread,     0.40f },
        { pid::dlyDamping,    0.50f },
        { pid::dlyDiffusion,  0.30f },
        { pid::dlyMix,        0.30f },

        // --- DIFFUSION -------------------------------------------------------
        { pid::diffEnable,    off },
        { pid::diffAmount,    0.40f },
        { pid::diffSize,      0.50f },
        { pid::diffDamping,   0.40f },
        { pid::diffMix,       0.30f },

        // --- ABYSS REVERB ----------------------------------------------------
        { pid::verbEnable,    off },
        { pid::verbSize,      0.60f },
        { pid::verbDecay,     0.60f },
        { pid::verbPredelay,  pre (20.0f) },
        { pid::verbDamping,   0.50f },
        { pid::verbLowCut,    vLowHz (120.0f) },
        { pid::verbHighCut,   vHighHz (9000.0f) },
        { pid::verbMod,       0.30f },
        { pid::verbMix,       0.30f },

        // --- MASTER ----------------------------------------------------------
        { pid::mastLow,       eqdb (0.0f) },
        { pid::mastMid,       eqdb (0.0f) },
        { pid::mastHigh,      eqdb (0.0f) },
        { pid::mastDrive,     0.00f },
        { pid::mastCeiling,   ceildb (-0.3f) },
        { pid::mastOutput,    outdb (0.0f) },

        // --- GLOBAL / VOICE --------------------------------------------------
        { pid::glideTime,     gtime (0.0f) },
        { pid::glideMode,     gmd (GlideMode::Off) },
        { pid::voiceCount,    vox (dsp::kMaxVoices) },
        { pid::bendRange,     bend (2) },
        { pid::masterTune,    linNorm (0.0f, -100.0f, 100.0f) },
    };

    // All twelve matrix slots start empty.
    for (int slot = 0; slot < pid::kNumModSlots; ++slot)
    {
        v[pid::modSourceID (slot)]  = choiceToNormalised ((int) ModSource::None, (int) ModSource::NumSources);
        v[pid::modDestID (slot)]    = choiceToNormalised ((int) ModDest::None,   (int) ModDest::NumDests);
        v[pid::modAmountID (slot)]  = bip (0.0f);
        v[pid::modBipolarID (slot)] = off;
    }

    return v;
}

//==============================================================================
/** Builds one preset: INIT baseline, then the deliberate overrides, then the
    matrix slots in list order. */
inline Preset make (const char* bank,
             const char* name,
             const char* category,
             std::initializer_list<const char*> tags,
             Dry dry,
             const char* description,
             std::initializer_list<PV> overrides,
             std::initializer_list<ModSpec> mods = {})
{
    Preset p;

    p.info.name             = name;
    p.info.author           = "Classa Music";
    p.info.category         = category;
    p.info.bank             = bank;
    p.info.description      = description;
    p.info.isFactory        = true;
    p.info.dryAquatic       = (dry == Dry::Yes);
    p.info.parameterVersion = pid::kParameterVersion;

    for (auto* t : tags)
        p.info.tags.add (t);

    p.values = initValues();

    for (const auto& o : overrides)
        p.values[o.first] = clamp01 (o.second);

    int slot = 0;

    for (const auto& m : mods)
    {
        if (slot >= pid::kNumModSlots)
            break;

        p.values[pid::modSourceID (slot)]  = choiceToNormalised ((int) m.src, (int) ModSource::NumSources);
        p.values[pid::modDestID (slot)]    = choiceToNormalised ((int) m.dst, (int) ModDest::NumDests);
        p.values[pid::modAmountID (slot)]  = bip (juce::jlimit (-1.0f, 1.0f, m.amount));
        p.values[pid::modBipolarID (slot)] = m.bipolar ? on : off;

        ++slot;
    }

    return p;
}

//==============================================================================
// Bank names, used both as the `bank` field and for display ordering.
constexpr const char* kSurface  = "THE SURFACE";
constexpr const char* kShallow  = "SHALLOW WATER";
constexpr const char* kDeepBlue = "DEEP BLUE";
constexpr const char* kAbyss    = "THE ABYSS";
constexpr const char* kDroplets = "DROPLETS";
constexpr const char* kCurrents = "CURRENTS";
constexpr const char* kBiolum   = "BIOLUMINESCENCE";
constexpr const char* kStorms   = "STORMS";

/** Every global effect explicitly out of the way. This heads every Dry::Yes
    preset, and it is deliberately stricter than "all effects bypassed": even
    STEREO CURRENT is off, so the stereo image of those patches comes from
    unison spread, oscillator panning, decorrelated modulators, droplet spread
    and resonator scatter rather than from a width effect. */
#define RIPPLES_FX_BYPASSED                               \
    { pid::scurEnable, off }, { pid::scurAmount, 0.00f }, \
    { pid::chorEnable, off }, { pid::chorMix,    0.00f }, \
    { pid::dlyEnable,  off }, { pid::dlyMix,     0.00f }, \
    { pid::diffEnable, off }, { pid::diffMix,    0.00f }, \
    { pid::verbEnable, off }, { pid::verbMix,    0.00f }


} // namespace ripples::factory
