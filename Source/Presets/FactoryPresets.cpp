#include "Presets/FactoryPresets.h"
#include "Parameters/ParameterIDs.h"
#include "Parameters/ParameterEnums.h"
#include "Parameters/ParameterLayout.h"
#include "Utilities/DSPConstants.h"

#include <cmath>
#include <utility>

/*
    RIPPLES — the factory bank.

    ---------------------------------------------------------------------------
    HOW A PRESET IS EXPRESSED
    ---------------------------------------------------------------------------
    Every value below is NORMALISED (0..1) — the position of the control, not
    the Hz or the seconds behind it. That is what lets a preset survive a later
    change to a parameter's range or skew.

    Writing raw 0..1 numbers by hand would be unreadable and unreviewable, so
    every value goes through a named helper: hz(820.0f), dec(1.8f), lvl(-6.0f),
    semi(-12), wv(OscWave::Water). The helpers are the ONLY place a natural unit
    becomes a normalised one.

    The helpers mirror ParameterLayout.cpp EXACTLY — same bounds, same skew
    centres — by reimplementing juce::NormalisableRange::convertTo0to1:

        linear : norm = (v - lo) / (hi - lo)
        skewed : norm = ((v - lo) / (hi - lo)) ^ skew,
                 where setSkewForCentre gives skew = log(0.5) / log(r_centre)

    If a range in ParameterLayout.cpp ever moves, the matching helper here is
    the one place to follow it. Everything is monotonic, so even an unmirrored
    change only shifts absolute values, never the relative shape of the bank: a
    dark patch stays darker than a bright one.

    ---------------------------------------------------------------------------
    THE DRY AQUATIC TEST
    ---------------------------------------------------------------------------
    Presets marked `Dry::Yes` read as water with every global effect off. Their
    character comes from the waveforms, the noise colour, the Depth filter, the
    Current / Drift / Ripple modulators, the droplet engine and the resonator.
    Search this file for "DRY AQUATIC" to list them; there are fifteen.
*/

namespace ripples::factory
{

namespace
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
std::map<juce::String, float> initValues()
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
Preset make (const char* bank,
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

//==============================================================================
//  THE SURFACE — light on water. Bright, airy, the air/water boundary.
//==============================================================================
void addSurfaceBank (std::vector<Preset>& out)
{
    out.push_back (make (kSurface, "INIT DEEP SAW", "Lead", { "Deep", "Calm" }, Dry::No,
        "The honest starting point, and identical to the plug-in's own defaults: "
        "two saws seven cents apart, a sine sub twelve down, a 24 dB ladder at "
        "1.2 kHz opening under the mod envelope. Every water extra is at zero so "
        "the raw voice can be judged on its own.",
    {
        // Deliberately empty: INIT DEEP SAW *is* the baseline. Changing it means
        // changing initValues() above, which mirrors ParameterLayout.cpp.
    }));

    // --- DRY AQUATIC #1 ----------------------------------------------------
    out.push_back (make (kSurface, "SURFACE", "Key", { "Bright", "Surface", "Calm" }, Dry::Yes,
        "Sunlight broken on a moving surface. Glass partials, a thread of air "
        "noise and a bright scattered resonator. No effects at all.",
    {
        RIPPLES_FX_BYPASSED,

        { pid::oscAWave,      wv (OscWave::Glass) },
        { pid::oscAShape,     0.62f },
        { pid::oscAUnison,    uni (3) },
        { pid::oscADetune,    0.10f },
        { pid::oscAStereo,    0.75f },
        { pid::oscALevel,     lvl (-3.0f) },

        { pid::oscBWave,      wv (OscWave::Triangle) },
        { pid::oscBOctave,    oct (1) },
        { pid::oscBFine,      cents (4.0f) },
        { pid::oscBShape,     0.40f },
        { pid::oscBUnison,    uni (2) },
        { pid::oscBDetune,    0.08f },
        { pid::oscBStereo,    0.65f },
        { pid::oscBLevel,     lvl (-11.0f) },

        { pid::subLevel,      lvl (-24.0f) },
        { pid::noiseType,     nz (NoiseType::Air) },
        { pid::noiseLevel,    lvl (-27.0f) },
        { pid::noiseTone,     0.78f },

        { pid::filtMode,      fm (FilterMode::LP24) },
        { pid::filtCutoff,    hz (5200.0f) },
        { pid::filtReso,      0.30f },
        { pid::filtDrive,     0.08f },
        { pid::filtKeyTrack,  0.80f },
        { pid::filtEnvAmt,    bip (0.22f) },
        { pid::filtPressure,  0.15f },

        { pid::fenvAttack,    att (0.020f) },
        { pid::fenvDecay,     dec (1.60f) },
        { pid::fenvSustain,   0.40f },
        { pid::fenvRelease,   rel (1.80f) },
        { pid::fenvVelocity,  0.45f },

        { pid::aenvAttack,    att (0.030f) },
        { pid::aenvDecay,     dec (2.20f) },
        { pid::aenvSustain,   0.62f },
        { pid::aenvRelease,   rel (2.40f) },
        { pid::aenvVelocity,  0.55f },

        { pid::macroDepth,    0.20f },
        { pid::macroWet,      0.45f },
        { pid::macroRipple,   0.35f },
        { pid::macroCurrent,  0.40f },
        { pid::macroDrops,    0.30f },
        { pid::macroGlow,     0.70f },
        { pid::fluidX,        bip (-0.25f) },
        { pid::fluidY,        bip (-0.55f) },
        { pid::fluidXAmount,  0.45f },
        { pid::fluidYAmount,  0.60f },

        { pid::tideShape,     ts (TideShape::Flow) },
        { pid::tideRate,      tideHz (0.55f) },
        { pid::tideDepth,     0.34f },
        { pid::tideStereo,    0.60f },
        { pid::currRate,      currHz (0.70f) },
        { pid::currAmount,    0.42f },
        { pid::currSmooth,    0.55f },
        { pid::currDrift,     0.25f },
        { pid::currStereo,    0.80f },
        { pid::driftRate,     driftHz (0.06f) },
        { pid::driftAmount,   0.22f },
        { pid::rippleRate,    rippleHz (6.0f) },
        { pid::rippleDecay,   0.62f },
        { pid::rippleDepth,   0.30f },
        { pid::rippleCycles,  cycles (3.0f) },
        { pid::rippleSpread,  0.55f },

        { pid::dropAmount,    0.22f },
        { pid::dropDensity,   0.26f },
        { pid::dropSize,      0.28f },
        { pid::dropTone,      0.80f },
        { pid::dropSplash,    0.40f },
        { pid::dropGravity,   0.35f },
        { pid::dropBounce,    0.25f },
        { pid::dropRandom,    0.65f },
        { pid::dropSpread,    0.85f },

        { pid::resoAmount,    0.38f },
        { pid::resoSize,      0.70f },
        { pid::resoDecay,     0.44f },
        { pid::resoDamping,   0.34f },
        { pid::resoScatter,   0.45f },
        { pid::resoMotion,    0.38f },

        { pid::mastHigh,      eqdb (1.5f) },
        { pid::mastDrive,     0.06f },
    },
    {
        { ModSource::Tide,     ModDest::ShapeA,           0.30f },
        { ModSource::Current,  ModDest::FilterCutoff,     0.28f },
        { ModSource::Current,  ModDest::ResonatorScatter, 0.22f },
        { ModSource::Ripple,   ModDest::FilterCutoff,     0.26f },
        { ModSource::Drift,    ModDest::PanA,             0.35f },
        { ModSource::Velocity, ModDest::DropletAmount,    0.30f },
        { ModSource::KeyTrack, ModDest::ResonatorSize,   -0.25f },
    }));

    out.push_back (make (kSurface, "SUN ON WATER", "Key", { "Bright", "Glassy", "Surface" }, Dry::No,
        "Glints. A double-crested tide runs the amplitude so every held note "
        "flickers the way light does on a swell.",
    {
        { pid::oscAWave,      wv (OscWave::Glass) },
        { pid::oscAShape,     0.70f },
        { pid::oscAUnison,    uni (2) },
        { pid::oscADetune,    0.09f },
        { pid::oscAStereo,    0.70f },
        { pid::oscALevel,     lvl (-3.5f) },

        { pid::oscBWave,      wv (OscWave::Sine) },
        { pid::oscBOctave,    oct (1) },
        { pid::oscBSemitone,  semi (7) },
        { pid::oscBLevel,     lvl (-13.0f) },
        { pid::oscBPan,       bip (0.25f) },

        { pid::subLevel,      lvl (-26.0f) },
        { pid::noiseType,     nz (NoiseType::Air) },
        { pid::noiseLevel,    lvl (-30.0f) },
        { pid::noiseTone,     0.85f },

        { pid::filtMode,      fm (FilterMode::LP12) },
        { pid::filtCutoff,    hz (6800.0f) },
        { pid::filtReso,      0.24f },
        { pid::filtKeyTrack,  0.85f },
        { pid::filtEnvAmt,    bip (0.18f) },

        { pid::fenvAttack,    att (0.006f) },
        { pid::fenvDecay,     dec (0.70f) },
        { pid::fenvSustain,   0.35f },
        { pid::fenvRelease,   rel (1.20f) },

        { pid::aenvAttack,    att (0.012f) },
        { pid::aenvDecay,     dec (1.40f) },
        { pid::aenvSustain,   0.58f },
        { pid::aenvRelease,   rel (1.60f) },
        { pid::aenvVelocity,  0.60f },

        { pid::macroWet,      0.40f },
        { pid::macroGlow,     0.80f },
        { pid::macroSpace,    0.42f },
        { pid::macroRipple,   0.30f },
        { pid::fluidX,        bip (-0.20f) },
        { pid::fluidY,        bip (-0.64f) },

        { pid::tideShape,     ts (TideShape::DoubleWave) },
        { pid::tideRate,      tideHz (2.6f) },
        { pid::tideDepth,     0.30f },
        { pid::tideStereo,    0.55f },
        { pid::currRate,      currHz (0.5f) },
        { pid::currAmount,    0.25f },

        { pid::resoAmount,    0.26f },
        { pid::resoSize,      0.74f },
        { pid::resoDecay,     0.38f },
        { pid::resoDamping,   0.30f },

        { pid::scurEnable,    on },
        { pid::scurAmount,    0.28f },
        { pid::scurRate,      scurHz (0.12f) },
        { pid::chorEnable,    on },
        { pid::chorRate,      chorHz (0.8f) },
        { pid::chorDepth,     0.40f },
        { pid::chorMix,       0.30f },
        { pid::dlyEnable,     on },
        { pid::dlySync,       on },
        { pid::dlySyncTime,   sd (SyncDivision::EighthD) },
        { pid::dlyFeedback,   0.30f },
        { pid::dlyDamping,    0.62f },
        { pid::dlyMix,        0.22f },
        { pid::verbEnable,    on },
        { pid::verbSize,      0.50f },
        { pid::verbHighCut,   vHighHz (12000.0f) },
        { pid::verbMix,       0.28f },
        { pid::mastHigh,      eqdb (2.0f) },
    },
    {
        { ModSource::Tide,     ModDest::Amplitude,    0.30f },
        { ModSource::Tide,     ModDest::FilterCutoff, 0.20f },
        { ModSource::Velocity, ModDest::ShapeA,       0.30f },
        { ModSource::Drift,    ModDest::PanB,         0.40f },
        { ModSource::ModWheel, ModDest::DelayMix,     0.35f },
    }));

    // --- DRY AQUATIC #2 ----------------------------------------------------
    out.push_back (make (kSurface, "FOAM", "Texture", { "Surface", "Organic", "Bright" }, Dry::Yes,
        "Surf noise through a moving band-pass, with a fine spray of droplets. "
        "The wash is the instrument; the oscillators are only a thread of pitch.",
    {
        RIPPLES_FX_BYPASSED,

        { pid::oscAWave,      wv (OscWave::Water) },
        { pid::oscAShape,     0.55f },
        { pid::oscAUnison,    uni (4) },
        { pid::oscADetune,    0.30f },
        { pid::oscAStereo,    0.85f },
        { pid::oscALevel,     lvl (-16.0f) },

        { pid::oscBWave,      wv (OscWave::Hollow) },
        { pid::oscBOctave,    oct (1) },
        { pid::oscBLevel,     lvl (-22.0f) },
        { pid::oscBStereo,    0.80f },

        { pid::subLevel,      lvl (kSilentDb) },
        { pid::noiseType,     nz (NoiseType::Surf) },
        { pid::noiseLevel,    lvl (-4.0f) },
        { pid::noiseTone,     0.62f },

        { pid::filtMode,      fm (FilterMode::BP12) },
        { pid::filtCutoff,    hz (1800.0f) },
        { pid::filtReso,      0.42f },
        { pid::filtDrive,     0.14f },
        { pid::filtKeyTrack,  0.25f },
        { pid::filtEnvAmt,    bip (0.20f) },
        { pid::filtMovement,  0.60f },
        { pid::filtPressure,  0.30f },

        { pid::fenvAttack,    att (0.90f) },
        { pid::fenvDecay,     dec (3.00f) },
        { pid::fenvSustain,   0.55f },
        { pid::fenvRelease,   rel (3.50f) },

        { pid::aenvAttack,    att (1.50f) },
        { pid::aenvDecay,     dec (3.00f) },
        { pid::aenvSustain,   0.80f },
        { pid::aenvRelease,   rel (4.00f) },
        { pid::aenvVelocity,  0.25f },

        { pid::macroWet,      0.65f },
        { pid::macroCurrent,  0.70f },
        { pid::macroDrops,    0.55f },
        { pid::macroGlow,     0.55f },
        { pid::fluidX,        bip (0.25f) },
        { pid::fluidY,        bip (-0.60f) },
        { pid::fluidXAmount,  0.70f },

        { pid::tideShape,     ts (TideShape::Swell) },
        { pid::tideRate,      tideHz (0.22f) },
        { pid::tideDepth,     0.55f },
        { pid::tideStereo,    0.70f },
        { pid::currRate,      currHz (1.30f) },
        { pid::currAmount,    0.72f },
        { pid::currSmooth,    0.40f },
        { pid::currDrift,     0.35f },
        { pid::currStereo,    0.90f },
        { pid::driftRate,     driftHz (0.08f) },
        { pid::driftAmount,   0.30f },

        { pid::dropAmount,    0.42f },
        { pid::dropDensity,   0.72f },
        { pid::dropSize,      0.18f },
        { pid::dropTone,      0.86f },
        { pid::dropSplash,    0.62f },
        { pid::dropGravity,   0.30f },
        { pid::dropBounce,    0.20f },
        { pid::dropRandom,    0.85f },
        { pid::dropSpread,    0.95f },

        { pid::resoAmount,    0.20f },
        { pid::resoSize,      0.55f },
        { pid::resoDecay,     0.30f },
        { pid::resoDamping,   0.55f },
        { pid::resoScatter,   0.70f },
        { pid::resoMotion,    0.55f },

        { pid::mastLow,       eqdb (-2.0f) },
        { pid::mastHigh,      eqdb (1.0f) },
    },
    {
        { ModSource::Current,     ModDest::FilterCutoff,   0.60f },
        { ModSource::Current,     ModDest::NoiseTone,      0.40f },
        { ModSource::Tide,        ModDest::NoiseLevel,     0.35f },
        { ModSource::Tide,        ModDest::FilterCutoff,   0.25f },
        { ModSource::Drift,       ModDest::DropletDensity, 0.40f },
        { ModSource::AmpEnvelope, ModDest::DropletAmount,  0.45f },
        { ModSource::Velocity,    ModDest::NoiseLevel,     0.25f },
    }));

    // --- DRY AQUATIC #3 ----------------------------------------------------
    out.push_back (make (kSurface, "SKIPPING STONE", "Pluck", { "Surface", "Bright", "Wet" }, Dry::Yes,
        "A stone thrown flat. The RIPPLE modulator bends the pitch in fast "
        "damped bounces and the droplet engine's gravity does the rest.",
    {
        RIPPLES_FX_BYPASSED,

        { pid::oscAWave,      wv (OscWave::Triangle) },
        { pid::oscAShape,     0.35f },
        { pid::oscAUnison,    uni (2) },
        { pid::oscADetune,    0.06f },
        { pid::oscALevel,     lvl (-2.0f) },

        { pid::oscBWave,      wv (OscWave::Glass) },
        { pid::oscBOctave,    oct (1) },
        { pid::oscBShape,     0.55f },
        { pid::oscBLevel,     lvl (-9.0f) },
        { pid::oscBStereo,    0.60f },

        { pid::subLevel,      lvl (-20.0f) },
        { pid::noiseType,     nz (NoiseType::Bubble) },
        { pid::noiseLevel,    lvl (-22.0f) },
        { pid::noiseTone,     0.70f },

        { pid::filtMode,      fm (FilterMode::LP24) },
        { pid::filtCutoff,    hz (2400.0f) },
        { pid::filtReso,      0.38f },
        { pid::filtDrive,     0.16f },
        { pid::filtKeyTrack,  0.70f },
        { pid::filtEnvAmt,    bip (0.55f) },
        { pid::filtPressure,  0.18f },

        { pid::fenvAttack,    att (0.002f) },
        { pid::fenvDecay,     dec (0.18f) },
        { pid::fenvSustain,   0.05f },
        { pid::fenvRelease,   rel (0.35f) },
        { pid::fenvVelocity,  0.65f },

        { pid::aenvAttack,    att (0.002f) },
        { pid::aenvDecay,     dec (0.55f) },
        { pid::aenvSustain,   0.00f },
        { pid::aenvRelease,   rel (0.45f) },
        { pid::aenvVelocity,  0.70f },

        { pid::macroRipple,   0.85f },
        { pid::macroDrops,    0.60f },
        { pid::macroWet,      0.40f },
        { pid::macroGlow,     0.55f },
        { pid::fluidX,        bip (0.10f) },
        { pid::fluidY,        bip (-0.50f) },

        { pid::rippleRate,    rippleHz (9.0f) },
        { pid::rippleDecay,   0.72f },
        { pid::rippleDepth,   0.70f },
        { pid::rippleCycles,  cycles (8.0f) },
        { pid::rippleSpread,  0.45f },
        { pid::rippleTrigger, rtg (RippleTrigger::NoteOn) },
        { pid::tideRate,      tideHz (0.8f) },
        { pid::tideDepth,     0.15f },
        { pid::currRate,      currHz (0.9f) },
        { pid::currAmount,    0.25f },

        { pid::dropAmount,    0.55f },
        { pid::dropMode,      dm (DropletMode::Note) },
        { pid::dropDensity,   0.45f },
        { pid::dropSize,      0.24f },
        { pid::dropTone,      0.72f },
        { pid::dropSplash,    0.35f },
        { pid::dropGravity,   0.88f },
        { pid::dropBounce,    0.82f },
        { pid::dropRandom,    0.30f },
        { pid::dropSpread,    0.70f },

        { pid::resoAmount,    0.30f },
        { pid::resoSize,      0.38f },
        { pid::resoDecay,     0.34f },
        { pid::resoDamping,   0.40f },
        { pid::resoScatter,   0.30f },
        { pid::resoMotion,    0.20f },

        { pid::glideMode,     gmd (GlideMode::Legato) },
        { pid::glideTime,     gtime (0.04f) },
    },
    {
        { ModSource::Ripple,      ModDest::PitchAll,      0.30f },
        { ModSource::Ripple,      ModDest::FilterCutoff,  0.45f },
        { ModSource::Ripple,      ModDest::DropletSize,  -0.30f },
        { ModSource::Velocity,    ModDest::DropletAmount, 0.50f },
        { ModSource::Velocity,    ModDest::FilterCutoff,  0.35f },
        { ModSource::ModEnvelope, ModDest::ShapeB,        0.35f },
        { ModSource::KeyTrack,    ModDest::DropletTone,   0.30f },
    }));

    out.push_back (make (kSurface, "HORIZON LINE", "Pad", { "Calm", "Dreamy", "Surface" }, Dry::No,
        "A flat, patient pad for the top of an arrangement. Five-voice unison "
        "drifting slowly out of tune with itself and back again.",
    {
        { pid::oscAWave,      wv (OscWave::Water) },
        { pid::oscAShape,     0.45f },
        { pid::oscAUnison,    uni (5) },
        { pid::oscADetune,    0.30f },
        { pid::oscAStereo,    0.90f },
        { pid::oscALevel,     lvl (-4.0f) },

        { pid::oscBWave,      wv (OscWave::Hollow) },
        { pid::oscBFine,      cents (7.0f) },
        { pid::oscBUnison,    uni (3) },
        { pid::oscBDetune,    0.22f },
        { pid::oscBStereo,    0.85f },
        { pid::oscBLevel,     lvl (-9.0f) },

        { pid::subLevel,      lvl (-20.0f) },
        { pid::noiseType,     nz (NoiseType::Air) },
        { pid::noiseLevel,    lvl (-32.0f) },
        { pid::noiseTone,     0.72f },

        { pid::filtMode,      fm (FilterMode::LP12) },
        { pid::filtCutoff,    hz (3000.0f) },
        { pid::filtReso,      0.16f },
        { pid::filtKeyTrack,  0.45f },
        { pid::filtEnvAmt,    bip (0.25f) },
        { pid::filtPressure,  0.28f },

        { pid::fenvAttack,    att (2.20f) },
        { pid::fenvDecay,     dec (4.00f) },
        { pid::fenvSustain,   0.60f },
        { pid::fenvRelease,   rel (5.00f) },

        { pid::aenvAttack,    att (2.50f) },
        { pid::aenvDecay,     dec (3.00f) },
        { pid::aenvSustain,   0.85f },
        { pid::aenvRelease,   rel (5.50f) },
        { pid::aenvVelocity,  0.20f },

        { pid::macroDepth,    0.30f },
        { pid::macroWet,      0.50f },
        { pid::macroCurrent,  0.45f },
        { pid::macroSpace,    0.65f },
        { pid::macroGlow,     0.50f },
        { pid::fluidX,        bip (-0.56f) },
        { pid::fluidY,        bip (-0.40f) },

        { pid::tideShape,     ts (TideShape::Flow) },
        { pid::tideRate,      tideHz (0.12f) },
        { pid::tideDepth,     0.35f },
        { pid::tideStereo,    0.75f },
        { pid::driftRate,     driftHz (0.025f) },
        { pid::driftAmount,   0.45f },
        { pid::driftStereo,   0.85f },
        { pid::currRate,      currHz (0.18f) },
        { pid::currAmount,    0.35f },
        { pid::currSmooth,    0.80f },

        { pid::resoAmount,    0.18f },
        { pid::resoSize,      0.65f },
        { pid::resoDecay,     0.50f },
        { pid::resoDamping,   0.50f },

        { pid::scurEnable,    on },
        { pid::scurAmount,    0.35f },
        { pid::scurRate,      scurHz (0.08f) },
        { pid::chorEnable,    on },
        { pid::chorRate,      chorHz (0.25f) },
        { pid::chorDepth,     0.45f },
        { pid::chorDelay,     chdel (18.0f) },
        { pid::chorMix,       0.32f },
        { pid::dlyEnable,     on },
        { pid::dlyTime,       dtime (0.62f) },
        { pid::dlyFeedback,   0.30f },
        { pid::dlyDamping,    0.65f },
        { pid::dlyMix,        0.18f },
        { pid::diffEnable,    on },
        { pid::diffMix,       0.28f },
        { pid::verbEnable,    on },
        { pid::verbSize,      0.72f },
        { pid::verbDecay,     0.68f },
        { pid::verbPredelay,  pre (35.0f) },
        { pid::verbMix,       0.40f },
        { pid::mastLow,       eqdb (-1.0f) },
    },
    {
        { ModSource::Drift,   ModDest::FineAll,      0.25f },
        { ModSource::Drift,   ModDest::FilterCutoff, 0.28f },
        { ModSource::Tide,    ModDest::ShapeA,       0.35f },
        { ModSource::Current, ModDest::PanB,         0.40f },
        { ModSource::Current, ModDest::OscMix,       0.25f },
        { ModSource::Note,    ModDest::Amplitude,   -0.15f },
    }));
}

//==============================================================================
//  SHALLOW WATER — clear, playful, close to the bottom. Plucks and small keys.
//==============================================================================
void addShallowBank (std::vector<Preset>& out)
{
    // --- DRY AQUATIC #4 ----------------------------------------------------
    out.push_back (make (kShallow, "LIQUID PLUCK", "Pluck", { "Wet", "Glassy", "Bright" }, Dry::Yes,
        "The reference pluck. A hard mod-envelope sweep for the attack, the "
        "resonator for the bubble that follows it, note-mode droplets for the "
        "splash. Nothing after the voice at all.",
    {
        RIPPLES_FX_BYPASSED,

        { pid::oscAWave,      wv (OscWave::Water) },
        { pid::oscAShape,     0.48f },
        { pid::oscAUnison,    uni (2) },
        { pid::oscADetune,    0.09f },
        { pid::oscAStereo,    0.55f },
        { pid::oscALevel,     lvl (-2.5f) },
        { pid::oscAPhase,     startPhase (0.0f) },   // consistent attack transient

        { pid::oscBWave,      wv (OscWave::Glass) },
        { pid::oscBOctave,    oct (1) },
        { pid::oscBShape,     0.62f },
        { pid::oscBFine,      cents (-5.0f) },
        { pid::oscBLevel,     lvl (-10.0f) },
        { pid::oscBStereo,    0.70f },
        { pid::oscBPhase,     startPhase (0.0f) },

        { pid::subWave,       sw (SubWave::Sine) },
        { pid::subLevel,      lvl (-17.0f) },
        { pid::noiseType,     nz (NoiseType::Bubble) },
        { pid::noiseLevel,    lvl (-21.0f) },
        { pid::noiseTone,     0.66f },

        { pid::filtMode,      fm (FilterMode::LP24) },
        { pid::filtCutoff,    hz (700.0f) },
        { pid::filtReso,      0.46f },
        { pid::filtDrive,     0.20f },
        { pid::filtKeyTrack,  0.75f },
        { pid::filtEnvAmt,    bip (0.72f) },
        { pid::filtPressure,  0.22f },

        { pid::fenvAttack,    att (0.001f) },
        { pid::fenvDecay,     dec (0.30f) },
        { pid::fenvSustain,   0.08f },
        { pid::fenvRelease,   rel (0.40f) },
        { pid::fenvVelocity,  0.70f },

        { pid::aenvAttack,    att (0.002f) },
        { pid::aenvDecay,     dec (1.10f) },
        { pid::aenvSustain,   0.10f },
        { pid::aenvRelease,   rel (0.70f) },
        { pid::aenvVelocity,  0.65f },

        { pid::macroDepth,    0.35f },
        { pid::macroWet,      0.60f },
        { pid::macroRipple,   0.45f },
        { pid::macroDrops,    0.50f },
        { pid::macroGlow,     0.55f },
        { pid::fluidX,        bip (0.05f) },
        { pid::fluidY,        bip (-0.25f) },

        { pid::tideRate,      tideHz (0.9f) },
        { pid::tideDepth,     0.18f },
        { pid::tideStereo,    0.40f },
        { pid::currRate,      currHz (1.4f) },
        { pid::currAmount,    0.30f },
        { pid::currSmooth,    0.45f },
        { pid::rippleRate,    rippleHz (7.5f) },
        { pid::rippleDecay,   0.68f },
        { pid::rippleDepth,   0.42f },
        { pid::rippleCycles,  cycles (5.0f) },
        { pid::rippleSpread,  0.40f },

        { pid::dropAmount,    0.45f },
        { pid::dropMode,      dm (DropletMode::Note) },
        { pid::dropDensity,   0.35f },
        { pid::dropSize,      0.32f },
        { pid::dropTone,      0.68f },
        { pid::dropSplash,    0.55f },
        { pid::dropGravity,   0.60f },
        { pid::dropBounce,    0.45f },
        { pid::dropRandom,    0.40f },
        { pid::dropSpread,    0.75f },

        { pid::resoAmount,    0.52f },
        { pid::resoSize,      0.44f },
        { pid::resoDecay,     0.56f },
        { pid::resoDamping,   0.38f },
        { pid::resoScatter,   0.28f },
        { pid::resoMotion,    0.30f },

        { pid::mastDrive,     0.10f },
        { pid::mastHigh,      eqdb (1.0f) },
    },
    {
        { ModSource::ModEnvelope, ModDest::ResonatorSize,   -0.30f },
        { ModSource::Ripple,      ModDest::FilterCutoff,     0.40f },
        { ModSource::Ripple,      ModDest::ResonatorScatter, 0.25f },
        { ModSource::Velocity,    ModDest::DropletAmount,    0.45f },
        { ModSource::Velocity,    ModDest::ResonatorAmount,  0.30f },
        { ModSource::Current,     ModDest::ShapeA,           0.30f },
        { ModSource::KeyTrack,    ModDest::DropletSize,     -0.35f },
        { ModSource::RandomPerNote, ModDest::PanA,           0.30f },
    }));

    out.push_back (make (kShallow, "TIDE POOL", "Key", { "Organic", "Calm", "Wet" }, Dry::No,
        "A small warm keyboard. Hollow odd harmonics, a bubble bed under it, "
        "and just enough room around it to sit in a mix.",
    {
        { pid::oscAWave,      wv (OscWave::Hollow) },
        { pid::oscAShape,     0.42f },
        { pid::oscAUnison,    uni (2) },
        { pid::oscADetune,    0.12f },
        { pid::oscAStereo,    0.50f },
        { pid::oscALevel,     lvl (-3.0f) },

        { pid::oscBWave,      wv (OscWave::Sine) },
        { pid::oscBOctave,    oct (-1) },
        { pid::oscBLevel,     lvl (-8.0f) },

        { pid::subLevel,      lvl (-15.0f) },
        { pid::noiseType,     nz (NoiseType::Bubble) },
        { pid::noiseLevel,    lvl (-25.0f) },
        { pid::noiseTone,     0.45f },

        { pid::filtMode,      fm (FilterMode::LP12) },
        { pid::filtCutoff,    hz (1500.0f) },
        { pid::filtReso,      0.22f },
        { pid::filtDrive,     0.18f },
        { pid::filtKeyTrack,  0.65f },
        { pid::filtEnvAmt,    bip (0.40f) },
        { pid::filtPressure,  0.35f },

        { pid::fenvAttack,    att (0.004f) },
        { pid::fenvDecay,     dec (0.80f) },
        { pid::fenvSustain,   0.25f },
        { pid::fenvRelease,   rel (0.90f) },
        { pid::fenvVelocity,  0.55f },

        { pid::aenvAttack,    att (0.006f) },
        { pid::aenvDecay,     dec (2.00f) },
        { pid::aenvSustain,   0.35f },
        { pid::aenvRelease,   rel (1.30f) },
        { pid::aenvVelocity,  0.60f },

        { pid::macroDepth,    0.35f },
        { pid::macroWet,      0.45f },
        { pid::macroDrops,    0.30f },
        { pid::macroSpace,    0.40f },
        { pid::macroPressure, 0.35f },
        { pid::fluidX,        bip (-0.15f) },
        { pid::fluidY,        bip (-0.20f) },

        { pid::tideShape,     ts (TideShape::Triangle) },
        { pid::tideRate,      tideHz (0.7f) },
        { pid::tideDepth,     0.22f },
        { pid::tideStereo,    0.45f },
        { pid::currRate,      currHz (0.6f) },
        { pid::currAmount,    0.38f },
        { pid::currSmooth,    0.65f },

        { pid::dropAmount,    0.28f },
        { pid::dropDensity,   0.30f },
        { pid::dropSize,      0.45f },
        { pid::dropTone,      0.52f },
        { pid::dropSpread,    0.70f },

        { pid::resoAmount,    0.30f },
        { pid::resoSize,      0.50f },
        { pid::resoDecay,     0.46f },
        { pid::resoDamping,   0.52f },

        { pid::scurEnable,    on },
        { pid::scurAmount,    0.25f },
        { pid::chorEnable,    on },
        { pid::chorRate,      chorHz (0.45f) },
        { pid::chorDepth,     0.32f },
        { pid::chorMix,       0.24f },
        { pid::dlyEnable,     on },
        { pid::dlySync,       on },
        { pid::dlySyncTime,   sd (SyncDivision::Eighth) },
        { pid::dlyFeedback,   0.28f },
        { pid::dlyDamping,    0.70f },
        { pid::dlyMix,        0.18f },
        { pid::verbEnable,    on },
        { pid::verbSize,      0.45f },
        { pid::verbDecay,     0.42f },
        { pid::verbMix,       0.24f },
    },
    {
        { ModSource::Velocity, ModDest::FilterCutoff,  0.40f },
        { ModSource::Current,  ModDest::FilterCutoff,  0.25f },
        { ModSource::Tide,     ModDest::DropletDensity, 0.25f },
        { ModSource::ModWheel, ModDest::ResonatorAmount, 0.40f },
        { ModSource::Drift,    ModDest::FineAll,       0.10f },
    }));

    out.push_back (make (kShallow, "WADING", "Bass", { "Organic", "Wet", "Surface" }, Dry::No,
        "A mid bass that stays out of the sub region — for tracks that already "
        "have a low end. Key-tracked LP12, short mod envelope, mono legato.",
    {
        { pid::oscAWave,      wv (OscWave::Triangle) },
        { pid::oscAShape,     0.55f },
        { pid::oscAUnison,    uni (1) },
        { pid::oscALevel,     lvl (-2.0f) },
        { pid::oscAPhase,     startPhase (0.0f) },

        { pid::oscBWave,      wv (OscWave::Hollow) },
        { pid::oscBOctave,    oct (0) },
        { pid::oscBFine,      cents (-9.0f) },
        { pid::oscBLevel,     lvl (-7.0f) },

        { pid::subWave,       sw (SubWave::Triangle) },
        { pid::subOctave,     subOct (-1) },
        { pid::subLevel,      lvl (-10.0f) },
        { pid::noiseType,     nz (NoiseType::Deep) },
        { pid::noiseLevel,    lvl (-30.0f) },
        { pid::noiseTone,     0.30f },

        { pid::filtMode,      fm (FilterMode::LP12) },
        { pid::filtCutoff,    hz (420.0f) },
        { pid::filtReso,      0.34f },
        { pid::filtDrive,     0.30f },
        { pid::filtKeyTrack,  0.85f },
        { pid::filtEnvAmt,    bip (0.55f) },
        { pid::filtPressure,  0.45f },

        { pid::fenvAttack,    att (0.001f) },
        { pid::fenvDecay,     dec (0.22f) },
        { pid::fenvSustain,   0.18f },
        { pid::fenvRelease,   rel (0.25f) },
        { pid::fenvVelocity,  0.60f },

        { pid::aenvAttack,    att (0.003f) },
        { pid::aenvDecay,     dec (0.60f) },
        { pid::aenvSustain,   0.72f },
        { pid::aenvRelease,   rel (0.24f) },
        { pid::aenvVelocity,  0.45f },

        { pid::macroDepth,    0.55f },
        { pid::macroPressure, 0.50f },
        { pid::macroWet,      0.25f },
        { pid::fluidX,        bip (-0.30f) },
        { pid::fluidY,        bip (0.15f) },

        { pid::tideRate,      tideHz (0.5f) },
        { pid::tideDepth,     0.12f },
        { pid::currRate,      currHz (0.8f) },
        { pid::currAmount,    0.22f },
        { pid::currSmooth,    0.60f },

        { pid::resoAmount,    0.16f },
        { pid::resoSize,      0.30f },
        { pid::resoDecay,     0.30f },
        { pid::resoDamping,   0.70f },

        { pid::scurEnable,    on },
        { pid::scurAmount,    0.15f },
        { pid::scurWidth,     0.30f },
        { pid::diffEnable,    on },
        { pid::diffAmount,    0.25f },
        { pid::diffMix,       0.14f },
        { pid::verbEnable,    on },
        { pid::verbSize,      0.35f },
        { pid::verbDecay,     0.30f },
        { pid::verbLowCut,    vLowHz (320.0f) },
        { pid::verbMix,       0.12f },

        { pid::mastLow,       eqdb (1.5f) },
        { pid::mastDrive,     0.18f },
        { pid::voiceCount,    vox (1) },
        { pid::glideMode,     gmd (GlideMode::Legato) },
        { pid::glideTime,     gtime (0.06f) },
    },
    {
        { ModSource::Velocity,    ModDest::FilterCutoff, 0.45f },
        { ModSource::Velocity,    ModDest::FilterDrive,  0.25f },
        { ModSource::ModEnvelope, ModDest::ShapeA,       0.30f },
        { ModSource::Current,     ModDest::FilterCutoff, 0.18f },
        { ModSource::ModWheel,    ModDest::FilterResonance, 0.30f },
    }));

    out.push_back (make (kShallow, "RIPPLE RINGS", "Arp", { "Wet", "Bright", "Glassy" }, Dry::No,
        "Written for sixteenths. Tempo-synced tide on the shape, a dotted-eighth "
        "delay behind it, and a short resonator so every note leaves a ring.",
    {
        { pid::oscAWave,      wv (OscWave::Glass) },
        { pid::oscAShape,     0.58f },
        { pid::oscAUnison,    uni (2) },
        { pid::oscADetune,    0.07f },
        { pid::oscAStereo,    0.60f },
        { pid::oscALevel,     lvl (-3.0f) },
        { pid::oscAPhase,     startPhase (0.0f) },

        { pid::oscBWave,      wv (OscWave::Pulse) },
        { pid::oscBShape,     0.30f },
        { pid::oscBOctave,    oct (1) },
        { pid::oscBLevel,     lvl (-14.0f) },
        { pid::oscBInterMode, im (InteractionMode::RingMod) },
        { pid::oscBInterAmt,  0.25f },

        { pid::subLevel,      lvl (-22.0f) },
        { pid::noiseType,     nz (NoiseType::Bubble) },
        { pid::noiseLevel,    lvl (-26.0f) },
        { pid::noiseTone,     0.72f },

        { pid::filtMode,      fm (FilterMode::LP24) },
        { pid::filtCutoff,    hz (1400.0f) },
        { pid::filtReso,      0.40f },
        { pid::filtDrive,     0.14f },
        { pid::filtKeyTrack,  0.70f },
        { pid::filtEnvAmt,    bip (0.60f) },

        { pid::fenvAttack,    att (0.001f) },
        { pid::fenvDecay,     dec (0.16f) },
        { pid::fenvSustain,   0.00f },
        { pid::fenvRelease,   rel (0.20f) },
        { pid::fenvVelocity,  0.75f },

        { pid::aenvAttack,    att (0.001f) },
        { pid::aenvDecay,     dec (0.40f) },
        { pid::aenvSustain,   0.00f },
        { pid::aenvRelease,   rel (0.30f) },
        { pid::aenvVelocity,  0.80f },

        { pid::macroRipple,   0.60f },
        { pid::macroWet,      0.50f },
        { pid::macroGlow,     0.60f },
        { pid::macroSpace,    0.45f },
        { pid::fluidX,        bip (0.30f) },
        { pid::fluidY,        bip (-0.30f) },

        { pid::tideShape,     ts (TideShape::Triangle) },
        { pid::tideSync,      on },
        { pid::tideSyncRate,  sd (SyncDivision::Half) },
        { pid::tideDepth,     0.45f },
        { pid::tideStereo,    0.50f },
        { pid::rippleRate,    rippleHz (12.0f) },
        { pid::rippleDecay,   0.80f },
        { pid::rippleDepth,   0.35f },
        { pid::rippleCycles,  cycles (3.0f) },

        { pid::dropAmount,    0.20f },
        { pid::dropMode,      dm (DropletMode::Note) },
        { pid::dropDensity,   0.22f },
        { pid::dropSize,      0.25f },
        { pid::dropTone,      0.80f },

        { pid::resoAmount,    0.34f },
        { pid::resoSize,      0.42f },
        { pid::resoDecay,     0.40f },
        { pid::resoDamping,   0.35f },
        { pid::resoScatter,   0.40f },

        { pid::scurEnable,    on },
        { pid::chorEnable,    on },
        { pid::chorRate,      chorHz (1.1f) },
        { pid::chorDepth,     0.30f },
        { pid::chorMix,       0.22f },
        { pid::dlyEnable,     on },
        { pid::dlySync,       on },
        { pid::dlySyncTime,   sd (SyncDivision::EighthD) },
        { pid::dlyFeedback,   0.46f },
        { pid::dlyMotion,     0.35f },
        { pid::dlySpread,     0.60f },
        { pid::dlyDamping,    0.55f },
        { pid::dlyMix,        0.30f },
        { pid::diffEnable,    on },
        { pid::diffMix,       0.20f },
        { pid::verbEnable,    on },
        { pid::verbSize,      0.50f },
        { pid::verbDecay,     0.45f },
        { pid::verbMix,       0.24f },
        { pid::mastHigh,      eqdb (1.5f) },
    },
    {
        { ModSource::Tide,          ModDest::ShapeA,            0.40f },
        { ModSource::Tide,          ModDest::FilterCutoff,      0.30f },
        { ModSource::Ripple,        ModDest::InteractionAmount, 0.35f },
        { ModSource::Velocity,      ModDest::FilterCutoff,      0.40f },
        { ModSource::RandomPerNote, ModDest::PanA,              0.45f },
        { ModSource::RandomPerNote, ModDest::FineAll,           0.06f },
        { ModSource::KeyTrack,      ModDest::DelayMix,         -0.20f },
    }));

    // --- DRY AQUATIC #5 ----------------------------------------------------
    out.push_back (make (kShallow, "SHOALS", "Texture", { "Organic", "Chaotic", "Wet" }, Dry::Yes,
        "A thousand small movements. Fast Current on shape, pan and resonator "
        "scatter, with dense atmospheric droplets — a shoal turning at once.",
    {
        RIPPLES_FX_BYPASSED,

        { pid::oscAWave,      wv (OscWave::Water) },
        { pid::oscAShape,     0.66f },
        { pid::oscAUnison,    uni (5) },
        { pid::oscADetune,    0.24f },
        { pid::oscAStereo,    0.95f },
        { pid::oscALevel,     lvl (-6.0f) },

        { pid::oscBWave,      wv (OscWave::Glass) },
        { pid::oscBOctave,    oct (1) },
        { pid::oscBSemitone,  semi (5) },
        { pid::oscBUnison,    uni (3) },
        { pid::oscBDetune,    0.20f },
        { pid::oscBStereo,    0.90f },
        { pid::oscBLevel,     lvl (-13.0f) },

        { pid::subLevel,      lvl (-26.0f) },
        { pid::noiseType,     nz (NoiseType::Bubble) },
        { pid::noiseLevel,    lvl (-19.0f) },
        { pid::noiseTone,     0.60f },

        { pid::filtMode,      fm (FilterMode::Morph) },
        { pid::filtCutoff,    hz (1600.0f) },
        { pid::filtReso,      0.44f },
        { pid::filtDrive,     0.12f },
        { pid::filtKeyTrack,  0.40f },
        { pid::filtEnvAmt,    bip (0.25f) },
        { pid::filtMovement,  0.35f },
        { pid::filtPressure,  0.25f },

        { pid::fenvAttack,    att (0.25f) },
        { pid::fenvDecay,     dec (1.50f) },
        { pid::fenvSustain,   0.45f },
        { pid::fenvRelease,   rel (2.00f) },

        { pid::aenvAttack,    att (0.40f) },
        { pid::aenvDecay,     dec (2.00f) },
        { pid::aenvSustain,   0.70f },
        { pid::aenvRelease,   rel (2.20f) },
        { pid::aenvVelocity,  0.40f },

        { pid::macroCurrent,  0.85f },
        { pid::macroWet,      0.55f },
        { pid::macroDrops,    0.65f },
        { pid::macroRipple,   0.30f },
        { pid::macroGlow,     0.45f },
        { pid::fluidX,        bip (0.55f) },
        { pid::fluidY,        bip (-0.20f) },
        { pid::fluidXAmount,  0.80f },

        { pid::tideShape,     ts (TideShape::DoubleWave) },
        { pid::tideRate,      tideHz (1.6f) },
        { pid::tideDepth,     0.30f },
        { pid::tideStereo,    0.85f },
        { pid::currRate,      currHz (3.2f) },
        { pid::currAmount,    0.80f },
        { pid::currSmooth,    0.30f },
        { pid::currDrift,     0.45f },
        { pid::currStereo,    0.95f },
        { pid::driftRate,     driftHz (0.12f) },
        { pid::driftAmount,   0.40f },

        { pid::dropAmount,    0.50f },
        { pid::dropDensity,   0.80f },
        { pid::dropSize,      0.22f },
        { pid::dropTone,      0.70f },
        { pid::dropSplash,    0.45f },
        { pid::dropGravity,   0.40f },
        { pid::dropBounce,    0.35f },
        { pid::dropRandom,    0.90f },
        { pid::dropSpread,    1.00f },

        { pid::resoAmount,    0.40f },
        { pid::resoSize,      0.48f },
        { pid::resoDecay,     0.42f },
        { pid::resoDamping,   0.45f },
        { pid::resoScatter,   0.75f },
        { pid::resoMotion,    0.70f },

        { pid::mastLow,       eqdb (-1.5f) },
        { pid::mastDrive,     0.08f },
    },
    {
        { ModSource::Current,     ModDest::ShapeA,           0.55f },
        { ModSource::Current,     ModDest::PanA,             0.50f },
        { ModSource::Current,     ModDest::ResonatorScatter, 0.45f },
        { ModSource::Tide,        ModDest::FilterMovement,   0.40f },
        { ModSource::Tide,        ModDest::PanB,            -0.45f },
        { ModSource::Drift,       ModDest::FilterCutoff,     0.30f },
        { ModSource::Drift,       ModDest::DropletDensity,   0.35f },
        { ModSource::AmpEnvelope, ModDest::DropletAmount,    0.40f },
        { ModSource::ModWheel,    ModDest::DropletDensity,   0.40f },
    }));
}
