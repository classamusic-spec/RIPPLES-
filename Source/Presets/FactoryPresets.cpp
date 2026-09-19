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
    Presets marked `Dry::Yes` read as water with every global effect off —
    chorus, delay, diffusion, reverb AND stereo current all disabled and at zero
    mix (see RIPPLES_FX_BYPASSED). Their character comes from the waveforms, the
    noise colour, the Depth filter, the Current / Drift / Ripple modulators, the
    droplet engine and the resonator instead. They are also flagged at runtime
    as PresetInfo::dryAquatic. The fifteen of them:

        THE SURFACE       SURFACE              Glass partials + air noise + scattered resonator
        THE SURFACE       FOAM                 Surf noise through a Current-swept band-pass
        THE SURFACE       SKIPPING STONE       Ripple modulator on pitch + droplet gravity/bounce
        SHALLOW WATER     LIQUID PLUCK         Mod-env sweep, resonator bubble, note-mode splash
        SHALLOW WATER     SHOALS               Fast Current on shape / pan / resonator scatter
        DEEP BLUE         SUBMERGED            Low LP24 + pressure, Deep noise, huge resonator
        THE ABYSS         ABYSS BASS           Key-tracked resonant LP24, note droplets, Ripple attack
        THE ABYSS         OCEAN FLOOR          Resonator at max size/decay standing in for a room
        DROPLETS          TINY BUBBLES         Dense tiny droplets + bubble noise + short resonator
        DROPLETS          RAIN ON GLASS        Maximum density, minimum size, short glassy resonator
        CURRENTS          CRYSTAL CURRENT      Resonator and random pan replace the delay
        CURRENTS          RIVER MOUTH          Stereo from decorrelated Current / Drift alone
        BIOLUMINESCENCE   BIOLUMINESCENT KEYS  Glass inharmonics into a large undamped resonator
        BIOLUMINESCENCE   PHOSPHOR             16 ms amp decay; all sustain is resonator ring
        STORMS            BREAKERS             Swell tide drives noise, cutoff and droplet density
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

//==============================================================================
//  DEEP BLUE — open water. Wide, slow, blue. The centre of the product.
//==============================================================================
void addDeepBlueBank (std::vector<Preset>& out)
{
    // The default preset. One chord has to say what RIPPLES is: deep, moving,
    // wide, unmistakably underwater, and still musical enough to keep.
    out.push_back (make (kDeepBlue, "SUBMERGED DREAMS", "Pad", { "Deep", "Cinematic" }, Dry::No,
        "The first thing you hear. A five-voice Water pad over a Hollow layer, "
        "sunk under an LP24 at 900 Hz with the pressure control filling the low "
        "mids. Drift detunes it a few cents over half a minute, Current walks "
        "the A/B balance, slow droplets fall somewhere behind it, and the whole "
        "thing sits in a wide, dark, modulated Abyss reverb.",
    {
        { pid::oscAWave,      wv (OscWave::Water) },
        { pid::oscAShape,     0.42f },
        { pid::oscAUnison,    uni (5) },
        { pid::oscADetune,    0.22f },
        { pid::oscAStereo,    0.90f },
        { pid::oscAPan,       bip (-0.08f) },
        { pid::oscALevel,     lvl (-4.0f) },

        { pid::oscBWave,      wv (OscWave::Hollow) },
        { pid::oscBShape,     0.55f },
        { pid::oscBOctave,    oct (0) },
        { pid::oscBFine,      cents (-7.0f) },
        { pid::oscBUnison,    uni (3) },
        { pid::oscBDetune,    0.18f },
        { pid::oscBStereo,    0.85f },
        { pid::oscBPan,       bip (0.08f) },
        { pid::oscBLevel,     lvl (-7.0f) },

        { pid::subWave,       sw (SubWave::Sine) },
        { pid::subOctave,     subOct (-1) },
        { pid::subLevel,      lvl (-23.0f) },
        { pid::noiseType,     nz (NoiseType::Deep) },
        { pid::noiseLevel,    lvl (-30.0f) },
        { pid::noiseTone,     0.35f },

        { pid::filtMode,      fm (FilterMode::LP24) },
        { pid::filtCutoff,    hz (3200.0f) },
        { pid::filtReso,      0.28f },
        { pid::filtDrive,     0.18f },
        { pid::filtKeyTrack,  0.35f },
        { pid::filtEnvAmt,    bip (0.30f) },
        { pid::filtMovement,  0.50f },
        { pid::filtPressure,  0.45f },

        { pid::fenvAttack,    att (1.20f) },
        { pid::fenvDecay,     dec (3.00f) },
        { pid::fenvSustain,   0.55f },
        { pid::fenvRelease,   rel (4.00f) },
        { pid::fenvVelocity,  0.35f },

        { pid::aenvAttack,    att (0.90f) },
        { pid::aenvDecay,     dec (2.50f) },
        { pid::aenvSustain,   0.85f },
        { pid::aenvRelease,   rel (4.50f) },
        { pid::aenvVelocity,  0.35f },

        { pid::macroDepth,    0.70f },
        { pid::macroWet,      0.55f },
        { pid::macroRipple,   0.25f },
        { pid::macroCurrent,  0.45f },
        { pid::macroDrops,    0.25f },
        { pid::macroPressure, 0.45f },
        { pid::macroSpace,    0.70f },
        { pid::macroGlow,     0.40f },
        { pid::fluidX,        bip (-0.35f) },
        { pid::fluidY,        bip (0.55f) },
        { pid::fluidXAmount,  0.55f },
        { pid::fluidYAmount,  0.75f },

        { pid::tideShape,     ts (TideShape::Flow) },
        { pid::tideRate,      tideHz (0.16f) },
        { pid::tideDepth,     0.42f },
        { pid::tideStereo,    0.65f },
        { pid::currRate,      currHz (0.22f) },
        { pid::currAmount,    0.50f },
        { pid::currSmooth,    0.75f },
        { pid::currDrift,     0.35f },
        { pid::currStereo,    0.80f },
        { pid::driftRate,     driftHz (0.02f) },
        { pid::driftAmount,   0.50f },
        { pid::driftStereo,   0.90f },
        { pid::rippleRate,    rippleHz (2.2f) },
        { pid::rippleDecay,   0.75f },
        { pid::rippleDepth,   0.22f },
        { pid::rippleCycles,  cycles (3.0f) },
        { pid::rippleSpread,  0.60f },

        { pid::dropAmount,    0.22f },
        { pid::dropDensity,   0.18f },
        { pid::dropSize,      0.62f },
        { pid::dropTone,      0.42f },
        { pid::dropSplash,    0.30f },
        { pid::dropGravity,   0.55f },
        { pid::dropBounce,    0.25f },
        { pid::dropRandom,    0.70f },
        { pid::dropSpread,    0.90f },

        { pid::resoAmount,    0.34f },
        { pid::resoSize,      0.68f },
        { pid::resoDecay,     0.62f },
        { pid::resoDamping,   0.55f },
        { pid::resoScatter,   0.35f },
        { pid::resoMotion,    0.35f },

        { pid::scurEnable,    on },
        { pid::scurAmount,    0.40f },
        { pid::scurRate,      scurHz (0.06f) },
        { pid::scurWidth,     0.60f },
        { pid::chorEnable,    on },
        { pid::chorRate,      chorHz (0.18f) },
        { pid::chorDepth,     0.50f },
        { pid::chorDelay,     chdel (22.0f) },
        { pid::chorFeedback,  0.20f },
        { pid::chorWidth,     0.70f },
        { pid::chorMix,       0.35f },
        { pid::dlyEnable,     on },
        { pid::dlyTime,       dtime (0.55f) },
        { pid::dlyFeedback,   0.42f },
        { pid::dlyMotion,     0.45f },
        { pid::dlySpread,     0.50f },
        { pid::dlyDamping,    0.68f },
        { pid::dlyDiffusion,  0.45f },
        { pid::dlyMix,        0.26f },
        { pid::diffEnable,    on },
        { pid::diffAmount,    0.55f },
        { pid::diffSize,      0.60f },
        { pid::diffDamping,   0.50f },
        { pid::diffMix,       0.32f },
        { pid::verbEnable,    on },
        { pid::verbSize,      0.80f },
        { pid::verbDecay,     0.78f },
        { pid::verbPredelay,  pre (45.0f) },
        { pid::verbDamping,   0.55f },
        { pid::verbLowCut,    vLowHz (90.0f) },
        { pid::verbHighCut,   vHighHz (6500.0f) },
        { pid::verbMod,       0.45f },
        { pid::verbMix,       0.45f },

        { pid::mastLow,       eqdb (1.0f) },
        { pid::mastMid,       eqdb (-1.0f) },
        { pid::mastHigh,      eqdb (-0.5f) },
        { pid::mastDrive,     0.12f },
        { pid::mastCeiling,   ceildb (-0.5f) },
    },
    {
        { ModSource::Drift,       ModDest::FineAll,         0.18f },
        { ModSource::Drift,       ModDest::FilterCutoff,    0.25f },
        { ModSource::Current,     ModDest::OscMix,          0.35f },
        { ModSource::Current,     ModDest::FilterCutoff,    0.30f },
        { ModSource::Tide,        ModDest::ShapeA,          0.35f },
        { ModSource::Tide,        ModDest::ResonatorSize,   0.20f },
        { ModSource::ModEnvelope, ModDest::FilterMovement,  0.30f },
        { ModSource::Velocity,    ModDest::FilterCutoff,    0.28f },
        { ModSource::ModWheel,    ModDest::ReverbMix,       0.35f },
        { ModSource::Ripple,      ModDest::PitchAll,        0.05f },
        { ModSource::KeyTrack,    ModDest::Amplitude,      -0.12f },
        { ModSource::Aftertouch,  ModDest::DropletAmount,   0.40f },
    }));

    // --- DRY AQUATIC #6 ----------------------------------------------------
    out.push_back (make (kDeepBlue, "SUBMERGED", "Pad", { "Submerged", "Deep", "Wet" }, Dry::Yes,
        "The proof that the reverb is optional. Everything muffled, everything "
        "moving: a low LP24 with heavy pressure, Deep noise breathing under the "
        "Tide, and a large slow resonator standing in for the room.",
    {
        RIPPLES_FX_BYPASSED,

        { pid::oscAWave,      wv (OscWave::Water) },
        { pid::oscAShape,     0.35f },
        { pid::oscAUnison,    uni (6) },
        { pid::oscADetune,    0.26f },
        { pid::oscAStereo,    0.95f },
        { pid::oscALevel,     lvl (-3.5f) },

        { pid::oscBWave,      wv (OscWave::Hollow) },
        { pid::oscBOctave,    oct (-1) },
        { pid::oscBFine,      cents (-11.0f) },
        { pid::oscBUnison,    uni (4) },
        { pid::oscBDetune,    0.22f },
        { pid::oscBStereo,    0.90f },
        { pid::oscBLevel,     lvl (-8.0f) },

        { pid::subWave,       sw (SubWave::Sine) },
        { pid::subLevel,      lvl (-13.0f) },
        { pid::noiseType,     nz (NoiseType::Deep) },
        { pid::noiseLevel,    lvl (-21.0f) },
        { pid::noiseTone,     0.22f },

        { pid::filtMode,      fm (FilterMode::LP24) },
        { pid::filtCutoff,    hz (480.0f) },
        { pid::filtReso,      0.34f },
        { pid::filtDrive,     0.24f },
        { pid::filtKeyTrack,  0.30f },
        { pid::filtEnvAmt,    bip (0.38f) },
        { pid::filtPressure,  0.70f },

        { pid::fenvAttack,    att (1.80f) },
        { pid::fenvDecay,     dec (5.00f) },
        { pid::fenvSustain,   0.50f },
        { pid::fenvRelease,   rel (6.00f) },
        { pid::fenvVelocity,  0.30f },

        { pid::aenvAttack,    att (1.40f) },
        { pid::aenvDecay,     dec (4.00f) },
        { pid::aenvSustain,   0.88f },
        { pid::aenvRelease,   rel (6.50f) },
        { pid::aenvVelocity,  0.30f },

        { pid::macroDepth,    0.85f },
        { pid::macroWet,      0.70f },
        { pid::macroCurrent,  0.55f },
        { pid::macroPressure, 0.70f },
        { pid::macroDrops,    0.30f },
        { pid::macroGlow,     0.20f },
        { pid::fluidX,        bip (-0.45f) },
        { pid::fluidY,        bip (0.80f) },
        { pid::fluidYAmount,  0.85f },

        { pid::tideShape,     ts (TideShape::Swell) },
        { pid::tideRate,      tideHz (0.09f) },
        { pid::tideDepth,     0.60f },
        { pid::tideStereo,    0.70f },
        { pid::currRate,      currHz (0.16f) },
        { pid::currAmount,    0.62f },
        { pid::currSmooth,    0.85f },
        { pid::currDrift,     0.45f },
        { pid::currStereo,    0.90f },
        { pid::driftRate,     driftHz (0.015f) },
        { pid::driftAmount,   0.60f },
        { pid::driftStereo,   0.95f },
        { pid::rippleRate,    rippleHz (1.4f) },
        { pid::rippleDecay,   0.85f },
        { pid::rippleDepth,   0.30f },
        { pid::rippleCycles,  cycles (2.0f) },

        { pid::dropAmount,    0.26f },
        { pid::dropDensity,   0.14f },
        { pid::dropSize,      0.78f },
        { pid::dropTone,      0.24f },
        { pid::dropSplash,    0.20f },
        { pid::dropGravity,   0.45f },
        { pid::dropBounce,    0.15f },
        { pid::dropRandom,    0.75f },
        { pid::dropSpread,    0.95f },

        { pid::resoAmount,    0.58f },
        { pid::resoSize,      0.88f },
        { pid::resoDecay,     0.80f },
        { pid::resoDamping,   0.72f },
        { pid::resoScatter,   0.45f },
        { pid::resoMotion,    0.45f },

        { pid::mastLow,       eqdb (2.0f) },
        { pid::mastHigh,      eqdb (-3.0f) },
        { pid::mastDrive,     0.15f },
    },
    {
        { ModSource::Tide,     ModDest::NoiseLevel,      0.45f },
        { ModSource::Tide,     ModDest::FilterCutoff,    0.30f },
        { ModSource::Current,  ModDest::FilterCutoff,    0.35f },
        { ModSource::Current,  ModDest::ResonatorSize,   0.30f },
        { ModSource::Drift,    ModDest::FineAll,         0.25f },
        { ModSource::Drift,    ModDest::ResonatorDamping, 0.30f },
        { ModSource::Ripple,   ModDest::FilterCutoff,    0.20f },
        { ModSource::ModWheel, ModDest::FilterCutoff,    0.40f },
        { ModSource::Velocity, ModDest::FilterCutoff,    0.20f },
        { ModSource::Aftertouch, ModDest::ResonatorAmount, 0.35f },
    }));

    out.push_back (make (kDeepBlue, "BLUE CATHEDRAL", "Pad", { "Cinematic", "Dreamy", "Deep" }, Dry::No,
        "Enormous. A slow choir of Water and Glass an octave apart, a 45-second "
        "reverb tail and a 90 ms pre-delay so the attack is still audible before "
        "the room swallows it. Play three notes and leave them.",
    {
        { pid::oscAWave,      wv (OscWave::Water) },
        { pid::oscAShape,     0.50f },
        { pid::oscAUnison,    uni (7) },
        { pid::oscADetune,    0.30f },
        { pid::oscAStereo,    1.00f },
        { pid::oscALevel,     lvl (-5.0f) },

        { pid::oscBWave,      wv (OscWave::Glass) },
        { pid::oscBOctave,    oct (1) },
        { pid::oscBShape,     0.45f },
        { pid::oscBFine,      cents (5.0f) },
        { pid::oscBUnison,    uni (4) },
        { pid::oscBDetune,    0.26f },
        { pid::oscBStereo,    0.95f },
        { pid::oscBLevel,     lvl (-13.0f) },

        { pid::subLevel,      lvl (-16.0f) },
        { pid::noiseType,     nz (NoiseType::Air) },
        { pid::noiseLevel,    lvl (-28.0f) },
        { pid::noiseTone,     0.55f },

        { pid::filtMode,      fm (FilterMode::LP12) },
        { pid::filtCutoff,    hz (2200.0f) },
        { pid::filtReso,      0.20f },
        { pid::filtDrive,     0.10f },
        { pid::filtKeyTrack,  0.40f },
        { pid::filtEnvAmt,    bip (0.32f) },
        { pid::filtPressure,  0.40f },

        { pid::fenvAttack,    att (3.50f) },
        { pid::fenvDecay,     dec (8.00f) },
        { pid::fenvSustain,   0.65f },
        { pid::fenvRelease,   rel (9.00f) },

        { pid::aenvAttack,    att (2.80f) },
        { pid::aenvDecay,     dec (6.00f) },
        { pid::aenvSustain,   0.90f },
        { pid::aenvRelease,   rel (8.00f) },
        { pid::aenvVelocity,  0.25f },

        { pid::macroDepth,    0.55f },
        { pid::macroWet,      0.60f },
        { pid::macroSpace,    0.95f },
        { pid::macroGlow,     0.55f },
        { pid::macroCurrent,  0.40f },
        { pid::fluidX,        bip (-0.55f) },
        { pid::fluidY,        bip (0.35f) },

        { pid::tideShape,     ts (TideShape::Flow) },
        { pid::tideRate,      tideHz (0.07f) },
        { pid::tideDepth,     0.40f },
        { pid::tideStereo,    0.80f },
        { pid::currRate,      currHz (0.12f) },
        { pid::currAmount,    0.45f },
        { pid::currSmooth,    0.88f },
        { pid::driftRate,     driftHz (0.012f) },
        { pid::driftAmount,   0.55f },
        { pid::driftStereo,   0.95f },

        { pid::resoAmount,    0.28f },
        { pid::resoSize,      0.82f },
        { pid::resoDecay,     0.70f },
        { pid::resoDamping,   0.48f },
        { pid::resoScatter,   0.30f },
        { pid::resoMotion,    0.30f },

        { pid::scurEnable,    on },
        { pid::scurAmount,    0.45f },
        { pid::scurRate,      scurHz (0.04f) },
        { pid::scurWidth,     0.70f },
        { pid::chorEnable,    on },
        { pid::chorRate,      chorHz (0.12f) },
        { pid::chorDepth,     0.55f },
        { pid::chorDelay,     chdel (28.0f) },
        { pid::chorWidth,     0.80f },
        { pid::chorMix,       0.38f },
        { pid::dlyEnable,     on },
        { pid::dlyTime,       dtime (1.20f) },
        { pid::dlyFeedback,   0.52f },
        { pid::dlyMotion,     0.50f },
        { pid::dlySpread,     0.70f },
        { pid::dlyDamping,    0.72f },
        { pid::dlyDiffusion,  0.60f },
        { pid::dlyMix,        0.28f },
        { pid::diffEnable,    on },
        { pid::diffAmount,    0.70f },
        { pid::diffSize,      0.75f },
        { pid::diffMix,       0.40f },
        { pid::verbEnable,    on },
        { pid::verbSize,      0.95f },
        { pid::verbDecay,     0.92f },
        { pid::verbPredelay,  pre (90.0f) },
        { pid::verbDamping,   0.45f },
        { pid::verbLowCut,    vLowHz (140.0f) },
        { pid::verbHighCut,   vHighHz (7500.0f) },
        { pid::verbMod,       0.55f },
        { pid::verbMix,       0.58f },

        { pid::mastLow,       eqdb (-1.0f) },
        { pid::mastHigh,      eqdb (1.0f) },
        { pid::mastCeiling,   ceildb (-0.8f) },
    },
    {
        { ModSource::Drift,       ModDest::FineAll,       0.22f },
        { ModSource::Drift,       ModDest::ReverbSize,    0.25f },
        { ModSource::Tide,        ModDest::ShapeA,        0.40f },
        { ModSource::Tide,        ModDest::LevelB,        0.30f },
        { ModSource::Current,     ModDest::FilterCutoff,  0.30f },
        { ModSource::Current,     ModDest::DiffusionAmount, 0.25f },
        { ModSource::ModEnvelope, ModDest::ShapeB,        0.35f },
        { ModSource::ModWheel,    ModDest::ReverbMix,     0.40f },
        { ModSource::Note,        ModDest::FilterCutoff,  0.20f },
    }));

    out.push_back (make (kDeepBlue, "THERMOCLINE", "Pad", { "Deep", "Calm", "Submerged" }, Dry::No,
        "The boundary layer, where warm water meets cold. The Morph filter walks "
        "from low-pass to band-pass under Drift, so the timbre crosses over and "
        "back without you touching anything.",
    {
        { pid::oscAWave,      wv (OscWave::Hollow) },
        { pid::oscAShape,     0.38f },
        { pid::oscAUnison,    uni (4) },
        { pid::oscADetune,    0.20f },
        { pid::oscAStereo,    0.85f },
        { pid::oscALevel,     lvl (-4.0f) },

        { pid::oscBWave,      wv (OscWave::Water) },
        { pid::oscBOctave,    oct (-1) },
        { pid::oscBShape,     0.60f },
        { pid::oscBFine,      cents (9.0f) },
        { pid::oscBUnison,    uni (3) },
        { pid::oscBDetune,    0.24f },
        { pid::oscBStereo,    0.90f },
        { pid::oscBLevel,     lvl (-6.5f) },
        { pid::oscBInterMode, im (InteractionMode::Crossfade) },
        { pid::oscBInterAmt,  0.45f },

        { pid::subLevel,      lvl (-15.0f) },
        { pid::noiseType,     nz (NoiseType::Deep) },
        { pid::noiseLevel,    lvl (-27.0f) },
        { pid::noiseTone,     0.30f },

        { pid::filtMode,      fm (FilterMode::Morph) },
        { pid::filtCutoff,    hz (1100.0f) },
        { pid::filtReso,      0.30f },
        { pid::filtDrive,     0.16f },
        { pid::filtKeyTrack,  0.40f },
        { pid::filtEnvAmt,    bip (0.25f) },
        { pid::filtMovement,  0.30f },
        { pid::filtPressure,  0.50f },

        { pid::fenvAttack,    att (2.00f) },
        { pid::fenvDecay,     dec (5.00f) },
        { pid::fenvSustain,   0.55f },
        { pid::fenvRelease,   rel (5.00f) },

        { pid::aenvAttack,    att (1.60f) },
        { pid::aenvDecay,     dec (4.00f) },
        { pid::aenvSustain,   0.85f },
        { pid::aenvRelease,   rel (5.00f) },
        { pid::aenvVelocity,  0.30f },

        { pid::macroDepth,    0.65f },
        { pid::macroWet,      0.50f },
        { pid::macroCurrent,  0.60f },
        { pid::macroPressure, 0.50f },
        { pid::macroSpace,    0.55f },
        { pid::fluidX,        bip (-0.40f) },
        { pid::fluidY,        bip (0.45f) },

        { pid::tideShape,     ts (TideShape::Swell) },
        { pid::tideRate,      tideHz (0.11f) },
        { pid::tideDepth,     0.50f },
        { pid::tideStereo,    0.60f },
        { pid::currRate,      currHz (0.20f) },
        { pid::currAmount,    0.55f },
        { pid::currSmooth,    0.80f },
        { pid::currDrift,     0.50f },
        { pid::driftRate,     driftHz (0.008f) },
        { pid::driftAmount,   0.70f },
        { pid::driftStereo,   0.85f },

        { pid::dropAmount,    0.18f },
        { pid::dropDensity,   0.16f },
        { pid::dropSize,      0.70f },
        { pid::dropTone,      0.35f },
        { pid::dropSpread,    0.85f },

        { pid::resoAmount,    0.36f },
        { pid::resoSize,      0.72f },
        { pid::resoDecay,     0.66f },
        { pid::resoDamping,   0.62f },
        { pid::resoMotion,    0.40f },

        { pid::scurEnable,    on },
        { pid::scurAmount,    0.40f },
        { pid::chorEnable,    on },
        { pid::chorRate,      chorHz (0.15f) },
        { pid::chorDepth,     0.48f },
        { pid::chorMix,       0.30f },
        { pid::dlyEnable,     on },
        { pid::dlyTime,       dtime (0.85f) },
        { pid::dlyFeedback,   0.40f },
        { pid::dlyDamping,    0.72f },
        { pid::dlyMix,        0.22f },
        { pid::diffEnable,    on },
        { pid::diffMix,       0.30f },
        { pid::verbEnable,    on },
        { pid::verbSize,      0.78f },
        { pid::verbDecay,     0.74f },
        { pid::verbDamping,   0.60f },
        { pid::verbMix,       0.40f },
        { pid::mastLow,       eqdb (1.0f) },
    },
    {
        { ModSource::Drift,       ModDest::FilterMovement,    0.60f },
        { ModSource::Drift,       ModDest::FineAll,           0.20f },
        { ModSource::Current,     ModDest::InteractionAmount, 0.45f },
        { ModSource::Current,     ModDest::FilterCutoff,      0.30f },
        { ModSource::Tide,        ModDest::FilterCutoff,      0.35f },
        { ModSource::Tide,        ModDest::OscMix,            0.25f },
        { ModSource::ModEnvelope, ModDest::ResonatorSize,     0.25f },
        { ModSource::ModWheel,    ModDest::FilterMovement,    0.40f },
    }));

    out.push_back (make (kDeepBlue, "WHALEFALL", "Drone", { "Deep", "Dark", "Cinematic" }, Dry::No,
        "A hold-one-note drone. Sub-heavy, slow, with a long resonator and a "
        "Swell tide that makes the whole thing breathe once every twenty "
        "seconds. Nothing here is fast.",
    {
        { pid::oscAWave,      wv (OscWave::Hollow) },
        { pid::oscAShape,     0.30f },
        { pid::oscAOctave,    oct (-1) },
        { pid::oscAUnison,    uni (5) },
        { pid::oscADetune,    0.28f },
        { pid::oscAStereo,    0.90f },
        { pid::oscALevel,     lvl (-5.0f) },

        { pid::oscBWave,      wv (OscWave::Water) },
        { pid::oscBOctave,    oct (-2) },
        { pid::oscBFine,      cents (-14.0f) },
        { pid::oscBUnison,    uni (3) },
        { pid::oscBDetune,    0.30f },
        { pid::oscBLevel,     lvl (-7.0f) },

        { pid::subWave,       sw (SubWave::Sine) },
        { pid::subOctave,     subOct (-2) },
        { pid::subLevel,      lvl (-9.0f) },
        { pid::noiseType,     nz (NoiseType::Deep) },
        { pid::noiseLevel,    lvl (-23.0f) },
        { pid::noiseTone,     0.12f },

        { pid::filtMode,      fm (FilterMode::LP24) },
        { pid::filtCutoff,    hz (320.0f) },
        { pid::filtReso,      0.26f },
        { pid::filtDrive,     0.28f },
        { pid::filtKeyTrack,  0.25f },
        { pid::filtEnvAmt,    bip (0.20f) },
        { pid::filtPressure,  0.65f },

        { pid::fenvAttack,    att (5.00f) },
        { pid::fenvDecay,     dec (12.00f) },
        { pid::fenvSustain,   0.70f },
        { pid::fenvRelease,   rel (10.00f) },

        { pid::aenvAttack,    att (4.00f) },
        { pid::aenvDecay,     dec (10.00f) },
        { pid::aenvSustain,   0.95f },
        { pid::aenvRelease,   rel (12.00f) },
        { pid::aenvVelocity,  0.15f },

        { pid::macroDepth,    0.90f },
        { pid::macroPressure, 0.70f },
        { pid::macroWet,      0.45f },
        { pid::macroSpace,    0.75f },
        { pid::macroGlow,     0.10f },
        { pid::fluidX,        bip (-0.60f) },
        { pid::fluidY,        bip (0.85f) },

        { pid::tideShape,     ts (TideShape::Swell) },
        { pid::tideRate,      tideHz (0.05f) },
        { pid::tideDepth,     0.65f },
        { pid::tideStereo,    0.55f },
        { pid::currRate,      currHz (0.09f) },
        { pid::currAmount,    0.50f },
        { pid::currSmooth,    0.90f },
        { pid::driftRate,     driftHz (0.006f) },
        { pid::driftAmount,   0.65f },

        { pid::resoAmount,    0.42f },
        { pid::resoSize,      0.95f },
        { pid::resoDecay,     0.85f },
        { pid::resoDamping,   0.75f },
        { pid::resoScatter,   0.25f },
        { pid::resoMotion,    0.35f },

        { pid::scurEnable,    on },
        { pid::scurAmount,    0.35f },
        { pid::scurRate,      scurHz (0.03f) },
        { pid::dlyEnable,     on },
        { pid::dlyTime,       dtime (1.60f) },
        { pid::dlyFeedback,   0.45f },
        { pid::dlyDamping,    0.85f },
        { pid::dlyMix,        0.20f },
        { pid::diffEnable,    on },
        { pid::diffAmount,    0.50f },
        { pid::diffMix,       0.26f },
        { pid::verbEnable,    on },
        { pid::verbSize,      0.90f },
        { pid::verbDecay,     0.88f },
        { pid::verbDamping,   0.70f },
        { pid::verbLowCut,    vLowHz (60.0f) },
        { pid::verbHighCut,   vHighHz (3500.0f) },
        { pid::verbMix,       0.42f },

        { pid::mastLow,       eqdb (3.0f) },
        { pid::mastHigh,      eqdb (-4.0f) },
        { pid::mastDrive,     0.20f },
        { pid::voiceCount,    vox (4) },
    },
    {
        { ModSource::Tide,     ModDest::Amplitude,       0.30f },
        { ModSource::Tide,     ModDest::FilterCutoff,    0.35f },
        { ModSource::Drift,    ModDest::FineAll,         0.30f },
        { ModSource::Drift,    ModDest::ResonatorDecay,  0.25f },
        { ModSource::Current,  ModDest::NoiseTone,       0.35f },
        { ModSource::Current,  ModDest::PanA,            0.40f },
        { ModSource::ModWheel, ModDest::ResonatorAmount, 0.45f },
    }));

    out.push_back (make (kDeepBlue, "MIDWATER", "Key", { "Deep", "Dreamy", "Wet" }, Dry::No,
        "A soft electric-piano shape for chords in the middle of a track. Fast "
        "attack, long tail, low resonance, nothing sharp anywhere.",
    {
        { pid::oscAWave,      wv (OscWave::Sine) },
        { pid::oscAUnison,    uni (3) },
        { pid::oscADetune,    0.14f },
        { pid::oscAStereo,    0.70f },
        { pid::oscALevel,     lvl (-3.0f) },

        { pid::oscBWave,      wv (OscWave::Water) },
        { pid::oscBOctave,    oct (1) },
        { pid::oscBShape,     0.40f },
        { pid::oscBLevel,     lvl (-12.0f) },
        { pid::oscBInterMode, im (InteractionMode::PhaseMod) },
        { pid::oscBInterAmt,  0.22f },

        { pid::subLevel,      lvl (-16.0f) },
        { pid::noiseType,     nz (NoiseType::Bubble) },
        { pid::noiseLevel,    lvl (-29.0f) },
        { pid::noiseTone,     0.40f },

        { pid::filtMode,      fm (FilterMode::LP12) },
        { pid::filtCutoff,    hz (1900.0f) },
        { pid::filtReso,      0.18f },
        { pid::filtDrive,     0.14f },
        { pid::filtKeyTrack,  0.60f },
        { pid::filtEnvAmt,    bip (0.45f) },
        { pid::filtPressure,  0.40f },

        { pid::fenvAttack,    att (0.004f) },
        { pid::fenvDecay,     dec (1.60f) },
        { pid::fenvSustain,   0.20f },
        { pid::fenvRelease,   rel (1.50f) },
        { pid::fenvVelocity,  0.55f },

        { pid::aenvAttack,    att (0.010f) },
        { pid::aenvDecay,     dec (3.00f) },
        { pid::aenvSustain,   0.45f },
        { pid::aenvRelease,   rel (2.20f) },
        { pid::aenvVelocity,  0.65f },

        { pid::macroDepth,    0.50f },
        { pid::macroWet,      0.50f },
        { pid::macroSpace,    0.55f },
        { pid::macroGlow,     0.35f },
        { pid::fluidX,        bip (-0.30f) },
        { pid::fluidY,        bip (0.30f) },

        { pid::tideRate,      tideHz (0.30f) },
        { pid::tideDepth,     0.25f },
        { pid::tideStereo,    0.55f },
        { pid::currRate,      currHz (0.35f) },
        { pid::currAmount,    0.40f },
        { pid::currSmooth,    0.75f },
        { pid::driftRate,     driftHz (0.03f) },
        { pid::driftAmount,   0.35f },

        { pid::dropAmount,    0.16f },
        { pid::dropDensity,   0.20f },
        { pid::dropSize,      0.55f },
        { pid::dropTone,      0.50f },

        { pid::resoAmount,    0.26f },
        { pid::resoSize,      0.60f },
        { pid::resoDecay,     0.52f },
        { pid::resoDamping,   0.58f },

        { pid::scurEnable,    on },
        { pid::chorEnable,    on },
        { pid::chorRate,      chorHz (0.30f) },
        { pid::chorDepth,     0.42f },
        { pid::chorMix,       0.30f },
        { pid::dlyEnable,     on },
        { pid::dlySync,       on },
        { pid::dlySyncTime,   sd (SyncDivision::QuarterD) },
        { pid::dlyFeedback,   0.35f },
        { pid::dlyDamping,    0.72f },
        { pid::dlyMix,        0.22f },
        { pid::diffEnable,    on },
        { pid::diffMix,       0.26f },
        { pid::verbEnable,    on },
        { pid::verbSize,      0.68f },
        { pid::verbDecay,     0.66f },
        { pid::verbMix,       0.38f },
    },
    {
        { ModSource::Velocity,    ModDest::FilterCutoff,      0.45f },
        { ModSource::Velocity,    ModDest::InteractionAmount, 0.30f },
        { ModSource::ModEnvelope, ModDest::ShapeB,            0.30f },
        { ModSource::Current,     ModDest::OscMix,            0.30f },
        { ModSource::Drift,       ModDest::FineAll,           0.12f },
        { ModSource::Tide,        ModDest::ResonatorSize,     0.22f },
        { ModSource::Aftertouch,  ModDest::FilterCutoff,      0.35f },
    }));
}

//==============================================================================
//  THE ABYSS — no light. Sub weight, closed filters, enormous slow spaces.
//==============================================================================
void addAbyssBank (std::vector<Preset>& out)
{
    // --- DRY AQUATIC #7 ----------------------------------------------------
    out.push_back (make (kAbyss, "ABYSS BASS", "Bass", { "Deep", "Dark", "Submerged" }, Dry::Yes,
        "A PLAYED bass, not a drone: mono legato, full key tracking so it stays "
        "in tune up the neck, a tight mod envelope on a resonant LP24 at 180 Hz, "
        "and a note-mode droplet per attack for the gulp. Compare OCEAN FLOOR, "
        "which is the same depth with none of the articulation.",
    {
        RIPPLES_FX_BYPASSED,

        { pid::oscAWave,      wv (OscWave::Hollow) },
        { pid::oscAShape,     0.28f },
        { pid::oscAOctave,    oct (-1) },
        { pid::oscAUnison,    uni (1) },
        { pid::oscAStereo,    0.20f },
        { pid::oscALevel,     lvl (-2.0f) },
        { pid::oscAPhase,     startPhase (0.0f) },   // identical attack every note

        { pid::oscBWave,      wv (OscWave::Water) },
        { pid::oscBOctave,    oct (-1) },
        { pid::oscBShape,     0.35f },
        { pid::oscBFine,      cents (-8.0f) },
        { pid::oscBLevel,     lvl (-8.0f) },
        { pid::oscBStereo,    0.25f },
        { pid::oscBPhase,     startPhase (0.0f) },

        { pid::subWave,       sw (SubWave::Triangle) },
        { pid::subOctave,     subOct (-1) },
        { pid::subLevel,      lvl (-5.0f) },
        { pid::noiseType,     nz (NoiseType::Deep) },
        { pid::noiseLevel,    lvl (-24.0f) },
        { pid::noiseTone,     0.18f },

        { pid::filtMode,      fm (FilterMode::LP24) },
        { pid::filtCutoff,    hz (180.0f) },
        { pid::filtReso,      0.55f },
        { pid::filtDrive,     0.35f },
        { pid::filtKeyTrack,  1.00f },
        { pid::filtEnvAmt,    bip (0.62f) },
        { pid::filtPressure,  0.60f },

        { pid::fenvAttack,    att (0.001f) },
        { pid::fenvDecay,     dec (0.38f) },
        { pid::fenvSustain,   0.15f },
        { pid::fenvRelease,   rel (0.30f) },
        { pid::fenvVelocity,  0.70f },

        { pid::aenvAttack,    att (0.003f) },
        { pid::aenvDecay,     dec (1.20f) },
        { pid::aenvSustain,   0.70f },
        { pid::aenvRelease,   rel (0.28f) },
        { pid::aenvVelocity,  0.55f },

        { pid::macroDepth,    0.90f },
        { pid::macroPressure, 0.65f },
        { pid::macroWet,      0.40f },
        { pid::macroDrops,    0.35f },
        { pid::macroRipple,   0.45f },
        { pid::macroGlow,     0.10f },
        { pid::fluidX,        bip (-0.20f) },
        { pid::fluidY,        bip (0.75f) },
        { pid::fluidYAmount,  0.70f },

        { pid::tideRate,      tideHz (0.40f) },
        { pid::tideDepth,     0.14f },
        { pid::tideStereo,    0.20f },
        { pid::currRate,      currHz (0.55f) },
        { pid::currAmount,    0.28f },
        { pid::currSmooth,    0.70f },
        { pid::currStereo,    0.30f },
        { pid::driftRate,     driftHz (0.03f) },
        { pid::driftAmount,   0.20f },
        { pid::rippleRate,    rippleHz (5.5f) },
        { pid::rippleDecay,   0.78f },
        { pid::rippleDepth,   0.48f },
        { pid::rippleCycles,  cycles (3.0f) },
        { pid::rippleTrigger, rtg (RippleTrigger::NoteOn) },

        { pid::dropAmount,    0.30f },
        { pid::dropMode,      dm (DropletMode::Note) },
        { pid::dropDensity,   0.22f },
        { pid::dropSize,      0.85f },
        { pid::dropTone,      0.15f },
        { pid::dropSplash,    0.25f },
        { pid::dropGravity,   0.70f },
        { pid::dropBounce,    0.20f },
        { pid::dropRandom,    0.25f },
        { pid::dropSpread,    0.40f },

        { pid::resoAmount,    0.24f },
        { pid::resoSize,      0.30f },
        { pid::resoDecay,     0.28f },
        { pid::resoDamping,   0.82f },
        { pid::resoScatter,   0.15f },
        { pid::resoMotion,    0.10f },

        { pid::mastLow,       eqdb (2.5f) },
        { pid::mastMid,       eqdb (-1.5f) },
        { pid::mastHigh,      eqdb (-3.0f) },
        { pid::mastDrive,     0.24f },
        { pid::mastCeiling,   ceildb (-0.5f) },

        { pid::voiceCount,    vox (1) },
        { pid::glideMode,     gmd (GlideMode::Legato) },
        { pid::glideTime,     gtime (0.05f) },
    },
    {
        { ModSource::Ripple,      ModDest::FilterCutoff,  0.40f },
        { ModSource::Ripple,      ModDest::DropletTone,   0.25f },
        { ModSource::Velocity,    ModDest::FilterCutoff,  0.45f },
        { ModSource::Velocity,    ModDest::FilterDrive,   0.30f },
        { ModSource::Velocity,    ModDest::DropletAmount, 0.40f },
        { ModSource::ModEnvelope, ModDest::NoiseLevel,    0.45f },
        { ModSource::ModEnvelope, ModDest::ShapeA,        0.30f },
        { ModSource::Current,     ModDest::FilterCutoff,  0.15f },
        { ModSource::ModWheel,    ModDest::FilterResonance, 0.30f },
        { ModSource::KeyTrack,    ModDest::SubLevel,     -0.35f },
    }));

    // --- DRY AQUATIC #8 ----------------------------------------------------
    out.push_back (make (kAbyss, "OCEAN FLOOR", "Drone", { "Deep", "Dark", "Cinematic" }, Dry::Yes,
        "Mass and stillness. No attack, no articulation, no effects — the room "
        "is the resonator, held at maximum size and decay with heavy damping, "
        "and the only motion is Drift moving a nearly closed filter. The "
        "opposite pole to ABYSS BASS.",
    {
        RIPPLES_FX_BYPASSED,

        { pid::oscAWave,      wv (OscWave::Water) },
        { pid::oscAShape,     0.22f },
        { pid::oscAOctave,    oct (-2) },
        { pid::oscAUnison,    uni (6) },
        { pid::oscADetune,    0.34f },
        { pid::oscAStereo,    0.95f },
        { pid::oscALevel,     lvl (-5.0f) },

        { pid::oscBWave,      wv (OscWave::Hollow) },
        { pid::oscBOctave,    oct (-2) },
        { pid::oscBSemitone,  semi (7) },
        { pid::oscBFine,      cents (-17.0f) },
        { pid::oscBUnison,    uni (4) },
        { pid::oscBDetune,    0.30f },
        { pid::oscBStereo,    0.95f },
        { pid::oscBLevel,     lvl (-11.0f) },

        { pid::subWave,       sw (SubWave::Sine) },
        { pid::subOctave,     subOct (-2) },
        { pid::subLevel,      lvl (-8.0f) },
        { pid::noiseType,     nz (NoiseType::Deep) },
        { pid::noiseLevel,    lvl (-18.0f) },
        { pid::noiseTone,     0.06f },

        { pid::filtMode,      fm (FilterMode::LP24) },
        { pid::filtCutoff,    hz (150.0f) },
        { pid::filtReso,      0.18f },
        { pid::filtDrive,     0.22f },
        { pid::filtKeyTrack,  0.15f },
        { pid::filtEnvAmt,    bip (0.12f) },
        { pid::filtPressure,  0.85f },

        { pid::fenvAttack,    att (6.00f) },
        { pid::fenvDecay,     dec (15.00f) },
        { pid::fenvSustain,   0.80f },
        { pid::fenvRelease,   rel (12.00f) },
        { pid::fenvVelocity,  0.10f },

        { pid::aenvAttack,    att (5.00f) },
        { pid::aenvDecay,     dec (12.00f) },
        { pid::aenvSustain,   1.00f },
        { pid::aenvRelease,   rel (14.00f) },
        { pid::aenvVelocity,  0.10f },

        { pid::macroDepth,    1.00f },
        { pid::macroPressure, 0.85f },
        { pid::macroWet,      0.55f },
        { pid::macroCurrent,  0.35f },
        { pid::macroDrops,    0.20f },
        { pid::macroGlow,     0.00f },
        { pid::fluidX,        bip (-0.70f) },
        { pid::fluidY,        bip (1.00f) },
        { pid::fluidYAmount,  0.90f },

        { pid::tideShape,     ts (TideShape::Swell) },
        { pid::tideRate,      tideHz (0.03f) },
        { pid::tideDepth,     0.55f },
        { pid::tideStereo,    0.50f },
        { pid::currRate,      currHz (0.06f) },
        { pid::currAmount,    0.40f },
        { pid::currSmooth,    0.95f },
        { pid::currDrift,     0.60f },
        { pid::currStereo,    0.85f },
        { pid::driftRate,     driftHz (0.004f) },
        { pid::driftAmount,   0.75f },
        { pid::driftStereo,   0.90f },
        { pid::rippleDepth,   0.00f },

        { pid::dropAmount,    0.20f },
        { pid::dropDensity,   0.08f },
        { pid::dropSize,      0.95f },
        { pid::dropTone,      0.10f },
        { pid::dropSplash,    0.15f },
        { pid::dropGravity,   0.30f },
        { pid::dropBounce,    0.10f },
        { pid::dropRandom,    0.80f },
        { pid::dropSpread,    0.90f },

        { pid::resoAmount,    0.50f },
        { pid::resoSize,      1.00f },
        { pid::resoDecay,     0.92f },
        { pid::resoDamping,   0.85f },
        { pid::resoScatter,   0.35f },
        { pid::resoMotion,    0.25f },

        { pid::mastLow,       eqdb (3.5f) },
        { pid::mastMid,       eqdb (-2.0f) },
        { pid::mastHigh,      eqdb (-6.0f) },
        { pid::mastDrive,     0.18f },
        { pid::voiceCount,    vox (6) },
    },
    {
        { ModSource::Drift,   ModDest::FilterCutoff,   0.45f },
        { ModSource::Drift,   ModDest::FineAll,        0.30f },
        { ModSource::Drift,   ModDest::ResonatorSize,  0.25f },
        { ModSource::Tide,    ModDest::Amplitude,      0.22f },
        { ModSource::Tide,    ModDest::NoiseLevel,     0.40f },
        { ModSource::Current, ModDest::NoiseTone,      0.30f },
        { ModSource::Current, ModDest::PanA,           0.45f },
        { ModSource::Current, ModDest::ResonatorDamping, 0.25f },
        { ModSource::ModWheel, ModDest::FilterCutoff,  0.35f },
    }));

    out.push_back (make (kAbyss, "PRESSURE", "Bass", { "Dark", "Deep", "Chaotic" }, Dry::No,
        "Hard-synced and driven. Osc B is the master sync source, so the shape "
        "control becomes a formant sweep instead of a PWM — an aggressive bass "
        "that still lives below the waterline.",
    {
        { pid::oscAWave,      wv (OscWave::Saw) },
        { pid::oscAShape,     0.45f },
        { pid::oscAOctave,    oct (-1) },
        { pid::oscAUnison,    uni (2) },
        { pid::oscADetune,    0.10f },
        { pid::oscAStereo,    0.35f },
        { pid::oscALevel,     lvl (-3.0f) },
        { pid::oscAPhase,     startPhase (0.0f) },

        { pid::oscBWave,      wv (OscWave::Shark) },
        { pid::oscBOctave,    oct (-1) },
        { pid::oscBShape,     0.40f },
        { pid::oscBLevel,     lvl (-10.0f) },
        { pid::oscBInterMode, im (InteractionMode::HardSync) },
        { pid::oscBInterAmt,  0.55f },

        { pid::subWave,       sw (SubWave::Square) },
        { pid::subLevel,      lvl (-7.0f) },
        { pid::noiseType,     nz (NoiseType::Deep) },
        { pid::noiseLevel,    lvl (-26.0f) },
        { pid::noiseTone,     0.25f },

        { pid::filtMode,      fm (FilterMode::LP24) },
        { pid::filtCutoff,    hz (260.0f) },
        { pid::filtReso,      0.48f },
        { pid::filtDrive,     0.55f },
        { pid::filtKeyTrack,  0.90f },
        { pid::filtEnvAmt,    bip (0.70f) },
        { pid::filtPressure,  0.75f },

        { pid::fenvAttack,    att (0.001f) },
        { pid::fenvDecay,     dec (0.24f) },
        { pid::fenvSustain,   0.10f },
        { pid::fenvRelease,   rel (0.22f) },
        { pid::fenvVelocity,  0.75f },

        { pid::aenvAttack,    att (0.002f) },
        { pid::aenvDecay,     dec (0.80f) },
        { pid::aenvSustain,   0.65f },
        { pid::aenvRelease,   rel (0.20f) },
        { pid::aenvVelocity,  0.60f },

        { pid::macroDepth,    0.80f },
        { pid::macroPressure, 0.85f },
        { pid::macroRipple,   0.35f },
        { pid::macroWet,      0.25f },
        { pid::fluidX,        bip (0.40f) },
        { pid::fluidY,        bip (0.65f) },

        { pid::tideRate,      tideHz (0.6f) },
        { pid::tideDepth,     0.20f },
        { pid::currRate,      currHz (1.1f) },
        { pid::currAmount,    0.35f },
        { pid::currSmooth,    0.40f },
        { pid::rippleRate,    rippleHz (8.0f) },
        { pid::rippleDecay,   0.80f },
        { pid::rippleDepth,   0.40f },

        { pid::resoAmount,    0.18f },
        { pid::resoSize,      0.26f },
        { pid::resoDecay,     0.24f },
        { pid::resoDamping,   0.80f },

        { pid::scurEnable,    on },
        { pid::scurAmount,    0.18f },
        { pid::scurWidth,     0.35f },
        { pid::diffEnable,    on },
        { pid::diffAmount,    0.30f },
        { pid::diffMix,       0.16f },
        { pid::verbEnable,    on },
        { pid::verbSize,      0.40f },
        { pid::verbDecay,     0.35f },
        { pid::verbLowCut,    vLowHz (400.0f) },
        { pid::verbMix,       0.14f },

        { pid::mastLow,       eqdb (2.0f) },
        { pid::mastMid,       eqdb (-2.0f) },
        { pid::mastDrive,     0.38f },
        { pid::mastCeiling,   ceildb (-0.8f) },
        { pid::voiceCount,    vox (1) },
        { pid::glideMode,     gmd (GlideMode::Legato) },
        { pid::glideTime,     gtime (0.03f) },
    },
    {
        { ModSource::ModEnvelope, ModDest::InteractionAmount, 0.55f },
        { ModSource::ModEnvelope, ModDest::ShapeB,            0.40f },
        { ModSource::Velocity,    ModDest::InteractionAmount, 0.35f },
        { ModSource::Velocity,    ModDest::FilterDrive,       0.35f },
        { ModSource::Ripple,      ModDest::PitchB,            0.12f },
        { ModSource::Current,     ModDest::FilterResonance,   0.25f },
        { ModSource::ModWheel,    ModDest::InteractionAmount, 0.45f },
    }));

    out.push_back (make (kAbyss, "TRENCH", "Pad", { "Dark", "Deep", "Cinematic" }, Dry::No,
        "A pad with the top three octaves removed. Notch filter and a heavy "
        "low shelf leave a hollow in the middle of the spectrum that the reverb "
        "fills in.",
    {
        { pid::oscAWave,      wv (OscWave::Saw) },
        { pid::oscAShape,     0.40f },
        { pid::oscAOctave,    oct (-1) },
        { pid::oscAUnison,    uni (7) },
        { pid::oscADetune,    0.32f },
        { pid::oscAStereo,    1.00f },
        { pid::oscALevel,     lvl (-6.0f) },

        { pid::oscBWave,      wv (OscWave::Hollow) },
        { pid::oscBOctave,    oct (-1) },
        { pid::oscBFine,      cents (12.0f) },
        { pid::oscBUnison,    uni (4) },
        { pid::oscBDetune,    0.28f },
        { pid::oscBStereo,    0.95f },
        { pid::oscBLevel,     lvl (-9.0f) },

        { pid::subLevel,      lvl (-11.0f) },
        { pid::noiseType,     nz (NoiseType::Deep) },
        { pid::noiseLevel,    lvl (-24.0f) },
        { pid::noiseTone,     0.15f },

        { pid::filtMode,      fm (FilterMode::Notch) },
        { pid::filtCutoff,    hz (700.0f) },
        { pid::filtReso,      0.55f },
        { pid::filtDrive,     0.20f },
        { pid::filtKeyTrack,  0.30f },
        { pid::filtEnvAmt,    bip (-0.30f) },
        { pid::filtPressure,  0.70f },

        { pid::fenvAttack,    att (3.00f) },
        { pid::fenvDecay,     dec (7.00f) },
        { pid::fenvSustain,   0.45f },
        { pid::fenvRelease,   rel (7.00f) },

        { pid::aenvAttack,    att (2.20f) },
        { pid::aenvDecay,     dec (5.00f) },
        { pid::aenvSustain,   0.90f },
        { pid::aenvRelease,   rel (7.50f) },
        { pid::aenvVelocity,  0.20f },

        { pid::macroDepth,    0.85f },
        { pid::macroPressure, 0.70f },
        { pid::macroSpace,    0.85f },
        { pid::macroWet,      0.55f },
        { pid::macroGlow,     0.10f },
        { pid::fluidX,        bip (-0.45f) },
        { pid::fluidY,        bip (0.85f) },

        { pid::tideShape,     ts (TideShape::Flow) },
        { pid::tideRate,      tideHz (0.08f) },
        { pid::tideDepth,     0.45f },
        { pid::tideStereo,    0.70f },
        { pid::currRate,      currHz (0.14f) },
        { pid::currAmount,    0.50f },
        { pid::currSmooth,    0.85f },
        { pid::driftRate,     driftHz (0.01f) },
        { pid::driftAmount,   0.60f },

        { pid::resoAmount,    0.30f },
        { pid::resoSize,      0.85f },
        { pid::resoDecay,     0.75f },
        { pid::resoDamping,   0.70f },
        { pid::resoScatter,   0.40f },

        { pid::scurEnable,    on },
        { pid::scurAmount,    0.42f },
        { pid::scurRate,      scurHz (0.05f) },
        { pid::chorEnable,    on },
        { pid::chorRate,      chorHz (0.14f) },
        { pid::chorDepth,     0.50f },
        { pid::chorMix,       0.28f },
        { pid::dlyEnable,     on },
        { pid::dlyTime,       dtime (1.10f) },
        { pid::dlyFeedback,   0.48f },
        { pid::dlyDamping,    0.80f },
        { pid::dlyMix,        0.22f },
        { pid::diffEnable,    on },
        { pid::diffAmount,    0.60f },
        { pid::diffMix,       0.34f },
        { pid::verbEnable,    on },
        { pid::verbSize,      0.92f },
        { pid::verbDecay,     0.88f },
        { pid::verbPredelay,  pre (70.0f) },
        { pid::verbDamping,   0.65f },
        { pid::verbLowCut,    vLowHz (80.0f) },
        { pid::verbHighCut,   vHighHz (4500.0f) },
        { pid::verbMod,       0.40f },
        { pid::verbMix,       0.52f },

        { pid::mastLow,       eqdb (2.5f) },
        { pid::mastHigh,      eqdb (-5.0f) },
    },
    {
        { ModSource::Tide,     ModDest::FilterCutoff,  0.40f },
        { ModSource::Drift,    ModDest::FilterCutoff,  0.35f },
        { ModSource::Drift,    ModDest::FineAll,       0.25f },
        { ModSource::Current,  ModDest::FilterResonance, 0.25f },
        { ModSource::Current,  ModDest::PanB,          0.45f },
        { ModSource::ModWheel, ModDest::FilterCutoff,  0.45f },
        { ModSource::Velocity, ModDest::Amplitude,     0.20f },
    }));

    out.push_back (make (kAbyss, "BLACK SMOKER", "Texture", { "Dark", "Chaotic", "Deep" }, Dry::No,
        "A hydrothermal vent. Ring-modulated Shark against a driven filter, with "
        "Current pushing everything around fast enough to stay unstable.",
    {
        { pid::oscAWave,      wv (OscWave::Shark) },
        { pid::oscAShape,     0.62f },
        { pid::oscAOctave,    oct (-1) },
        { pid::oscAUnison,    uni (3) },
        { pid::oscADetune,    0.35f },
        { pid::oscAStereo,    0.80f },
        { pid::oscALevel,     lvl (-8.0f) },

        { pid::oscBWave,      wv (OscWave::Pulse) },
        { pid::oscBShape,     0.22f },
        { pid::oscBSemitone,  semi (-5) },
        { pid::oscBLevel,     lvl (-14.0f) },
        { pid::oscBInterMode, im (InteractionMode::RingMod) },
        { pid::oscBInterAmt,  0.60f },

        { pid::subLevel,      lvl (-12.0f) },
        { pid::noiseType,     nz (NoiseType::Deep) },
        { pid::noiseLevel,    lvl (-12.0f) },
        { pid::noiseTone,     0.32f },

        { pid::filtMode,      fm (FilterMode::BP12) },
        { pid::filtCutoff,    hz (420.0f) },
        { pid::filtReso,      0.60f },
        { pid::filtDrive,     0.62f },
        { pid::filtKeyTrack,  0.35f },
        { pid::filtEnvAmt,    bip (0.40f) },
        { pid::filtMovement,  0.45f },
        { pid::filtPressure,  0.65f },

        { pid::fenvAttack,    att (0.30f) },
        { pid::fenvDecay,     dec (2.00f) },
        { pid::fenvSustain,   0.40f },
        { pid::fenvRelease,   rel (2.50f) },

        { pid::aenvAttack,    att (0.60f) },
        { pid::aenvDecay,     dec (2.50f) },
        { pid::aenvSustain,   0.75f },
        { pid::aenvRelease,   rel (3.00f) },
        { pid::aenvVelocity,  0.35f },

        { pid::macroDepth,    0.75f },
        { pid::macroPressure, 0.80f },
        { pid::macroCurrent,  0.85f },
        { pid::macroDrops,    0.45f },
        { pid::macroWet,      0.45f },
        { pid::fluidX,        bip (0.80f) },
        { pid::fluidY,        bip (0.70f) },
        { pid::fluidXAmount,  0.85f },

        { pid::tideShape,     ts (TideShape::DoubleWave) },
        { pid::tideRate,      tideHz (0.9f) },
        { pid::tideDepth,     0.45f },
        { pid::tideStereo,    0.65f },
        { pid::currRate,      currHz (2.6f) },
        { pid::currAmount,    0.85f },
        { pid::currSmooth,    0.25f },
        { pid::currDrift,     0.55f },
        { pid::currStereo,    0.85f },
        { pid::driftRate,     driftHz (0.15f) },
        { pid::driftAmount,   0.45f },

        { pid::dropAmount,    0.38f },
        { pid::dropDensity,   0.55f },
        { pid::dropSize,      0.70f },
        { pid::dropTone,      0.28f },
        { pid::dropSplash,    0.70f },
        { pid::dropRandom,    0.95f },
        { pid::dropSpread,    0.80f },

        { pid::resoAmount,    0.32f },
        { pid::resoSize,      0.40f },
        { pid::resoDecay,     0.55f },
        { pid::resoDamping,   0.55f },
        { pid::resoScatter,   0.80f },
        { pid::resoMotion,    0.75f },

        { pid::scurEnable,    on },
        { pid::scurAmount,    0.50f },
        { pid::dlyEnable,     on },
        { pid::dlyTime,       dtime (0.30f) },
        { pid::dlyFeedback,   0.55f },
        { pid::dlyMotion,     0.65f },
        { pid::dlyDamping,    0.60f },
        { pid::dlyMix,        0.24f },
        { pid::diffEnable,    on },
        { pid::diffAmount,    0.65f },
        { pid::diffMix,       0.34f },
        { pid::verbEnable,    on },
        { pid::verbSize,      0.75f },
        { pid::verbDecay,     0.72f },
        { pid::verbDamping,   0.60f },
        { pid::verbMix,       0.38f },

        { pid::mastLow,       eqdb (1.0f) },
        { pid::mastHigh,      eqdb (-3.0f) },
        { pid::mastDrive,     0.30f },
        { pid::mastCeiling,   ceildb (-1.0f) },
    },
    {
        { ModSource::Current,     ModDest::FilterCutoff,      0.70f },
        { ModSource::Current,     ModDest::InteractionAmount, 0.50f },
        { ModSource::Current,     ModDest::ResonatorScatter,  0.55f },
        { ModSource::Tide,        ModDest::PitchB,            0.10f },
        { ModSource::Tide,        ModDest::FilterMovement,    0.45f },
        { ModSource::Drift,       ModDest::NoiseTone,         0.40f },
        { ModSource::ModEnvelope, ModDest::FilterDrive,       0.35f },
        { ModSource::RandomPerNote, ModDest::PitchB,          0.08f },
        { ModSource::ModWheel,    ModDest::DropletDensity,    0.50f },
    }));

    out.push_back (make (kAbyss, "LEVIATHAN", "Lead", { "Dark", "Deep", "Cinematic" }, Dry::No,
        "A mono lead with a slow portamento — something very large moving past. "
        "Aftertouch opens the filter and pushes the drive, so it can be played "
        "as a swell rather than as notes.",
    {
        { pid::oscAWave,      wv (OscWave::Saw) },
        { pid::oscAShape,     0.50f },
        { pid::oscAOctave,    oct (-1) },
        { pid::oscAUnison,    uni (4) },
        { pid::oscADetune,    0.16f },
        { pid::oscAStereo,    0.60f },
        { pid::oscALevel,     lvl (-3.0f) },

        { pid::oscBWave,      wv (OscWave::Hollow) },
        { pid::oscBOctave,    oct (-1) },
        { pid::oscBFine,      cents (-10.0f) },
        { pid::oscBUnison,    uni (2) },
        { pid::oscBDetune,    0.14f },
        { pid::oscBLevel,     lvl (-6.0f) },

        { pid::subLevel,      lvl (-10.0f) },
        { pid::noiseType,     nz (NoiseType::Deep) },
        { pid::noiseLevel,    lvl (-25.0f) },
        { pid::noiseTone,     0.20f },

        { pid::filtMode,      fm (FilterMode::LP24) },
        { pid::filtCutoff,    hz (520.0f) },
        { pid::filtReso,      0.40f },
        { pid::filtDrive,     0.38f },
        { pid::filtKeyTrack,  0.55f },
        { pid::filtEnvAmt,    bip (0.35f) },
        { pid::filtPressure,  0.60f },

        { pid::fenvAttack,    att (0.20f) },
        { pid::fenvDecay,     dec (1.50f) },
        { pid::fenvSustain,   0.35f },
        { pid::fenvRelease,   rel (1.20f) },
        { pid::fenvVelocity,  0.50f },

        { pid::aenvAttack,    att (0.25f) },
        { pid::aenvDecay,     dec (2.00f) },
        { pid::aenvSustain,   0.88f },
        { pid::aenvRelease,   rel (1.40f) },
        { pid::aenvVelocity,  0.40f },

        { pid::macroDepth,    0.75f },
        { pid::macroPressure, 0.60f },
        { pid::macroWet,      0.45f },
        { pid::macroSpace,    0.60f },
        { pid::fluidX,        bip (-0.10f) },
        { pid::fluidY,        bip (0.70f) },

        { pid::tideShape,     ts (TideShape::Sine) },
        { pid::tideRate,      tideHz (4.5f) },
        { pid::tideDepth,     0.12f },
        { pid::currRate,      currHz (0.4f) },
        { pid::currAmount,    0.30f },
        { pid::currSmooth,    0.70f },
        { pid::driftRate,     driftHz (0.02f) },
        { pid::driftAmount,   0.30f },

        { pid::resoAmount,    0.22f },
        { pid::resoSize,      0.55f },
        { pid::resoDecay,     0.50f },
        { pid::resoDamping,   0.68f },

        { pid::scurEnable,    on },
        { pid::scurAmount,    0.30f },
        { pid::chorEnable,    on },
        { pid::chorRate,      chorHz (0.25f) },
        { pid::chorDepth,     0.35f },
        { pid::chorMix,       0.22f },
        { pid::dlyEnable,     on },
        { pid::dlySync,       on },
        { pid::dlySyncTime,   sd (SyncDivision::QuarterD) },
        { pid::dlyFeedback,   0.42f },
        { pid::dlyDamping,    0.70f },
        { pid::dlyMix,        0.24f },
        { pid::verbEnable,    on },
        { pid::verbSize,      0.80f },
        { pid::verbDecay,     0.76f },
        { pid::verbHighCut,   vHighHz (5000.0f) },
        { pid::verbMix,       0.34f },

        { pid::mastLow,       eqdb (2.0f) },
        { pid::mastHigh,      eqdb (-2.0f) },
        { pid::mastDrive,     0.22f },
        { pid::voiceCount,    vox (1) },
        { pid::glideMode,     gmd (GlideMode::Always) },
        { pid::glideTime,     gtime (0.28f) },
        { pid::bendRange,     bend (12) },
    },
    {
        { ModSource::Aftertouch,  ModDest::FilterCutoff, 0.55f },
        { ModSource::Aftertouch,  ModDest::FilterDrive,  0.35f },
        { ModSource::Aftertouch,  ModDest::Amplitude,    0.15f },
        { ModSource::ModWheel,    ModDest::PitchAll,     0.02f },
        { ModSource::Tide,        ModDest::PitchAll,     0.04f },
        { ModSource::Velocity,    ModDest::FilterCutoff, 0.35f },
        { ModSource::ModEnvelope, ModDest::ShapeA,       0.30f },
        { ModSource::Drift,       ModDest::FineAll,      0.15f },
    }));
}

//==============================================================================
//  DROPLETS — the droplet engine in front. Percussive, sparse, physical.
//==============================================================================
void addDropletsBank (std::vector<Preset>& out)
{
    // --- DRY AQUATIC #9 ----------------------------------------------------
    out.push_back (make (kDroplets, "TINY BUBBLES", "Texture", { "Organic", "Wet", "Surface" }, Dry::Yes,
        "Small, fast, high. Dense little droplets with almost no size, a Bubble "
        "noise bed and a short bright resonator. The oscillator is only there to "
        "give the bubbles a key to sit in.",
    {
        RIPPLES_FX_BYPASSED,

        { pid::oscAWave,      wv (OscWave::Sine) },
        { pid::oscAOctave,    oct (1) },
        { pid::oscAUnison,    uni (2) },
        { pid::oscADetune,    0.12f },
        { pid::oscAStereo,    0.70f },
        { pid::oscALevel,     lvl (-19.0f) },

        { pid::oscBWave,      wv (OscWave::Glass) },
        { pid::oscBOctave,    oct (2) },
        { pid::oscBShape,     0.72f },
        { pid::oscBLevel,     lvl (-24.0f) },
        { pid::oscBStereo,    0.85f },

        { pid::subLevel,      lvl (kSilentDb) },
        { pid::noiseType,     nz (NoiseType::Bubble) },
        { pid::noiseLevel,    lvl (-14.0f) },
        { pid::noiseTone,     0.80f },

        { pid::filtMode,      fm (FilterMode::BP12) },
        { pid::filtCutoff,    hz (3200.0f) },
        { pid::filtReso,      0.38f },
        { pid::filtDrive,     0.08f },
        { pid::filtKeyTrack,  0.55f },
        { pid::filtEnvAmt,    bip (0.30f) },
        { pid::filtMovement,  0.55f },

        { pid::fenvAttack,    att (0.010f) },
        { pid::fenvDecay,     dec (0.80f) },
        { pid::fenvSustain,   0.35f },
        { pid::fenvRelease,   rel (1.00f) },

        { pid::aenvAttack,    att (0.060f) },
        { pid::aenvDecay,     dec (1.50f) },
        { pid::aenvSustain,   0.55f },
        { pid::aenvRelease,   rel (1.40f) },
        { pid::aenvVelocity,  0.45f },

        { pid::macroDrops,    0.95f },
        { pid::macroWet,      0.60f },
        { pid::macroGlow,     0.65f },
        { pid::macroCurrent,  0.45f },
        { pid::fluidX,        bip (0.35f) },
        { pid::fluidY,        bip (-0.65f) },

        { pid::tideRate,      tideHz (1.2f) },
        { pid::tideDepth,     0.25f },
        { pid::tideStereo,    0.75f },
        { pid::currRate,      currHz (2.0f) },
        { pid::currAmount,    0.50f },
        { pid::currSmooth,    0.35f },
        { pid::currStereo,    0.90f },
        { pid::driftRate,     driftHz (0.10f) },
        { pid::driftAmount,   0.30f },

        { pid::dropAmount,    0.90f },
        { pid::dropDensity,   0.88f },
        { pid::dropSize,      0.12f },
        { pid::dropTone,      0.88f },
        { pid::dropSplash,    0.45f },
        { pid::dropGravity,   0.25f },
        { pid::dropBounce,    0.40f },
        { pid::dropRandom,    0.80f },
        { pid::dropSpread,    1.00f },
        { pid::dropMode,      dm (DropletMode::Atmospheric) },

        { pid::resoAmount,    0.42f },
        { pid::resoSize,      0.28f },
        { pid::resoDecay,     0.32f },
        { pid::resoDamping,   0.30f },
        { pid::resoScatter,   0.60f },
        { pid::resoMotion,    0.50f },

        { pid::mastLow,       eqdb (-3.0f) },
        { pid::mastHigh,      eqdb (2.0f) },
    },
    {
        { ModSource::Current,     ModDest::DropletDensity, 0.45f },
        { ModSource::Current,     ModDest::DropletTone,    0.35f },
        { ModSource::Tide,        ModDest::DropletSize,    0.30f },
        { ModSource::Drift,       ModDest::FilterCutoff,   0.35f },
        { ModSource::AmpEnvelope, ModDest::DropletAmount,  0.50f },
        { ModSource::Velocity,    ModDest::DropletDensity, 0.35f },
        { ModSource::KeyTrack,    ModDest::DropletSize,   -0.40f },
        { ModSource::ModWheel,    ModDest::DropletDensity, 0.45f },
    }));

    out.push_back (make (kDroplets, "CAVE DRIP", "FX", { "Organic", "Calm", "Submerged" }, Dry::No,
        "High gravity and high bounce give the accelerating repeat of a real "
        "drip; a long dark reverb gives it somewhere to land.",
    {
        { pid::oscAWave,      wv (OscWave::Sine) },
        { pid::oscALevel,     lvl (-22.0f) },
        { pid::oscAUnison,    uni (1) },

        { pid::oscBWave,      wv (OscWave::Hollow) },
        { pid::oscBOctave,    oct (-1) },
        { pid::oscBLevel,     lvl (-26.0f) },

        { pid::subLevel,      lvl (-28.0f) },
        { pid::noiseType,     nz (NoiseType::Deep) },
        { pid::noiseLevel,    lvl (-30.0f) },
        { pid::noiseTone,     0.25f },

        { pid::filtMode,      fm (FilterMode::LP12) },
        { pid::filtCutoff,    hz (1300.0f) },
        { pid::filtReso,      0.28f },
        { pid::filtKeyTrack,  0.45f },
        { pid::filtEnvAmt,    bip (0.30f) },
        { pid::filtPressure,  0.45f },

        { pid::fenvAttack,    att (0.020f) },
        { pid::fenvDecay,     dec (2.00f) },
        { pid::fenvSustain,   0.20f },
        { pid::fenvRelease,   rel (2.00f) },

        { pid::aenvAttack,    att (0.100f) },
        { pid::aenvDecay,     dec (3.00f) },
        { pid::aenvSustain,   0.40f },
        { pid::aenvRelease,   rel (3.00f) },
        { pid::aenvVelocity,  0.35f },

        { pid::macroDrops,    0.85f },
        { pid::macroSpace,    0.80f },
        { pid::macroWet,      0.55f },
        { pid::macroDepth,    0.55f },
        { pid::fluidX,        bip (-0.30f) },
        { pid::fluidY,        bip (0.40f) },

        { pid::tideRate,      tideHz (0.18f) },
        { pid::tideDepth,     0.35f },
        { pid::currRate,      currHz (0.30f) },
        { pid::currAmount,    0.40f },
        { pid::currSmooth,    0.65f },
        { pid::driftRate,     driftHz (0.05f) },
        { pid::driftAmount,   0.40f },
        { pid::rippleRate,    rippleHz (3.0f) },
        { pid::rippleDecay,   0.70f },
        { pid::rippleDepth,   0.35f },
        { pid::rippleTrigger, rtg (RippleTrigger::Droplet) },

        { pid::dropAmount,    0.85f },
        { pid::dropDensity,   0.22f },
        { pid::dropSize,      0.58f },
        { pid::dropTone,      0.45f },
        { pid::dropSplash,    0.35f },
        { pid::dropGravity,   0.92f },
        { pid::dropBounce,    0.88f },
        { pid::dropRandom,    0.55f },
        { pid::dropSpread,    0.80f },

        { pid::resoAmount,    0.45f },
        { pid::resoSize,      0.62f },
        { pid::resoDecay,     0.70f },
        { pid::resoDamping,   0.45f },
        { pid::resoScatter,   0.40f },
        { pid::resoMotion,    0.25f },

        { pid::scurEnable,    on },
        { pid::scurAmount,    0.35f },
        { pid::dlyEnable,     on },
        { pid::dlyTime,       dtime (0.90f) },
        { pid::dlyFeedback,   0.45f },
        { pid::dlySpread,     0.65f },
        { pid::dlyDamping,    0.78f },
        { pid::dlyMix,        0.26f },
        { pid::diffEnable,    on },
        { pid::diffAmount,    0.55f },
        { pid::diffMix,       0.30f },
        { pid::verbEnable,    on },
        { pid::verbSize,      0.88f },
        { pid::verbDecay,     0.85f },
        { pid::verbPredelay,  pre (60.0f) },
        { pid::verbDamping,   0.62f },
        { pid::verbHighCut,   vHighHz (5500.0f) },
        { pid::verbMix,       0.50f },
        { pid::mastLow,       eqdb (-1.0f) },
    },
    {
        { ModSource::Ripple,      ModDest::FilterCutoff,   0.40f },
        { ModSource::Ripple,      ModDest::ResonatorSize,  0.25f },
        { ModSource::Drift,       ModDest::DropletDensity, 0.45f },
        { ModSource::Drift,       ModDest::DropletSize,    0.35f },
        { ModSource::Current,     ModDest::DropletTone,    0.35f },
        { ModSource::AmpEnvelope, ModDest::DropletAmount,  0.40f },
        { ModSource::ModWheel,    ModDest::DropletDensity, 0.50f },
    }));

    // --- DRY AQUATIC #10 ---------------------------------------------------
    out.push_back (make (kDroplets, "RAIN ON GLASS", "Texture", { "Wet", "Glassy", "Calm" }, Dry::Yes,
        "Heard from inside. Very dense, very small droplets against a bright "
        "band-pass, with the resonator tuned short and glassy so each hit has a "
        "pitch but no tail. No reverb — the resonator is the window.",
    {
        RIPPLES_FX_BYPASSED,

        { pid::oscAWave,      wv (OscWave::Glass) },
        { pid::oscAShape,     0.80f },
        { pid::oscAOctave,    oct (1) },
        { pid::oscAUnison,    uni (3) },
        { pid::oscADetune,    0.14f },
        { pid::oscAStereo,    0.90f },
        { pid::oscALevel,     lvl (-15.0f) },

        { pid::oscBWave,      wv (OscWave::Triangle) },
        { pid::oscBOctave,    oct (2) },
        { pid::oscBLevel,     lvl (-25.0f) },
        { pid::oscBStereo,    0.85f },

        { pid::subLevel,      lvl (kSilentDb) },
        { pid::noiseType,     nz (NoiseType::Air) },
        { pid::noiseLevel,    lvl (-17.0f) },
        { pid::noiseTone,     0.88f },

        { pid::filtMode,      fm (FilterMode::BP12) },
        { pid::filtCutoff,    hz (4200.0f) },
        { pid::filtReso,      0.34f },
        { pid::filtKeyTrack,  0.50f },
        { pid::filtEnvAmt,    bip (0.18f) },
        { pid::filtMovement,  0.65f },

        { pid::fenvAttack,    att (0.30f) },
        { pid::fenvDecay,     dec (2.00f) },
        { pid::fenvSustain,   0.50f },
        { pid::fenvRelease,   rel (2.00f) },

        { pid::aenvAttack,    att (0.50f) },
        { pid::aenvDecay,     dec (2.50f) },
        { pid::aenvSustain,   0.65f },
        { pid::aenvRelease,   rel (2.50f) },
        { pid::aenvVelocity,  0.30f },

        { pid::macroDrops,    1.00f },
        { pid::macroWet,      0.55f },
        { pid::macroGlow,     0.70f },
        { pid::macroCurrent,  0.50f },
        { pid::fluidX,        bip (0.45f) },
        { pid::fluidY,        bip (-0.70f) },

        { pid::tideShape,     ts (TideShape::Flow) },
        { pid::tideRate,      tideHz (0.35f) },
        { pid::tideDepth,     0.40f },
        { pid::tideStereo,    0.80f },
        { pid::currRate,      currHz (1.6f) },
        { pid::currAmount,    0.55f },
        { pid::currSmooth,    0.45f },
        { pid::currStereo,    0.95f },
        { pid::driftRate,     driftHz (0.07f) },
        { pid::driftAmount,   0.35f },

        { pid::dropAmount,    0.95f },
        { pid::dropDensity,   1.00f },
        { pid::dropSize,      0.10f },
        { pid::dropTone,      0.92f },
        { pid::dropSplash,    0.30f },
        { pid::dropGravity,   0.20f },
        { pid::dropBounce,    0.15f },
        { pid::dropRandom,    0.95f },
        { pid::dropSpread,    1.00f },

        { pid::resoAmount,    0.55f },
        { pid::resoSize,      0.22f },
        { pid::resoDecay,     0.26f },
        { pid::resoDamping,   0.25f },
        { pid::resoScatter,   0.55f },
        { pid::resoMotion,    0.40f },

        { pid::mastLow,       eqdb (-4.0f) },
        { pid::mastHigh,      eqdb (1.5f) },
    },
    {
        { ModSource::Tide,        ModDest::DropletDensity, 0.35f },
        { ModSource::Current,     ModDest::DropletTone,    0.40f },
        { ModSource::Current,     ModDest::FilterCutoff,   0.35f },
        { ModSource::Drift,       ModDest::DropletSize,    0.30f },
        { ModSource::Drift,       ModDest::ResonatorSize,  0.25f },
        { ModSource::AmpEnvelope, ModDest::DropletAmount,  0.55f },
        { ModSource::ModWheel,    ModDest::DropletDensity, 0.40f },
    }));

    out.push_back (make (kDroplets, "DROPLET KEYS", "Key", { "Wet", "Organic", "Bright" }, Dry::No,
        "Note-mode droplets tuned to the note you play, so every key strike "
        "arrives with its own splash. Velocity decides how big the splash is.",
    {
        { pid::oscAWave,      wv (OscWave::Water) },
        { pid::oscAShape,     0.52f },
        { pid::oscAUnison,    uni (2) },
        { pid::oscADetune,    0.10f },
        { pid::oscAStereo,    0.55f },
        { pid::oscALevel,     lvl (-4.0f) },
        { pid::oscAPhase,     startPhase (0.0f) },

        { pid::oscBWave,      wv (OscWave::Sine) },
        { pid::oscBOctave,    oct (1) },
        { pid::oscBLevel,     lvl (-14.0f) },

        { pid::subLevel,      lvl (-18.0f) },
        { pid::noiseType,     nz (NoiseType::Bubble) },
        { pid::noiseLevel,    lvl (-22.0f) },
        { pid::noiseTone,     0.62f },

        { pid::filtMode,      fm (FilterMode::LP24) },
        { pid::filtCutoff,    hz (1600.0f) },
        { pid::filtReso,      0.30f },
        { pid::filtDrive,     0.16f },
        { pid::filtKeyTrack,  0.70f },
        { pid::filtEnvAmt,    bip (0.50f) },
        { pid::filtPressure,  0.30f },

        { pid::fenvAttack,    att (0.002f) },
        { pid::fenvDecay,     dec (0.45f) },
        { pid::fenvSustain,   0.15f },
        { pid::fenvRelease,   rel (0.60f) },
        { pid::fenvVelocity,  0.65f },

        { pid::aenvAttack,    att (0.004f) },
        { pid::aenvDecay,     dec (1.80f) },
        { pid::aenvSustain,   0.30f },
        { pid::aenvRelease,   rel (1.20f) },
        { pid::aenvVelocity,  0.70f },

        { pid::macroDrops,    0.80f },
        { pid::macroWet,      0.50f },
        { pid::macroGlow,     0.50f },
        { pid::macroSpace,    0.45f },
        { pid::fluidX,        bip (0.10f) },
        { pid::fluidY,        bip (-0.20f) },

        { pid::tideRate,      tideHz (0.6f) },
        { pid::tideDepth,     0.20f },
        { pid::currRate,      currHz (0.9f) },
        { pid::currAmount,    0.35f },
        { pid::rippleRate,    rippleHz (6.5f) },
        { pid::rippleDecay,   0.75f },
        { pid::rippleDepth,   0.35f },

        { pid::dropAmount,    0.70f },
        { pid::dropMode,      dm (DropletMode::Note) },
        { pid::dropDensity,   0.40f },
        { pid::dropSize,      0.40f },
        { pid::dropTone,      0.62f },
        { pid::dropSplash,    0.65f },
        { pid::dropGravity,   0.55f },
        { pid::dropBounce,    0.50f },
        { pid::dropRandom,    0.45f },
        { pid::dropSpread,    0.75f },

        { pid::resoAmount,    0.35f },
        { pid::resoSize,      0.45f },
        { pid::resoDecay,     0.45f },
        { pid::resoDamping,   0.42f },

        { pid::scurEnable,    on },
        { pid::chorEnable,    on },
        { pid::chorRate,      chorHz (0.5f) },
        { pid::chorMix,       0.24f },
        { pid::dlyEnable,     on },
        { pid::dlySync,       on },
        { pid::dlySyncTime,   sd (SyncDivision::Sixteenth) },
        { pid::dlyFeedback,   0.32f },
        { pid::dlyDamping,    0.68f },
        { pid::dlyMix,        0.18f },
        { pid::verbEnable,    on },
        { pid::verbSize,      0.60f },
        { pid::verbDecay,     0.55f },
        { pid::verbMix,       0.30f },
    },
    {
        { ModSource::Velocity,    ModDest::DropletAmount, 0.60f },
        { ModSource::Velocity,    ModDest::DropletSize,   0.35f },
        { ModSource::Velocity,    ModDest::FilterCutoff,  0.40f },
        { ModSource::KeyTrack,    ModDest::DropletTone,   0.40f },
        { ModSource::Ripple,      ModDest::DropletSize,  -0.30f },
        { ModSource::ModEnvelope, ModDest::ShapeA,        0.30f },
        { ModSource::RandomPerNote, ModDest::PanA,        0.35f },
    }));

    out.push_back (make (kDroplets, "CONDENSATION", "FX", { "Calm", "Wet", "Submerged" }, Dry::No,
        "Almost nothing happening. Very sparse, very large droplets over a "
        "barely-there pad — an ambience bed rather than an instrument.",
    {
        { pid::oscAWave,      wv (OscWave::Water) },
        { pid::oscAShape,     0.30f },
        { pid::oscAUnison,    uni (4) },
        { pid::oscADetune,    0.26f },
        { pid::oscAStereo,    0.95f },
        { pid::oscALevel,     lvl (-13.0f) },

        { pid::oscBWave,      wv (OscWave::Hollow) },
        { pid::oscBOctave,    oct (-1) },
        { pid::oscBUnison,    uni (2) },
        { pid::oscBLevel,     lvl (-18.0f) },

        { pid::subLevel,      lvl (-20.0f) },
        { pid::noiseType,     nz (NoiseType::Surf) },
        { pid::noiseLevel,    lvl (-26.0f) },
        { pid::noiseTone,     0.40f },

        { pid::filtMode,      fm (FilterMode::LP12) },
        { pid::filtCutoff,    hz (900.0f) },
        { pid::filtReso,      0.20f },
        { pid::filtKeyTrack,  0.30f },
        { pid::filtEnvAmt,    bip (0.20f) },
        { pid::filtPressure,  0.50f },

        { pid::fenvAttack,    att (4.00f) },
        { pid::fenvDecay,     dec (8.00f) },
        { pid::fenvSustain,   0.60f },
        { pid::fenvRelease,   rel (8.00f) },

        { pid::aenvAttack,    att (3.50f) },
        { pid::aenvDecay,     dec (6.00f) },
        { pid::aenvSustain,   0.85f },
        { pid::aenvRelease,   rel (7.00f) },
        { pid::aenvVelocity,  0.15f },

        { pid::macroDrops,    0.60f },
        { pid::macroSpace,    0.85f },
        { pid::macroDepth,    0.55f },
        { pid::macroWet,      0.55f },
        { pid::fluidX,        bip (-0.65f) },
        { pid::fluidY,        bip (0.35f) },

        { pid::tideShape,     ts (TideShape::Swell) },
        { pid::tideRate,      tideHz (0.06f) },
        { pid::tideDepth,     0.45f },
        { pid::tideStereo,    0.70f },
        { pid::currRate,      currHz (0.12f) },
        { pid::currAmount,    0.40f },
        { pid::currSmooth,    0.90f },
        { pid::driftRate,     driftHz (0.008f) },
        { pid::driftAmount,   0.55f },
        { pid::driftStereo,   0.95f },

        { pid::dropAmount,    0.55f },
        { pid::dropDensity,   0.06f },
        { pid::dropSize,      0.90f },
        { pid::dropTone,      0.35f },
        { pid::dropSplash,    0.20f },
        { pid::dropGravity,   0.40f },
        { pid::dropBounce,    0.10f },
        { pid::dropRandom,    0.90f },
        { pid::dropSpread,    1.00f },

        { pid::resoAmount,    0.38f },
        { pid::resoSize,      0.80f },
        { pid::resoDecay,     0.72f },
        { pid::resoDamping,   0.60f },
        { pid::resoMotion,    0.30f },

        { pid::scurEnable,    on },
        { pid::scurAmount,    0.45f },
        { pid::scurRate,      scurHz (0.03f) },
        { pid::chorEnable,    on },
        { pid::chorRate,      chorHz (0.10f) },
        { pid::chorDepth,     0.55f },
        { pid::chorMix,       0.30f },
        { pid::dlyEnable,     on },
        { pid::dlyTime,       dtime (1.80f) },
        { pid::dlyFeedback,   0.50f },
        { pid::dlyDamping,    0.82f },
        { pid::dlyMix,        0.22f },
        { pid::diffEnable,    on },
        { pid::diffAmount,    0.65f },
        { pid::diffMix,       0.36f },
        { pid::verbEnable,    on },
        { pid::verbSize,      0.92f },
        { pid::verbDecay,     0.90f },
        { pid::verbDamping,   0.58f },
        { pid::verbMix,       0.55f },
        { pid::mastHigh,      eqdb (-1.0f) },
        { pid::voiceCount,    vox (8) },
    },
    {
        { ModSource::Drift,       ModDest::DropletDensity, 0.40f },
        { ModSource::Drift,       ModDest::FineAll,        0.25f },
        { ModSource::Tide,        ModDest::FilterCutoff,   0.35f },
        { ModSource::Tide,        ModDest::DropletSize,    0.30f },
        { ModSource::Current,     ModDest::PanA,           0.45f },
        { ModSource::Current,     ModDest::ReverbMix,      0.20f },
        { ModSource::AmpEnvelope, ModDest::DropletAmount,  0.35f },
    }));
}

//==============================================================================
//  CURRENTS — motion first. The Current, Drift and Tide modulators lead.
//==============================================================================
void addCurrentsBank (std::vector<Preset>& out)
{
    // --- DRY AQUATIC #11 ---------------------------------------------------
    out.push_back (make (kCurrents, "CRYSTAL CURRENT", "Arp", { "Glassy", "Bright", "Wet" }, Dry::Yes,
        "Built for fast arpeggios with no delay under them: the repeats come "
        "from the resonator and the per-note random pan, and Current keeps the "
        "timbre moving so a repeated pattern never sounds looped.",
    {
        RIPPLES_FX_BYPASSED,

        { pid::oscAWave,      wv (OscWave::Glass) },
        { pid::oscAShape,     0.66f },
        { pid::oscAUnison,    uni (2) },
        { pid::oscADetune,    0.08f },
        { pid::oscAStereo,    0.70f },
        { pid::oscALevel,     lvl (-3.0f) },
        { pid::oscAPhase,     startPhase (0.0f) },

        { pid::oscBWave,      wv (OscWave::Water) },
        { pid::oscBOctave,    oct (1) },
        { pid::oscBShape,     0.45f },
        { pid::oscBFine,      cents (6.0f) },
        { pid::oscBLevel,     lvl (-11.0f) },
        { pid::oscBStereo,    0.75f },

        { pid::subLevel,      lvl (-21.0f) },
        { pid::noiseType,     nz (NoiseType::Bubble) },
        { pid::noiseLevel,    lvl (-24.0f) },
        { pid::noiseTone,     0.75f },

        { pid::filtMode,      fm (FilterMode::LP24) },
        { pid::filtCutoff,    hz (1900.0f) },
        { pid::filtReso,      0.42f },
        { pid::filtDrive,     0.12f },
        { pid::filtKeyTrack,  0.75f },
        { pid::filtEnvAmt,    bip (0.55f) },
        { pid::filtPressure,  0.20f },

        { pid::fenvAttack,    att (0.001f) },
        { pid::fenvDecay,     dec (0.22f) },
        { pid::fenvSustain,   0.05f },
        { pid::fenvRelease,   rel (0.25f) },
        { pid::fenvVelocity,  0.70f },

        { pid::aenvAttack,    att (0.001f) },
        { pid::aenvDecay,     dec (0.60f) },
        { pid::aenvSustain,   0.05f },
        { pid::aenvRelease,   rel (0.50f) },
        { pid::aenvVelocity,  0.75f },

        { pid::macroCurrent,  0.75f },
        { pid::macroWet,      0.55f },
        { pid::macroGlow,     0.70f },
        { pid::macroRipple,   0.40f },
        { pid::macroDrops,    0.35f },
        { pid::fluidX,        bip (0.40f) },
        { pid::fluidY,        bip (-0.35f) },
        { pid::fluidXAmount,  0.70f },

        { pid::tideShape,     ts (TideShape::Triangle) },
        { pid::tideSync,      on },
        { pid::tideSyncRate,  sd (SyncDivision::TwoBars) },
        { pid::tideDepth,     0.50f },
        { pid::tideStereo,    0.60f },
        { pid::currRate,      currHz (2.4f) },
        { pid::currAmount,    0.65f },
        { pid::currSmooth,    0.40f },
        { pid::currDrift,     0.30f },
        { pid::currStereo,    0.90f },
        { pid::driftRate,     driftHz (0.06f) },
        { pid::driftAmount,   0.35f },
        { pid::rippleRate,    rippleHz (11.0f) },
        { pid::rippleDecay,   0.82f },
        { pid::rippleDepth,   0.38f },
        { pid::rippleCycles,  cycles (4.0f) },
        { pid::rippleSpread,  0.60f },

        { pid::dropAmount,    0.30f },
        { pid::dropMode,      dm (DropletMode::Note) },
        { pid::dropDensity,   0.30f },
        { pid::dropSize,      0.20f },
        { pid::dropTone,      0.85f },
        { pid::dropSplash,    0.40f },
        { pid::dropSpread,    0.90f },

        { pid::resoAmount,    0.55f },
        { pid::resoSize,      0.40f },
        { pid::resoDecay,     0.62f },
        { pid::resoDamping,   0.30f },
        { pid::resoScatter,   0.50f },
        { pid::resoMotion,    0.55f },

        { pid::mastHigh,      eqdb (2.0f) },
        { pid::mastDrive,     0.10f },
    },
    {
        { ModSource::Current,       ModDest::ShapeA,           0.50f },
        { ModSource::Current,       ModDest::FilterCutoff,     0.40f },
        { ModSource::Current,       ModDest::ResonatorScatter, 0.35f },
        { ModSource::Tide,          ModDest::FilterCutoff,     0.35f },
        { ModSource::Ripple,        ModDest::ResonatorSize,   -0.30f },
        { ModSource::RandomPerNote, ModDest::PanA,             0.55f },
        { ModSource::RandomPerNote, ModDest::ResonatorSize,    0.20f },
        { ModSource::Velocity,      ModDest::FilterCutoff,     0.40f },
        { ModSource::KeyTrack,      ModDest::ResonatorDecay,  -0.25f },
    }));

    out.push_back (make (kCurrents, "UNDERTOW", "Bass", { "Dark", "Deep", "Organic" }, Dry::No,
        "A bass that will not sit still. Current runs the cutoff at about two "
        "cycles a second and the drive with it, so the tone pulls away and back "
        "under a steady note.",
    {
        { pid::oscAWave,      wv (OscWave::Saw) },
        { pid::oscAShape,     0.48f },
        { pid::oscAOctave,    oct (-1) },
        { pid::oscAUnison,    uni (2) },
        { pid::oscADetune,    0.12f },
        { pid::oscAStereo,    0.30f },
        { pid::oscALevel,     lvl (-3.0f) },

        { pid::oscBWave,      wv (OscWave::Water) },
        { pid::oscBOctave,    oct (-1) },
        { pid::oscBShape,     0.40f },
        { pid::oscBFine,      cents (-12.0f) },
        { pid::oscBLevel,     lvl (-9.0f) },

        { pid::subWave,       sw (SubWave::Sine) },
        { pid::subLevel,      lvl (-8.0f) },
        { pid::noiseType,     nz (NoiseType::Deep) },
        { pid::noiseLevel,    lvl (-27.0f) },
        { pid::noiseTone,     0.22f },

        { pid::filtMode,      fm (FilterMode::LP24) },
        { pid::filtCutoff,    hz (300.0f) },
        { pid::filtReso,      0.45f },
        { pid::filtDrive,     0.40f },
        { pid::filtKeyTrack,  0.85f },
        { pid::filtEnvAmt,    bip (0.50f) },
        { pid::filtPressure,  0.65f },

        { pid::fenvAttack,    att (0.002f) },
        { pid::fenvDecay,     dec (0.50f) },
        { pid::fenvSustain,   0.25f },
        { pid::fenvRelease,   rel (0.35f) },
        { pid::fenvVelocity,  0.65f },

        { pid::aenvAttack,    att (0.004f) },
        { pid::aenvDecay,     dec (1.00f) },
        { pid::aenvSustain,   0.75f },
        { pid::aenvRelease,   rel (0.30f) },
        { pid::aenvVelocity,  0.50f },

        { pid::macroDepth,    0.80f },
        { pid::macroCurrent,  0.80f },
        { pid::macroPressure, 0.65f },
        { pid::macroWet,      0.35f },
        { pid::fluidX,        bip (0.35f) },
        { pid::fluidY,        bip (0.60f) },

        { pid::tideRate,      tideHz (0.45f) },
        { pid::tideDepth,     0.20f },
        { pid::currRate,      currHz (2.0f) },
        { pid::currAmount,    0.70f },
        { pid::currSmooth,    0.45f },
        { pid::currDrift,     0.40f },
        { pid::currStereo,    0.40f },
        { pid::driftRate,     driftHz (0.04f) },
        { pid::driftAmount,   0.30f },

        { pid::resoAmount,    0.20f },
        { pid::resoSize,      0.34f },
        { pid::resoDecay,     0.35f },
        { pid::resoDamping,   0.75f },
        { pid::resoMotion,    0.40f },

        { pid::scurEnable,    on },
        { pid::scurAmount,    0.20f },
        { pid::scurWidth,     0.35f },
        { pid::diffEnable,    on },
        { pid::diffAmount,    0.30f },
        { pid::diffMix,       0.16f },
        { pid::verbEnable,    on },
        { pid::verbSize,      0.45f },
        { pid::verbDecay,     0.40f },
        { pid::verbLowCut,    vLowHz (350.0f) },
        { pid::verbMix,       0.16f },

        { pid::mastLow,       eqdb (2.0f) },
        { pid::mastDrive,     0.26f },
        { pid::voiceCount,    vox (2) },
        { pid::glideMode,     gmd (GlideMode::Legato) },
        { pid::glideTime,     gtime (0.08f) },
    },
    {
        { ModSource::Current,     ModDest::FilterCutoff, 0.60f },
        { ModSource::Current,     ModDest::FilterDrive,  0.35f },
        { ModSource::Current,     ModDest::ShapeA,       0.30f },
        { ModSource::Drift,       ModDest::FilterCutoff, 0.25f },
        { ModSource::Velocity,    ModDest::FilterCutoff, 0.40f },
        { ModSource::ModEnvelope, ModDest::SubLevel,     0.25f },
        { ModSource::ModWheel,    ModDest::FilterResonance, 0.30f },
    }));

    out.push_back (make (kCurrents, "GULF STREAM", "Pad", { "Calm", "Dreamy", "Deep" }, Dry::No,
        "The slowest thing in the bank. Drift at one cycle every three minutes "
        "moves tuning, cutoff and stereo position together, so a held chord is "
        "never quite the same chord twice.",
    {
        { pid::oscAWave,      wv (OscWave::Water) },
        { pid::oscAShape,     0.48f },
        { pid::oscAUnison,    uni (6) },
        { pid::oscADetune,    0.28f },
        { pid::oscAStereo,    0.95f },
        { pid::oscALevel,     lvl (-5.0f) },

        { pid::oscBWave,      wv (OscWave::Sine) },
        { pid::oscBOctave,    oct (1) },
        { pid::oscBFine,      cents (3.0f) },
        { pid::oscBUnison,    uni (3) },
        { pid::oscBDetune,    0.20f },
        { pid::oscBStereo,    0.90f },
        { pid::oscBLevel,     lvl (-14.0f) },

        { pid::subLevel,      lvl (-17.0f) },
        { pid::noiseType,     nz (NoiseType::Surf) },
        { pid::noiseLevel,    lvl (-28.0f) },
        { pid::noiseTone,     0.45f },

        { pid::filtMode,      fm (FilterMode::LP12) },
        { pid::filtCutoff,    hz (1700.0f) },
        { pid::filtReso,      0.18f },
        { pid::filtKeyTrack,  0.40f },
        { pid::filtEnvAmt,    bip (0.28f) },
        { pid::filtPressure,  0.45f },

        { pid::fenvAttack,    att (3.00f) },
        { pid::fenvDecay,     dec (6.00f) },
        { pid::fenvSustain,   0.60f },
        { pid::fenvRelease,   rel (6.00f) },

        { pid::aenvAttack,    att (2.20f) },
        { pid::aenvDecay,     dec (5.00f) },
        { pid::aenvSustain,   0.88f },
        { pid::aenvRelease,   rel (6.00f) },
        { pid::aenvVelocity,  0.20f },

        { pid::macroCurrent,  0.60f },
        { pid::macroDepth,    0.55f },
        { pid::macroWet,      0.55f },
        { pid::macroSpace,    0.75f },
        { pid::macroGlow,     0.40f },
        { pid::fluidX,        bip (-0.50f) },
        { pid::fluidY,        bip (0.25f) },

        { pid::tideShape,     ts (TideShape::Flow) },
        { pid::tideRate,      tideHz (0.05f) },
        { pid::tideDepth,     0.45f },
        { pid::tideStereo,    0.85f },
        { pid::currRate,      currHz (0.10f) },
        { pid::currAmount,    0.55f },
        { pid::currSmooth,    0.92f },
        { pid::currDrift,     0.55f },
        { pid::currStereo,    0.90f },
        { pid::driftRate,     driftHz (0.005f) },
        { pid::driftAmount,   0.80f },
        { pid::driftStereo,   0.95f },

        { pid::resoAmount,    0.24f },
        { pid::resoSize,      0.75f },
        { pid::resoDecay,     0.66f },
        { pid::resoDamping,   0.55f },
        { pid::resoMotion,    0.45f },

        { pid::scurEnable,    on },
        { pid::scurAmount,    0.50f },
        { pid::scurRate,      scurHz (0.025f) },
        { pid::scurWidth,     0.70f },
        { pid::chorEnable,    on },
        { pid::chorRate,      chorHz (0.09f) },
        { pid::chorDepth,     0.55f },
        { pid::chorDelay,     chdel (26.0f) },
        { pid::chorMix,       0.34f },
        { pid::dlyEnable,     on },
        { pid::dlyTime,       dtime (1.40f) },
        { pid::dlyFeedback,   0.44f },
        { pid::dlyMotion,     0.55f },
        { pid::dlyDamping,    0.75f },
        { pid::dlyMix,        0.20f },
        { pid::diffEnable,    on },
        { pid::diffMix,       0.30f },
        { pid::verbEnable,    on },
        { pid::verbSize,      0.85f },
        { pid::verbDecay,     0.82f },
        { pid::verbMod,       0.50f },
        { pid::verbMix,       0.44f },
    },
    {
        { ModSource::Drift,   ModDest::FineAll,        0.30f },
        { ModSource::Drift,   ModDest::FilterCutoff,   0.35f },
        { ModSource::Drift,   ModDest::PanA,           0.40f },
        { ModSource::Drift,   ModDest::StereoWidth,    0.30f },
        { ModSource::Current, ModDest::OscMix,         0.35f },
        { ModSource::Current, ModDest::ResonatorSize,  0.25f },
        { ModSource::Tide,    ModDest::ShapeA,         0.35f },
        { ModSource::Tide,    ModDest::DelayMotion,    0.25f },
    }));

    out.push_back (make (kCurrents, "EDDY", "Arp", { "Chaotic", "Wet", "Bright" }, Dry::No,
        "A synced sixteenth-note tide against a free-running Current, so the "
        "pattern turns against itself. Ring-modulated highs for the spin.",
    {
        { pid::oscAWave,      wv (OscWave::Pulse) },
        { pid::oscAShape,     0.35f },
        { pid::oscAUnison,    uni (2) },
        { pid::oscADetune,    0.10f },
        { pid::oscAStereo,    0.65f },
        { pid::oscALevel,     lvl (-4.0f) },
        { pid::oscAPhase,     startPhase (0.0f) },

        { pid::oscBWave,      wv (OscWave::Glass) },
        { pid::oscBOctave,    oct (1) },
        { pid::oscBSemitone,  semi (7) },
        { pid::oscBLevel,     lvl (-13.0f) },
        { pid::oscBInterMode, im (InteractionMode::RingMod) },
        { pid::oscBInterAmt,  0.35f },

        { pid::subLevel,      lvl (-19.0f) },
        { pid::noiseType,     nz (NoiseType::Bubble) },
        { pid::noiseLevel,    lvl (-23.0f) },
        { pid::noiseTone,     0.68f },

        { pid::filtMode,      fm (FilterMode::Morph) },
        { pid::filtCutoff,    hz (2100.0f) },
        { pid::filtReso,      0.48f },
        { pid::filtDrive,     0.20f },
        { pid::filtKeyTrack,  0.65f },
        { pid::filtEnvAmt,    bip (0.50f) },
        { pid::filtMovement,  0.40f },

        { pid::fenvAttack,    att (0.001f) },
        { pid::fenvDecay,     dec (0.14f) },
        { pid::fenvSustain,   0.00f },
        { pid::fenvRelease,   rel (0.18f) },
        { pid::fenvVelocity,  0.75f },

        { pid::aenvAttack,    att (0.001f) },
        { pid::aenvDecay,     dec (0.35f) },
        { pid::aenvSustain,   0.00f },
        { pid::aenvRelease,   rel (0.28f) },
        { pid::aenvVelocity,  0.80f },

        { pid::macroCurrent,  0.85f },
        { pid::macroRipple,   0.55f },
        { pid::macroWet,      0.50f },
        { pid::macroGlow,     0.60f },
        { pid::fluidX,        bip (0.70f) },
        { pid::fluidY,        bip (-0.25f) },
        { pid::fluidXAmount,  0.85f },

        { pid::tideShape,     ts (TideShape::DoubleWave) },
        { pid::tideSync,      on },
        { pid::tideSyncRate,  sd (SyncDivision::Sixteenth) },
        { pid::tideDepth,     0.55f },
        { pid::tideStereo,    0.70f },
        { pid::currRate,      currHz (3.5f) },
        { pid::currAmount,    0.70f },
        { pid::currSmooth,    0.30f },
        { pid::currStereo,    0.90f },
        { pid::rippleRate,    rippleHz (14.0f) },
        { pid::rippleDecay,   0.85f },
        { pid::rippleDepth,   0.45f },
        { pid::rippleCycles,  cycles (2.5f) },

        { pid::dropAmount,    0.25f },
        { pid::dropMode,      dm (DropletMode::Note) },
        { pid::dropDensity,   0.35f },
        { pid::dropSize,      0.22f },
        { pid::dropTone,      0.82f },
        { pid::dropRandom,    0.75f },
        { pid::dropSpread,    0.90f },

        { pid::resoAmount,    0.38f },
        { pid::resoSize,      0.36f },
        { pid::resoDecay,     0.45f },
        { pid::resoDamping,   0.38f },
        { pid::resoScatter,   0.65f },
        { pid::resoMotion,    0.65f },

        { pid::scurEnable,    on },
        { pid::scurAmount,    0.45f },
        { pid::chorEnable,    on },
        { pid::chorRate,      chorHz (1.5f) },
        { pid::chorDepth,     0.35f },
        { pid::chorMix,       0.24f },
        { pid::dlyEnable,     on },
        { pid::dlySync,       on },
        { pid::dlySyncTime,   sd (SyncDivision::SixteenthD) },
        { pid::dlyFeedback,   0.52f },
        { pid::dlyMotion,     0.60f },
        { pid::dlySpread,     0.75f },
        { pid::dlyMix,        0.28f },
        { pid::verbEnable,    on },
        { pid::verbSize,      0.55f },
        { pid::verbDecay,     0.50f },
        { pid::verbMix,       0.26f },
        { pid::mastHigh,      eqdb (1.5f) },
    },
    {
        { ModSource::Tide,          ModDest::FilterMovement,    0.50f },
        { ModSource::Tide,          ModDest::ShapeA,            0.40f },
        { ModSource::Current,       ModDest::FilterCutoff,      0.55f },
        { ModSource::Current,       ModDest::InteractionAmount, 0.40f },
        { ModSource::Ripple,        ModDest::PitchB,            0.10f },
        { ModSource::RandomPerNote, ModDest::PanA,              0.50f },
        { ModSource::RandomPerNote, ModDest::ShapeA,            0.25f },
        { ModSource::Velocity,      ModDest::InteractionAmount, 0.30f },
    }));

    // --- DRY AQUATIC #12 ---------------------------------------------------
    out.push_back (make (kCurrents, "RIVER MOUTH", "Texture", { "Organic", "Calm", "Surface" }, Dry::Yes,
        "Where the fresh water meets the salt. Surf noise and a Water oscillator "
        "share a slow Morph filter, and the stereo image comes entirely from "
        "decorrelated Current and Drift — there is no width effect running.",
    {
        RIPPLES_FX_BYPASSED,

        { pid::oscAWave,      wv (OscWave::Water) },
        { pid::oscAShape,     0.58f },
        { pid::oscAUnison,    uni (5) },
        { pid::oscADetune,    0.24f },
        { pid::oscAStereo,    1.00f },
        { pid::oscAPan,       bip (-0.15f) },
        { pid::oscALevel,     lvl (-7.0f) },

        { pid::oscBWave,      wv (OscWave::Hollow) },
        { pid::oscBOctave,    oct (-1) },
        { pid::oscBFine,      cents (11.0f) },
        { pid::oscBUnison,    uni (3) },
        { pid::oscBDetune,    0.26f },
        { pid::oscBStereo,    1.00f },
        { pid::oscBPan,       bip (0.15f) },
        { pid::oscBLevel,     lvl (-10.0f) },

        { pid::subLevel,      lvl (-19.0f) },
        { pid::noiseType,     nz (NoiseType::Surf) },
        { pid::noiseLevel,    lvl (-13.0f) },
        { pid::noiseTone,     0.48f },

        { pid::filtMode,      fm (FilterMode::Morph) },
        { pid::filtCutoff,    hz (1300.0f) },
        { pid::filtReso,      0.30f },
        { pid::filtDrive,     0.14f },
        { pid::filtKeyTrack,  0.35f },
        { pid::filtEnvAmt,    bip (0.22f) },
        { pid::filtMovement,  0.25f },
        { pid::filtPressure,  0.40f },

        { pid::fenvAttack,    att (1.50f) },
        { pid::fenvDecay,     dec (4.00f) },
        { pid::fenvSustain,   0.55f },
        { pid::fenvRelease,   rel (4.00f) },

        { pid::aenvAttack,    att (1.20f) },
        { pid::aenvDecay,     dec (3.50f) },
        { pid::aenvSustain,   0.82f },
        { pid::aenvRelease,   rel (4.50f) },
        { pid::aenvVelocity,  0.25f },

        { pid::macroCurrent,  0.70f },
        { pid::macroWet,      0.60f },
        { pid::macroDepth,    0.45f },
        { pid::macroDrops,    0.35f },
        { pid::macroGlow,     0.40f },
        { pid::fluidX,        bip (0.15f) },
        { pid::fluidY,        bip (-0.15f) },

        { pid::tideShape,     ts (TideShape::Swell) },
        { pid::tideRate,      tideHz (0.14f) },
        { pid::tideDepth,     0.50f },
        { pid::tideStereo,    0.90f },
        { pid::currRate,      currHz (0.85f) },
        { pid::currAmount,    0.65f },
        { pid::currSmooth,    0.60f },
        { pid::currDrift,     0.50f },
        { pid::currStereo,    1.00f },
        { pid::driftRate,     driftHz (0.03f) },
        { pid::driftAmount,   0.55f },
        { pid::driftStereo,   1.00f },

        { pid::dropAmount,    0.34f },
        { pid::dropDensity,   0.45f },
        { pid::dropSize,      0.40f },
        { pid::dropTone,      0.58f },
        { pid::dropSplash,    0.50f },
        { pid::dropRandom,    0.80f },
        { pid::dropSpread,    1.00f },

        { pid::resoAmount,    0.34f },
        { pid::resoSize,      0.62f },
        { pid::resoDecay,     0.52f },
        { pid::resoDamping,   0.50f },
        { pid::resoScatter,   0.55f },
        { pid::resoMotion,    0.60f },

        { pid::mastLow,       eqdb (-1.0f) },
        { pid::mastDrive,     0.08f },
    },
    {
        { ModSource::Current,     ModDest::FilterMovement, 0.55f },
        { ModSource::Current,     ModDest::PanA,           0.55f },
        { ModSource::Current,     ModDest::NoiseTone,      0.40f },
        { ModSource::Drift,       ModDest::PanB,          -0.50f },
        { ModSource::Drift,       ModDest::FilterCutoff,   0.30f },
        { ModSource::Tide,        ModDest::NoiseLevel,     0.40f },
        { ModSource::Tide,        ModDest::OscMix,         0.30f },
        { ModSource::AmpEnvelope, ModDest::DropletAmount,  0.35f },
        { ModSource::ModWheel,    ModDest::FilterMovement, 0.40f },
    }));
}

//==============================================================================
//  BIOLUMINESCENCE — light made by living things. Glassy, inharmonic, glowing.
//==============================================================================
void addBioluminescenceBank (std::vector<Preset>& out)
{
    // --- DRY AQUATIC #13 ---------------------------------------------------
    out.push_back (make (kBiolum, "BIOLUMINESCENT KEYS", "Key", { "Glassy", "Dreamy", "Organic" }, Dry::Yes,
        "Bell-like, but alive. The Glass oscillator supplies the inharmonic "
        "partials and the resonator bank — large, bright, lightly scattered — "
        "supplies the glow. The soft-edged attack and long tail come from the "
        "envelopes, not from a reverb: there isn't one.",
    {
        RIPPLES_FX_BYPASSED,

        { pid::oscAWave,      wv (OscWave::Glass) },
        { pid::oscAShape,     0.58f },
        { pid::oscAUnison,    uni (3) },
        { pid::oscADetune,    0.11f },
        { pid::oscAStereo,    0.80f },
        { pid::oscALevel,     lvl (-4.0f) },

        { pid::oscBWave,      wv (OscWave::Sine) },
        { pid::oscBOctave,    oct (1) },
        { pid::oscBSemitone,  semi (7) },
        { pid::oscBFine,      cents (4.0f) },
        { pid::oscBLevel,     lvl (-15.0f) },
        { pid::oscBStereo,    0.70f },
        { pid::oscBInterMode, im (InteractionMode::PhaseMod) },
        { pid::oscBInterAmt,  0.18f },

        { pid::subWave,       sw (SubWave::Sine) },
        { pid::subLevel,      lvl (-19.0f) },
        { pid::noiseType,     nz (NoiseType::Air) },
        { pid::noiseLevel,    lvl (-26.0f) },
        { pid::noiseTone,     0.82f },

        { pid::filtMode,      fm (FilterMode::LP24) },
        { pid::filtCutoff,    hz (2600.0f) },
        { pid::filtReso,      0.34f },
        { pid::filtDrive,     0.10f },
        { pid::filtKeyTrack,  0.80f },
        { pid::filtEnvAmt,    bip (0.42f) },
        { pid::filtPressure,  0.25f },

        { pid::fenvAttack,    att (0.030f) },
        { pid::fenvDecay,     dec (2.20f) },
        { pid::fenvSustain,   0.22f },
        { pid::fenvRelease,   rel (2.50f) },
        { pid::fenvVelocity,  0.55f },

        { pid::aenvAttack,    att (0.050f) },
        { pid::aenvDecay,     dec (4.00f) },
        { pid::aenvSustain,   0.35f },
        { pid::aenvRelease,   rel (3.50f) },
        { pid::aenvVelocity,  0.60f },

        { pid::macroGlow,     0.90f },
        { pid::macroWet,      0.55f },
        { pid::macroRipple,   0.40f },
        { pid::macroDrops,    0.30f },
        { pid::macroCurrent,  0.40f },
        { pid::fluidX,        bip (0.05f) },
        { pid::fluidY,        bip (-0.20f) },

        { pid::tideShape,     ts (TideShape::Flow) },
        { pid::tideRate,      tideHz (0.28f) },
        { pid::tideDepth,     0.38f },
        { pid::tideStereo,    0.75f },
        { pid::currRate,      currHz (0.55f) },
        { pid::currAmount,    0.45f },
        { pid::currSmooth,    0.60f },
        { pid::currStereo,    0.85f },
        { pid::driftRate,     driftHz (0.04f) },
        { pid::driftAmount,   0.35f },
        { pid::rippleRate,    rippleHz (5.0f) },
        { pid::rippleDecay,   0.70f },
        { pid::rippleDepth,   0.32f },
        { pid::rippleCycles,  cycles (4.0f) },
        { pid::rippleSpread,  0.65f },

        { pid::dropAmount,    0.26f },
        { pid::dropMode,      dm (DropletMode::Note) },
        { pid::dropDensity,   0.24f },
        { pid::dropSize,      0.30f },
        { pid::dropTone,      0.85f },
        { pid::dropSplash,    0.35f },
        { pid::dropRandom,    0.60f },
        { pid::dropSpread,    0.90f },

        { pid::resoAmount,    0.62f },
        { pid::resoSize,      0.72f },
        { pid::resoDecay,     0.74f },
        { pid::resoDamping,   0.28f },
        { pid::resoScatter,   0.38f },
        { pid::resoMotion,    0.45f },

        { pid::mastHigh,      eqdb (1.5f) },
        { pid::mastDrive,     0.06f },
    },
    {
        { ModSource::ModEnvelope,   ModDest::InteractionAmount, 0.35f },
        { ModSource::Tide,          ModDest::ResonatorSize,     0.30f },
        { ModSource::Tide,          ModDest::ShapeA,            0.30f },
        { ModSource::Current,       ModDest::ResonatorScatter,  0.35f },
        { ModSource::Current,       ModDest::FilterCutoff,      0.30f },
        { ModSource::Ripple,        ModDest::ResonatorDamping, -0.35f },
        { ModSource::Velocity,      ModDest::ResonatorAmount,   0.35f },
        { ModSource::Velocity,      ModDest::FilterCutoff,      0.40f },
        { ModSource::KeyTrack,      ModDest::ResonatorDecay,   -0.30f },
        { ModSource::RandomPerNote, ModDest::PanA,              0.25f },
        { ModSource::Aftertouch,    ModDest::ResonatorAmount,   0.30f },
    }));

    out.push_back (make (kBiolum, "GLOW PLANKTON", "Texture", { "Glassy", "Dreamy", "Organic" }, Dry::No,
        "A cloud of tiny lights. High, sparse, and never still — the Current "
        "moves shape and pan, the droplets are almost too small to hear on "
        "their own, and the delay keeps spreading them further out.",
    {
        { pid::oscAWave,      wv (OscWave::Glass) },
        { pid::oscAShape,     0.75f },
        { pid::oscAOctave,    oct (1) },
        { pid::oscAUnison,    uni (4) },
        { pid::oscADetune,    0.18f },
        { pid::oscAStereo,    0.95f },
        { pid::oscALevel,     lvl (-9.0f) },

        { pid::oscBWave,      wv (OscWave::Water) },
        { pid::oscBOctave,    oct (2) },
        { pid::oscBShape,     0.65f },
        { pid::oscBUnison,    uni (2) },
        { pid::oscBLevel,     lvl (-18.0f) },
        { pid::oscBStereo,    0.90f },

        { pid::subLevel,      lvl (-30.0f) },
        { pid::noiseType,     nz (NoiseType::Air) },
        { pid::noiseLevel,    lvl (-20.0f) },
        { pid::noiseTone,     0.90f },

        { pid::filtMode,      fm (FilterMode::BP12) },
        { pid::filtCutoff,    hz (3800.0f) },
        { pid::filtReso,      0.40f },
        { pid::filtKeyTrack,  0.60f },
        { pid::filtEnvAmt,    bip (0.25f) },
        { pid::filtMovement,  0.60f },

        { pid::fenvAttack,    att (0.60f) },
        { pid::fenvDecay,     dec (3.00f) },
        { pid::fenvSustain,   0.45f },
        { pid::fenvRelease,   rel (3.00f) },

        { pid::aenvAttack,    att (0.90f) },
        { pid::aenvDecay,     dec (3.50f) },
        { pid::aenvSustain,   0.60f },
        { pid::aenvRelease,   rel (3.50f) },
        { pid::aenvVelocity,  0.35f },

        { pid::macroGlow,     0.95f },
        { pid::macroWet,      0.55f },
        { pid::macroDrops,    0.55f },
        { pid::macroCurrent,  0.60f },
        { pid::macroSpace,    0.60f },
        { pid::fluidX,        bip (0.30f) },
        { pid::fluidY,        bip (-0.70f) },

        { pid::tideShape,     ts (TideShape::DoubleWave) },
        { pid::tideRate,      tideHz (0.65f) },
        { pid::tideDepth,     0.40f },
        { pid::tideStereo,    0.85f },
        { pid::currRate,      currHz (1.8f) },
        { pid::currAmount,    0.60f },
        { pid::currSmooth,    0.40f },
        { pid::currStereo,    0.95f },
        { pid::driftRate,     driftHz (0.09f) },
        { pid::driftAmount,   0.40f },

        { pid::dropAmount,    0.45f },
        { pid::dropDensity,   0.60f },
        { pid::dropSize,      0.15f },
        { pid::dropTone,      0.92f },
        { pid::dropSplash,    0.40f },
        { pid::dropRandom,    0.90f },
        { pid::dropSpread,    1.00f },

        { pid::resoAmount,    0.48f },
        { pid::resoSize,      0.55f },
        { pid::resoDecay,     0.60f },
        { pid::resoDamping,   0.25f },
        { pid::resoScatter,   0.70f },
        { pid::resoMotion,    0.65f },

        { pid::scurEnable,    on },
        { pid::scurAmount,    0.50f },
        { pid::chorEnable,    on },
        { pid::chorRate,      chorHz (0.7f) },
        { pid::chorDepth,     0.40f },
        { pid::chorMix,       0.28f },
        { pid::dlyEnable,     on },
        { pid::dlyTime,       dtime (0.48f) },
        { pid::dlyFeedback,   0.50f },
        { pid::dlyMotion,     0.55f },
        { pid::dlySpread,     0.80f },
        { pid::dlyDamping,    0.45f },
        { pid::dlyMix,        0.30f },
        { pid::diffEnable,    on },
        { pid::diffMix,       0.30f },
        { pid::verbEnable,    on },
        { pid::verbSize,      0.70f },
        { pid::verbDecay,     0.68f },
        { pid::verbHighCut,   vHighHz (14000.0f) },
        { pid::verbMix,       0.40f },

        { pid::mastLow,       eqdb (-3.0f) },
        { pid::mastHigh,      eqdb (2.5f) },
    },
    {
        { ModSource::Current,       ModDest::ShapeA,          0.50f },
        { ModSource::Current,       ModDest::PanA,            0.50f },
        { ModSource::Current,       ModDest::DropletDensity,  0.40f },
        { ModSource::Tide,          ModDest::FilterCutoff,    0.40f },
        { ModSource::Tide,          ModDest::ResonatorScatter, 0.30f },
        { ModSource::Drift,         ModDest::PanB,           -0.45f },
        { ModSource::AmpEnvelope,   ModDest::DropletAmount,   0.40f },
        { ModSource::RandomPerNote, ModDest::FineAll,         0.05f },
    }));

    out.push_back (make (kBiolum, "ANGLERFISH", "Lead", { "Dark", "Glassy", "Cinematic" }, Dry::No,
        "A dark body with one bright point in front of it: a closed LP24 on the "
        "sub-heavy layer, and a thin Glass partial riding above the cutoff that "
        "the mod wheel brings forward.",
    {
        { pid::oscAWave,      wv (OscWave::Hollow) },
        { pid::oscAShape,     0.32f },
        { pid::oscAOctave,    oct (-1) },
        { pid::oscAUnison,    uni (3) },
        { pid::oscADetune,    0.16f },
        { pid::oscAStereo,    0.55f },
        { pid::oscALevel,     lvl (-4.0f) },

        { pid::oscBWave,      wv (OscWave::Glass) },
        { pid::oscBOctave,    oct (2) },
        { pid::oscBShape,     0.80f },
        { pid::oscBLevel,     lvl (-20.0f) },
        { pid::oscBStereo,    0.85f },

        { pid::subWave,       sw (SubWave::Sine) },
        { pid::subLevel,      lvl (-9.0f) },
        { pid::noiseType,     nz (NoiseType::Deep) },
        { pid::noiseLevel,    lvl (-26.0f) },
        { pid::noiseTone,     0.20f },

        { pid::filtMode,      fm (FilterMode::LP24) },
        { pid::filtCutoff,    hz (600.0f) },
        { pid::filtReso,      0.44f },
        { pid::filtDrive,     0.30f },
        { pid::filtKeyTrack,  0.60f },
        { pid::filtEnvAmt,    bip (0.45f) },
        { pid::filtPressure,  0.55f },

        { pid::fenvAttack,    att (0.080f) },
        { pid::fenvDecay,     dec (1.20f) },
        { pid::fenvSustain,   0.30f },
        { pid::fenvRelease,   rel (1.00f) },
        { pid::fenvVelocity,  0.60f },

        { pid::aenvAttack,    att (0.120f) },
        { pid::aenvDecay,     dec (1.80f) },
        { pid::aenvSustain,   0.80f },
        { pid::aenvRelease,   rel (1.20f) },
        { pid::aenvVelocity,  0.50f },

        { pid::macroDepth,    0.70f },
        { pid::macroGlow,     0.55f },
        { pid::macroWet,      0.45f },
        { pid::macroSpace,    0.55f },
        { pid::fluidX,        bip (-0.15f) },
        { pid::fluidY,        bip (0.60f) },

        { pid::tideShape,     ts (TideShape::Sine) },
        { pid::tideRate,      tideHz (3.2f) },
        { pid::tideDepth,     0.18f },
        { pid::currRate,      currHz (0.35f) },
        { pid::currAmount,    0.35f },
        { pid::currSmooth,    0.70f },
        { pid::driftRate,     driftHz (0.02f) },
        { pid::driftAmount,   0.30f },
        { pid::rippleRate,    rippleHz (4.5f) },
        { pid::rippleDecay,   0.70f },
        { pid::rippleDepth,   0.25f },

        { pid::resoAmount,    0.40f },
        { pid::resoSize,      0.58f },
        { pid::resoDecay,     0.66f },
        { pid::resoDamping,   0.35f },
        { pid::resoScatter,   0.35f },
        { pid::resoMotion,    0.40f },

        { pid::scurEnable,    on },
        { pid::scurAmount,    0.30f },
        { pid::chorEnable,    on },
        { pid::chorRate,      chorHz (0.3f) },
        { pid::chorMix,       0.22f },
        { pid::dlyEnable,     on },
        { pid::dlySync,       on },
        { pid::dlySyncTime,   sd (SyncDivision::Quarter) },
        { pid::dlyFeedback,   0.44f },
        { pid::dlyDamping,    0.66f },
        { pid::dlyMix,        0.26f },
        { pid::verbEnable,    on },
        { pid::verbSize,      0.76f },
        { pid::verbDecay,     0.72f },
        { pid::verbHighCut,   vHighHz (6000.0f) },
        { pid::verbMix,       0.36f },

        { pid::mastLow,       eqdb (1.5f) },
        { pid::mastDrive,     0.18f },
        { pid::voiceCount,    vox (2) },
        { pid::glideMode,     gmd (GlideMode::Legato) },
        { pid::glideTime,     gtime (0.12f) },
        { pid::bendRange,     bend (12) },
    },
    {
        { ModSource::ModWheel,    ModDest::LevelB,           0.60f },
        { ModSource::ModWheel,    ModDest::ResonatorAmount,  0.35f },
        { ModSource::Aftertouch,  ModDest::FilterCutoff,     0.45f },
        { ModSource::Velocity,    ModDest::FilterCutoff,     0.35f },
        { ModSource::ModEnvelope, ModDest::ShapeB,           0.40f },
        { ModSource::Ripple,      ModDest::ResonatorScatter, 0.30f },
        { ModSource::Tide,        ModDest::LevelB,           0.25f },
        { ModSource::Drift,       ModDest::FineAll,          0.12f },
    }));

    out.push_back (make (kBiolum, "JELLYFISH", "Pad", { "Dreamy", "Glassy", "Calm" }, Dry::No,
        "Pulse and glide. A Swell-shaped tide drives amplitude and cutoff "
        "together so the pad contracts and releases on its own, once every few "
        "seconds, whether or not you play anything new.",
    {
        { pid::oscAWave,      wv (OscWave::Water) },
        { pid::oscAShape,     0.60f },
        { pid::oscAUnison,    uni (5) },
        { pid::oscADetune,    0.20f },
        { pid::oscAStereo,    0.95f },
        { pid::oscALevel,     lvl (-5.0f) },

        { pid::oscBWave,      wv (OscWave::Glass) },
        { pid::oscBOctave,    oct (1) },
        { pid::oscBShape,     0.50f },
        { pid::oscBFine,      cents (-6.0f) },
        { pid::oscBUnison,    uni (3) },
        { pid::oscBDetune,    0.16f },
        { pid::oscBStereo,    0.90f },
        { pid::oscBLevel,     lvl (-14.0f) },

        { pid::subLevel,      lvl (-18.0f) },
        { pid::noiseType,     nz (NoiseType::Air) },
        { pid::noiseLevel,    lvl (-27.0f) },
        { pid::noiseTone,     0.75f },

        { pid::filtMode,      fm (FilterMode::LP12) },
        { pid::filtCutoff,    hz (2000.0f) },
        { pid::filtReso,      0.26f },
        { pid::filtKeyTrack,  0.50f },
        { pid::filtEnvAmt,    bip (0.30f) },
        { pid::filtPressure,  0.35f },

        { pid::fenvAttack,    att (1.80f) },
        { pid::fenvDecay,     dec (4.00f) },
        { pid::fenvSustain,   0.55f },
        { pid::fenvRelease,   rel (4.50f) },

        { pid::aenvAttack,    att (1.50f) },
        { pid::aenvDecay,     dec (3.50f) },
        { pid::aenvSustain,   0.85f },
        { pid::aenvRelease,   rel (5.00f) },
        { pid::aenvVelocity,  0.25f },

        { pid::macroGlow,     0.70f },
        { pid::macroWet,      0.55f },
        { pid::macroSpace,    0.70f },
        { pid::macroRipple,   0.35f },
        { pid::macroCurrent,  0.45f },
        { pid::fluidX,        bip (-0.35f) },
        { pid::fluidY,        bip (-0.10f) },

        { pid::tideShape,     ts (TideShape::Swell) },
        { pid::tideRate,      tideHz (0.25f) },
        { pid::tideDepth,     0.70f },
        { pid::tideStereo,    0.65f },
        { pid::currRate,      currHz (0.30f) },
        { pid::currAmount,    0.45f },
        { pid::currSmooth,    0.75f },
        { pid::driftRate,     driftHz (0.02f) },
        { pid::driftAmount,   0.45f },
        { pid::rippleRate,    rippleHz (2.5f) },
        { pid::rippleDecay,   0.65f },
        { pid::rippleDepth,   0.30f },
        { pid::rippleSpread,  0.70f },

        { pid::dropAmount,    0.20f },
        { pid::dropDensity,   0.20f },
        { pid::dropSize,      0.45f },
        { pid::dropTone,      0.75f },
        { pid::dropSpread,    0.95f },

        { pid::resoAmount,    0.44f },
        { pid::resoSize,      0.66f },
        { pid::resoDecay,     0.66f },
        { pid::resoDamping,   0.35f },
        { pid::resoScatter,   0.45f },
        { pid::resoMotion,    0.55f },

        { pid::scurEnable,    on },
        { pid::scurAmount,    0.45f },
        { pid::scurRate,      scurHz (0.07f) },
        { pid::chorEnable,    on },
        { pid::chorRate,      chorHz (0.22f) },
        { pid::chorDepth,     0.50f },
        { pid::chorMix,       0.32f },
        { pid::dlyEnable,     on },
        { pid::dlyTime,       dtime (0.70f) },
        { pid::dlyFeedback,   0.42f },
        { pid::dlyMotion,     0.50f },
        { pid::dlyDamping,    0.60f },
        { pid::dlyMix,        0.24f },
        { pid::diffEnable,    on },
        { pid::diffMix,       0.32f },
        { pid::verbEnable,    on },
        { pid::verbSize,      0.80f },
        { pid::verbDecay,     0.76f },
        { pid::verbMod,       0.50f },
        { pid::verbMix,       0.44f },
    },
    {
        { ModSource::Tide,        ModDest::Amplitude,       0.45f },
        { ModSource::Tide,        ModDest::FilterCutoff,    0.40f },
        { ModSource::Tide,        ModDest::ResonatorAmount, 0.25f },
        { ModSource::Ripple,      ModDest::ShapeA,          0.30f },
        { ModSource::Current,     ModDest::OscMix,          0.30f },
        { ModSource::Drift,       ModDest::FineAll,         0.15f },
        { ModSource::Drift,       ModDest::PanB,            0.40f },
        { ModSource::ModWheel,    ModDest::ChorusDepth,     0.40f },
    }));

    // --- DRY AQUATIC #14 ---------------------------------------------------
    out.push_back (make (kBiolum, "PHOSPHOR", "Pluck", { "Bright", "Glassy", "Wet" }, Dry::Yes,
        "A struck light. Extremely short amp decay into a long, bright, "
        "undamped resonator — all of the sustain you hear is the resonator "
        "bank ringing, with no reverb or delay anywhere.",
    {
        RIPPLES_FX_BYPASSED,

        { pid::oscAWave,      wv (OscWave::Glass) },
        { pid::oscAShape,     0.72f },
        { pid::oscAUnison,    uni (2) },
        { pid::oscADetune,    0.07f },
        { pid::oscAStereo,    0.65f },
        { pid::oscALevel,     lvl (-4.0f) },
        { pid::oscAPhase,     startPhase (0.0f) },

        { pid::oscBWave,      wv (OscWave::Triangle) },
        { pid::oscBOctave,    oct (2) },
        { pid::oscBLevel,     lvl (-17.0f) },
        { pid::oscBPhase,     startPhase (0.0f) },

        { pid::subLevel,      lvl (-24.0f) },
        { pid::noiseType,     nz (NoiseType::Bubble) },
        { pid::noiseLevel,    lvl (-18.0f) },
        { pid::noiseTone,     0.88f },

        { pid::filtMode,      fm (FilterMode::BP12) },
        { pid::filtCutoff,    hz (2800.0f) },
        { pid::filtReso,      0.40f },
        { pid::filtKeyTrack,  0.85f },
        { pid::filtEnvAmt,    bip (0.60f) },
        { pid::filtMovement,  0.55f },

        { pid::fenvAttack,    att (0.001f) },
        { pid::fenvDecay,     dec (0.10f) },
        { pid::fenvSustain,   0.00f },
        { pid::fenvRelease,   rel (0.12f) },
        { pid::fenvVelocity,  0.80f },

        { pid::aenvAttack,    att (0.001f) },
        { pid::aenvDecay,     dec (0.16f) },
        { pid::aenvSustain,   0.00f },
        { pid::aenvRelease,   rel (0.14f) },
        { pid::aenvVelocity,  0.85f },

        { pid::macroGlow,     0.85f },
        { pid::macroWet,      0.60f },
        { pid::macroRipple,   0.50f },
        { pid::macroDrops,    0.40f },
        { pid::fluidX,        bip (0.25f) },
        { pid::fluidY,        bip (-0.45f) },

        { pid::tideRate,      tideHz (1.0f) },
        { pid::tideDepth,     0.20f },
        { pid::tideStereo,    0.70f },
        { pid::currRate,      currHz (1.2f) },
        { pid::currAmount,    0.40f },
        { pid::currSmooth,    0.45f },
        { pid::rippleRate,    rippleHz (16.0f) },
        { pid::rippleDecay,   0.88f },
        { pid::rippleDepth,   0.40f },
        { pid::rippleCycles,  cycles (3.0f) },
        { pid::rippleSpread,  0.70f },

        { pid::dropAmount,    0.35f },
        { pid::dropMode,      dm (DropletMode::Note) },
        { pid::dropDensity,   0.28f },
        { pid::dropSize,      0.16f },
        { pid::dropTone,      0.90f },
        { pid::dropSplash,    0.45f },
        { pid::dropBounce,    0.35f },
        { pid::dropSpread,    0.85f },

        { pid::resoAmount,    0.75f },
        { pid::resoSize,      0.58f },
        { pid::resoDecay,     0.88f },
        { pid::resoDamping,   0.15f },
        { pid::resoScatter,   0.42f },
        { pid::resoMotion,    0.50f },

        { pid::mastLow,       eqdb (-2.0f) },
        { pid::mastHigh,      eqdb (2.0f) },
        { pid::mastDrive,     0.08f },
    },
    {
        { ModSource::Velocity,      ModDest::ResonatorAmount,  0.45f },
        { ModSource::Velocity,      ModDest::ResonatorDecay,   0.30f },
        { ModSource::Velocity,      ModDest::FilterCutoff,     0.45f },
        { ModSource::Ripple,        ModDest::ResonatorScatter, 0.40f },
        { ModSource::Ripple,        ModDest::FilterCutoff,     0.35f },
        { ModSource::KeyTrack,      ModDest::ResonatorDecay,  -0.35f },
        { ModSource::Current,       ModDest::ResonatorSize,    0.25f },
        { ModSource::RandomPerNote, ModDest::PanA,             0.45f },
        { ModSource::RandomPerNote, ModDest::ResonatorSize,    0.15f },
    }));
}

//==============================================================================
//  STORMS — weather. Loud, unstable, driven. The chaos end of the Fluid Field.
//==============================================================================
void addStormsBank (std::vector<Preset>& out)
{
    out.push_back (make (kStorms, "SQUALL", "Texture", { "Chaotic", "Dark", "Surface" }, Dry::No,
        "Wind over water. Almost entirely noise: a band-pass sweeping fast under "
        "Current, with the oscillators only thick enough to give it a key.",
    {
        { pid::oscAWave,      wv (OscWave::Shark) },
        { pid::oscAShape,     0.55f },
        { pid::oscAUnison,    uni (4) },
        { pid::oscADetune,    0.38f },
        { pid::oscAStereo,    0.90f },
        { pid::oscALevel,     lvl (-14.0f) },

        { pid::oscBWave,      wv (OscWave::Water) },
        { pid::oscBOctave,    oct (-1) },
        { pid::oscBUnison,    uni (2) },
        { pid::oscBLevel,     lvl (-18.0f) },

        { pid::subLevel,      lvl (-22.0f) },
        { pid::noiseType,     nz (NoiseType::Surf) },
        { pid::noiseLevel,    lvl (-2.0f) },
        { pid::noiseTone,     0.55f },

        { pid::filtMode,      fm (FilterMode::BP12) },
        { pid::filtCutoff,    hz (900.0f) },
        { pid::filtReso,      0.52f },
        { pid::filtDrive,     0.35f },
        { pid::filtKeyTrack,  0.20f },
        { pid::filtEnvAmt,    bip (0.30f) },
        { pid::filtMovement,  0.50f },
        { pid::filtPressure,  0.55f },

        { pid::fenvAttack,    att (0.40f) },
        { pid::fenvDecay,     dec (2.50f) },
        { pid::fenvSustain,   0.50f },
        { pid::fenvRelease,   rel (2.50f) },

        { pid::aenvAttack,    att (0.80f) },
        { pid::aenvDecay,     dec (3.00f) },
        { pid::aenvSustain,   0.80f },
        { pid::aenvRelease,   rel (3.00f) },
        { pid::aenvVelocity,  0.30f },

        { pid::macroCurrent,  0.90f },
        { pid::macroWet,      0.55f },
        { pid::macroPressure, 0.60f },
        { pid::macroSpace,    0.55f },
        { pid::fluidX,        bip (0.85f) },
        { pid::fluidY,        bip (-0.30f) },
        { pid::fluidXAmount,  0.90f },

        { pid::tideShape,     ts (TideShape::Swell) },
        { pid::tideRate,      tideHz (0.30f) },
        { pid::tideDepth,     0.65f },
        { pid::tideStereo,    0.80f },
        { pid::currRate,      currHz (4.0f) },
        { pid::currAmount,    0.90f },
        { pid::currSmooth,    0.25f },
        { pid::currDrift,     0.55f },
        { pid::currStereo,    0.95f },
        { pid::driftRate,     driftHz (0.20f) },
        { pid::driftAmount,   0.50f },

        { pid::dropAmount,    0.35f },
        { pid::dropDensity,   0.85f },
        { pid::dropSize,      0.20f },
        { pid::dropTone,      0.70f },
        { pid::dropSplash,    0.80f },
        { pid::dropRandom,    1.00f },
        { pid::dropSpread,    1.00f },

        { pid::resoAmount,    0.22f },
        { pid::resoSize,      0.45f },
        { pid::resoDecay,     0.35f },
        { pid::resoDamping,   0.60f },
        { pid::resoScatter,   0.85f },
        { pid::resoMotion,    0.80f },

        { pid::scurEnable,    on },
        { pid::scurAmount,    0.55f },
        { pid::scurRate,      scurHz (0.35f) },
        { pid::dlyEnable,     on },
        { pid::dlyTime,       dtime (0.28f) },
        { pid::dlyFeedback,   0.50f },
        { pid::dlyMotion,     0.70f },
        { pid::dlySpread,     0.80f },
        { pid::dlyMix,        0.22f },
        { pid::diffEnable,    on },
        { pid::diffAmount,    0.70f },
        { pid::diffMix,       0.36f },
        { pid::verbEnable,    on },
        { pid::verbSize,      0.72f },
        { pid::verbDecay,     0.66f },
        { pid::verbMix,       0.34f },

        { pid::mastLow,       eqdb (-2.0f) },
        { pid::mastDrive,     0.22f },
        { pid::mastCeiling,   ceildb (-1.0f) },
    },
    {
        { ModSource::Current,     ModDest::FilterCutoff,   0.80f },
        { ModSource::Current,     ModDest::NoiseTone,      0.50f },
        { ModSource::Current,     ModDest::PanA,           0.50f },
        { ModSource::Tide,        ModDest::NoiseLevel,     0.50f },
        { ModSource::Tide,        ModDest::FilterResonance, 0.25f },
        { ModSource::Drift,       ModDest::FilterCutoff,   0.35f },
        { ModSource::Drift,       ModDest::DropletDensity, 0.45f },
        { ModSource::ModWheel,    ModDest::FilterCutoff,   0.45f },
    }));

    // --- DRY AQUATIC #15 ---------------------------------------------------
    out.push_back (make (kStorms, "BREAKERS", "FX", { "Chaotic", "Organic", "Surface" }, Dry::Yes,
        "Waves arriving. The Swell tide shape does the work — slow rise, fast "
        "fall — driving noise level, cutoff and droplet density together, so "
        "each cycle builds and collapses. Completely dry.",
    {
        RIPPLES_FX_BYPASSED,

        { pid::oscAWave,      wv (OscWave::Water) },
        { pid::oscAShape,     0.40f },
        { pid::oscAOctave,    oct (-1) },
        { pid::oscAUnison,    uni (5) },
        { pid::oscADetune,    0.32f },
        { pid::oscAStereo,    1.00f },
        { pid::oscALevel,     lvl (-12.0f) },

        { pid::oscBWave,      wv (OscWave::Hollow) },
        { pid::oscBOctave,    oct (-1) },
        { pid::oscBFine,      cents (-15.0f) },
        { pid::oscBUnison,    uni (3) },
        { pid::oscBStereo,    0.95f },
        { pid::oscBLevel,     lvl (-16.0f) },

        { pid::subWave,       sw (SubWave::Sine) },
        { pid::subLevel,      lvl (-14.0f) },
        { pid::noiseType,     nz (NoiseType::Surf) },
        { pid::noiseLevel,    lvl (-3.0f) },
        { pid::noiseTone,     0.42f },

        { pid::filtMode,      fm (FilterMode::LP12) },
        { pid::filtCutoff,    hz (700.0f) },
        { pid::filtReso,      0.36f },
        { pid::filtDrive,     0.28f },
        { pid::filtKeyTrack,  0.20f },
        { pid::filtEnvAmt,    bip (0.30f) },
        { pid::filtPressure,  0.60f },

        { pid::fenvAttack,    att (1.20f) },
        { pid::fenvDecay,     dec (4.00f) },
        { pid::fenvSustain,   0.60f },
        { pid::fenvRelease,   rel (4.00f) },

        { pid::aenvAttack,    att (1.80f) },
        { pid::aenvDecay,     dec (4.00f) },
        { pid::aenvSustain,   0.85f },
        { pid::aenvRelease,   rel (5.00f) },
        { pid::aenvVelocity,  0.20f },

        { pid::macroWet,      0.75f },
        { pid::macroCurrent,  0.65f },
        { pid::macroDrops,    0.60f },
        { pid::macroPressure, 0.60f },
        { pid::macroDepth,    0.50f },
        { pid::fluidX,        bip (0.60f) },
        { pid::fluidY,        bip (0.10f) },

        { pid::tideShape,     ts (TideShape::Swell) },
        { pid::tideRate,      tideHz (0.13f) },
        { pid::tideDepth,     0.90f },
        { pid::tideStereo,    0.65f },
        { pid::currRate,      currHz (1.6f) },
        { pid::currAmount,    0.70f },
        { pid::currSmooth,    0.35f },
        { pid::currDrift,     0.45f },
        { pid::currStereo,    0.95f },
        { pid::driftRate,     driftHz (0.05f) },
        { pid::driftAmount,   0.45f },
        { pid::rippleRate,    rippleHz (2.0f) },
        { pid::rippleDecay,   0.60f },
        { pid::rippleDepth,   0.35f },
        { pid::rippleCycles,  cycles (6.0f) },

        { pid::dropAmount,    0.55f },
        { pid::dropDensity,   0.65f },
        { pid::dropSize,      0.35f },
        { pid::dropTone,      0.55f },
        { pid::dropSplash,    0.85f },
        { pid::dropGravity,   0.45f },
        { pid::dropBounce,    0.30f },
        { pid::dropRandom,    0.95f },
        { pid::dropSpread,    1.00f },

        { pid::resoAmount,    0.30f },
        { pid::resoSize,      0.70f },
        { pid::resoDecay,     0.50f },
        { pid::resoDamping,   0.65f },
        { pid::resoScatter,   0.60f },
        { pid::resoMotion,    0.60f },

        { pid::mastLow,       eqdb (1.0f) },
        { pid::mastDrive,     0.16f },
    },
    {
        { ModSource::Tide,        ModDest::NoiseLevel,     0.70f },
        { ModSource::Tide,        ModDest::FilterCutoff,   0.60f },
        { ModSource::Tide,        ModDest::DropletDensity, 0.55f },
        { ModSource::Tide,        ModDest::Amplitude,      0.25f },
        { ModSource::Current,     ModDest::NoiseTone,      0.45f },
        { ModSource::Current,     ModDest::PanA,           0.50f },
        { ModSource::Drift,       ModDest::FilterCutoff,   0.30f },
        { ModSource::Ripple,      ModDest::DropletAmount,  0.35f },
        { ModSource::AmpEnvelope, ModDest::DropletAmount,  0.30f },
        { ModSource::ModWheel,    ModDest::DropletDensity, 0.45f },
    }));

    out.push_back (make (kStorms, "RIPTIDE", "Bass", { "Chaotic", "Dark", "Deep" }, Dry::No,
        "FM between two detuned Sharks, with the mod envelope on the FM amount "
        "so every note starts metallic and collapses into a clean low note.",
    {
        { pid::oscAWave,      wv (OscWave::Shark) },
        { pid::oscAShape,     0.50f },
        { pid::oscAOctave,    oct (-1) },
        { pid::oscAUnison,    uni (2) },
        { pid::oscADetune,    0.14f },
        { pid::oscAStereo,    0.35f },
        { pid::oscALevel,     lvl (-3.0f) },
        { pid::oscAPhase,     startPhase (0.0f) },

        { pid::oscBWave,      wv (OscWave::Saw) },
        { pid::oscBOctave,    oct (0) },
        { pid::oscBSemitone,  semi (7) },
        { pid::oscBLevel,     lvl (-22.0f) },
        { pid::oscBInterMode, im (InteractionMode::FM) },
        { pid::oscBInterAmt,  0.40f },
        { pid::oscBPhase,     startPhase (0.0f) },

        { pid::subWave,       sw (SubWave::Square) },
        { pid::subLevel,      lvl (-7.0f) },
        { pid::noiseType,     nz (NoiseType::Deep) },
        { pid::noiseLevel,    lvl (-24.0f) },
        { pid::noiseTone,     0.28f },

        { pid::filtMode,      fm (FilterMode::LP24) },
        { pid::filtCutoff,    hz (240.0f) },
        { pid::filtReso,      0.50f },
        { pid::filtDrive,     0.60f },
        { pid::filtKeyTrack,  0.90f },
        { pid::filtEnvAmt,    bip (0.75f) },
        { pid::filtPressure,  0.70f },

        { pid::fenvAttack,    att (0.001f) },
        { pid::fenvDecay,     dec (0.20f) },
        { pid::fenvSustain,   0.08f },
        { pid::fenvRelease,   rel (0.20f) },
        { pid::fenvVelocity,  0.80f },

        { pid::aenvAttack,    att (0.002f) },
        { pid::aenvDecay,     dec (0.70f) },
        { pid::aenvSustain,   0.62f },
        { pid::aenvRelease,   rel (0.18f) },
        { pid::aenvVelocity,  0.65f },

        { pid::macroDepth,    0.85f },
        { pid::macroPressure, 0.80f },
        { pid::macroCurrent,  0.55f },
        { pid::macroRipple,   0.45f },
        { pid::fluidX,        bip (0.75f) },
        { pid::fluidY,        bip (0.70f) },

        { pid::tideRate,      tideHz (0.8f) },
        { pid::tideDepth,     0.25f },
        { pid::currRate,      currHz (2.8f) },
        { pid::currAmount,    0.50f },
        { pid::currSmooth,    0.30f },
        { pid::rippleRate,    rippleHz (10.0f) },
        { pid::rippleDecay,   0.85f },
        { pid::rippleDepth,   0.50f },
        { pid::rippleCycles,  cycles (2.0f) },

        { pid::resoAmount,    0.20f },
        { pid::resoSize,      0.28f },
        { pid::resoDecay,     0.30f },
        { pid::resoDamping,   0.78f },
        { pid::resoScatter,   0.45f },

        { pid::scurEnable,    on },
        { pid::scurAmount,    0.22f },
        { pid::scurWidth,     0.35f },
        { pid::dlyEnable,     on },
        { pid::dlySync,       on },
        { pid::dlySyncTime,   sd (SyncDivision::Sixteenth) },
        { pid::dlyFeedback,   0.30f },
        { pid::dlyDamping,    0.70f },
        { pid::dlyMix,        0.14f },
        { pid::verbEnable,    on },
        { pid::verbSize,      0.40f },
        { pid::verbDecay,     0.35f },
        { pid::verbLowCut,    vLowHz (450.0f) },
        { pid::verbMix,       0.14f },

        { pid::mastLow,       eqdb (2.5f) },
        { pid::mastMid,       eqdb (-2.5f) },
        { pid::mastDrive,     0.42f },
        { pid::mastCeiling,   ceildb (-1.0f) },
        { pid::voiceCount,    vox (1) },
        { pid::glideMode,     gmd (GlideMode::Legato) },
        { pid::glideTime,     gtime (0.04f) },
    },
    {
        { ModSource::ModEnvelope, ModDest::InteractionAmount, 0.70f },
        { ModSource::ModEnvelope, ModDest::FilterDrive,       0.30f },
        { ModSource::Velocity,    ModDest::InteractionAmount, 0.40f },
        { ModSource::Velocity,    ModDest::FilterCutoff,      0.45f },
        { ModSource::Ripple,      ModDest::PitchB,            0.18f },
        { ModSource::Current,     ModDest::InteractionAmount, 0.25f },
        { ModSource::ModWheel,    ModDest::InteractionAmount, 0.50f },
        { ModSource::KeyTrack,    ModDest::SubLevel,         -0.40f },
    }));

    out.push_back (make (kStorms, "THUNDERHEAD", "Drone", { "Dark", "Chaotic", "Cinematic" }, Dry::No,
        "A wall. Seven-voice unison two octaves down, heavy drive, a notch in "
        "the middle, and the longest reverb in the bank. For the two seconds "
        "before something terrible happens.",
    {
        { pid::oscAWave,      wv (OscWave::Saw) },
        { pid::oscAShape,     0.45f },
        { pid::oscAOctave,    oct (-2) },
        { pid::oscAUnison,    uni (7) },
        { pid::oscADetune,    0.42f },
        { pid::oscAStereo,    1.00f },
        { pid::oscALevel,     lvl (-7.0f) },

        { pid::oscBWave,      wv (OscWave::Shark) },
        { pid::oscBOctave,    oct (-2) },
        { pid::oscBFine,      cents (-22.0f) },
        { pid::oscBUnison,    uni (5) },
        { pid::oscBDetune,    0.38f },
        { pid::oscBStereo,    1.00f },
        { pid::oscBLevel,     lvl (-11.0f) },

        { pid::subWave,       sw (SubWave::Sine) },
        { pid::subOctave,     subOct (-2) },
        { pid::subLevel,      lvl (-8.0f) },
        { pid::noiseType,     nz (NoiseType::Deep) },
        { pid::noiseLevel,    lvl (-16.0f) },
        { pid::noiseTone,     0.18f },

        { pid::filtMode,      fm (FilterMode::LP24) },
        { pid::filtCutoff,    hz (380.0f) },
        { pid::filtReso,      0.38f },
        { pid::filtDrive,     0.55f },
        { pid::filtKeyTrack,  0.20f },
        { pid::filtEnvAmt,    bip (0.28f) },
        { pid::filtPressure,  0.90f },

        { pid::fenvAttack,    att (4.00f) },
        { pid::fenvDecay,     dec (10.00f) },
        { pid::fenvSustain,   0.70f },
        { pid::fenvRelease,   rel (8.00f) },

        { pid::aenvAttack,    att (3.00f) },
        { pid::aenvDecay,     dec (8.00f) },
        { pid::aenvSustain,   0.92f },
        { pid::aenvRelease,   rel (9.00f) },
        { pid::aenvVelocity,  0.20f },

        { pid::macroDepth,    0.95f },
        { pid::macroPressure, 0.90f },
        { pid::macroSpace,    0.90f },
        { pid::macroCurrent,  0.60f },
        { pid::macroWet,      0.50f },
        { pid::fluidX,        bip (0.55f) },
        { pid::fluidY,        bip (0.90f) },

        { pid::tideShape,     ts (TideShape::Swell) },
        { pid::tideRate,      tideHz (0.04f) },
        { pid::tideDepth,     0.60f },
        { pid::tideStereo,    0.60f },
        { pid::currRate,      currHz (0.45f) },
        { pid::currAmount,    0.60f },
        { pid::currSmooth,    0.60f },
        { pid::currDrift,     0.60f },
        { pid::driftRate,     driftHz (0.01f) },
        { pid::driftAmount,   0.70f },

        { pid::dropAmount,    0.25f },
        { pid::dropDensity,   0.35f },
        { pid::dropSize,      0.80f },
        { pid::dropTone,      0.20f },
        { pid::dropSplash,    0.60f },
        { pid::dropRandom,    0.85f },
        { pid::dropSpread,    0.95f },

        { pid::resoAmount,    0.34f },
        { pid::resoSize,      0.90f },
        { pid::resoDecay,     0.82f },
        { pid::resoDamping,   0.75f },
        { pid::resoScatter,   0.50f },
        { pid::resoMotion,    0.45f },

        { pid::scurEnable,    on },
        { pid::scurAmount,    0.50f },
        { pid::scurRate,      scurHz (0.04f) },
        { pid::chorEnable,    on },
        { pid::chorRate,      chorHz (0.12f) },
        { pid::chorDepth,     0.55f },
        { pid::chorMix,       0.26f },
        { pid::dlyEnable,     on },
        { pid::dlyTime,       dtime (2.20f) },
        { pid::dlyFeedback,   0.55f },
        { pid::dlyDamping,    0.85f },
        { pid::dlyMix,        0.20f },
        { pid::diffEnable,    on },
        { pid::diffAmount,    0.75f },
        { pid::diffSize,      0.80f },
        { pid::diffMix,       0.38f },
        { pid::verbEnable,    on },
        { pid::verbSize,      1.00f },
        { pid::verbDecay,     0.95f },
        { pid::verbPredelay,  pre (110.0f) },
        { pid::verbDamping,   0.68f },
        { pid::verbLowCut,    vLowHz (55.0f) },
        { pid::verbHighCut,   vHighHz (3000.0f) },
        { pid::verbMod,       0.45f },
        { pid::verbMix,       0.52f },

        { pid::mastLow,       eqdb (3.0f) },
        { pid::mastMid,       eqdb (-3.0f) },
        { pid::mastHigh,      eqdb (-5.0f) },
        { pid::mastDrive,     0.35f },
        { pid::mastCeiling,   ceildb (-1.5f) },
        { pid::voiceCount,    vox (4) },
    },
    {
        { ModSource::Tide,     ModDest::FilterCutoff,   0.40f },
        { ModSource::Tide,     ModDest::Amplitude,      0.20f },
        { ModSource::Drift,    ModDest::FineAll,        0.35f },
        { ModSource::Drift,    ModDest::FilterCutoff,   0.30f },
        { ModSource::Current,  ModDest::FilterDrive,    0.35f },
        { ModSource::Current,  ModDest::PanB,           0.45f },
        { ModSource::Current,  ModDest::NoiseTone,      0.30f },
        { ModSource::ModWheel, ModDest::FilterCutoff,   0.50f },
        { ModSource::Aftertouch, ModDest::FilterDrive,  0.30f },
    }));

    out.push_back (make (kStorms, "MAELSTROM", "Lead", { "Chaotic", "Bright", "Dark" }, Dry::No,
        "A lead that fights back. Ring modulation at a fifth, high resonance, "
        "and the Ripple modulator retriggering on every note so the pitch never "
        "settles immediately. Mod wheel takes it over the edge.",
    {
        { pid::oscAWave,      wv (OscWave::Shark) },
        { pid::oscAShape,     0.58f },
        { pid::oscAUnison,    uni (3) },
        { pid::oscADetune,    0.18f },
        { pid::oscAStereo,    0.60f },
        { pid::oscALevel,     lvl (-4.0f) },

        { pid::oscBWave,      wv (OscWave::Pulse) },
        { pid::oscBShape,     0.28f },
        { pid::oscBSemitone,  semi (7) },
        { pid::oscBLevel,     lvl (-16.0f) },
        { pid::oscBInterMode, im (InteractionMode::RingMod) },
        { pid::oscBInterAmt,  0.45f },

        { pid::subLevel,      lvl (-13.0f) },
        { pid::noiseType,     nz (NoiseType::White) },
        { pid::noiseLevel,    lvl (-28.0f) },
        { pid::noiseTone,     0.60f },

        { pid::filtMode,      fm (FilterMode::LP24) },
        { pid::filtCutoff,    hz (1500.0f) },
        { pid::filtReso,      0.62f },
        { pid::filtDrive,     0.48f },
        { pid::filtKeyTrack,  0.70f },
        { pid::filtEnvAmt,    bip (0.55f) },
        { pid::filtPressure,  0.45f },

        { pid::fenvAttack,    att (0.004f) },
        { pid::fenvDecay,     dec (0.40f) },
        { pid::fenvSustain,   0.30f },
        { pid::fenvRelease,   rel (0.40f) },
        { pid::fenvVelocity,  0.70f },

        { pid::aenvAttack,    att (0.006f) },
        { pid::aenvDecay,     dec (0.90f) },
        { pid::aenvSustain,   0.82f },
        { pid::aenvRelease,   rel (0.50f) },
        { pid::aenvVelocity,  0.60f },

        { pid::macroCurrent,  0.75f },
        { pid::macroRipple,   0.80f },
        { pid::macroDepth,    0.55f },
        { pid::macroGlow,     0.55f },
        { pid::macroSpace,    0.50f },
        { pid::fluidX,        bip (0.95f) },
        { pid::fluidY,        bip (0.30f) },
        { pid::fluidXAmount,  0.95f },

        { pid::tideShape,     ts (TideShape::Triangle) },
        { pid::tideRate,      tideHz (5.5f) },
        { pid::tideDepth,     0.25f },
        { pid::tideStereo,    0.50f },
        { pid::currRate,      currHz (5.0f) },
        { pid::currAmount,    0.75f },
        { pid::currSmooth,    0.20f },
        { pid::currStereo,    0.75f },
        { pid::rippleRate,    rippleHz (18.0f) },
        { pid::rippleDecay,   0.60f },
        { pid::rippleDepth,   0.55f },
        { pid::rippleCycles,  cycles (10.0f) },
        { pid::rippleSpread,  0.65f },
        { pid::ripplePolarity, on },

        { pid::dropAmount,    0.25f },
        { pid::dropMode,      dm (DropletMode::Note) },
        { pid::dropDensity,   0.50f },
        { pid::dropSize,      0.30f },
        { pid::dropTone,      0.65f },
        { pid::dropSplash,    0.75f },
        { pid::dropRandom,    0.85f },

        { pid::resoAmount,    0.30f },
        { pid::resoSize,      0.38f },
        { pid::resoDecay,     0.48f },
        { pid::resoDamping,   0.42f },
        { pid::resoScatter,   0.75f },
        { pid::resoMotion,    0.70f },

        { pid::scurEnable,    on },
        { pid::scurAmount,    0.40f },
        { pid::chorEnable,    on },
        { pid::chorRate,      chorHz (2.0f) },
        { pid::chorDepth,     0.30f },
        { pid::chorMix,       0.20f },
        { pid::dlyEnable,     on },
        { pid::dlySync,       on },
        { pid::dlySyncTime,   sd (SyncDivision::EighthT) },
        { pid::dlyFeedback,   0.58f },
        { pid::dlyMotion,     0.70f },
        { pid::dlySpread,     0.85f },
        { pid::dlyMix,        0.26f },
        { pid::diffEnable,    on },
        { pid::diffMix,       0.24f },
        { pid::verbEnable,    on },
        { pid::verbSize,      0.62f },
        { pid::verbDecay,     0.58f },
        { pid::verbMix,       0.28f },

        { pid::mastMid,       eqdb (-1.5f) },
        { pid::mastDrive,     0.34f },
        { pid::mastCeiling,   ceildb (-1.0f) },
        { pid::voiceCount,    vox (1) },
        { pid::glideMode,     gmd (GlideMode::Legato) },
        { pid::glideTime,     gtime (0.06f) },
        { pid::bendRange,     bend (12) },
    },
    {
        { ModSource::Ripple,      ModDest::PitchAll,          0.14f },
        { ModSource::Ripple,      ModDest::FilterCutoff,      0.45f },
        { ModSource::Ripple,      ModDest::InteractionAmount, 0.35f },
        { ModSource::Current,     ModDest::FilterCutoff,      0.50f },
        { ModSource::Current,     ModDest::ResonatorScatter,  0.45f },
        { ModSource::Tide,        ModDest::ShapeA,            0.30f },
        { ModSource::ModWheel,    ModDest::InteractionAmount, 0.55f },
        { ModSource::ModWheel,    ModDest::FilterResonance,   0.35f },
        { ModSource::Aftertouch,  ModDest::FilterCutoff,      0.40f },
        { ModSource::Velocity,    ModDest::FilterCutoff,      0.40f },
        { ModSource::ModEnvelope, ModDest::ShapeB,            0.35f },
    }));
}

} // anonymous namespace

//==============================================================================
//  Registry
//==============================================================================
const std::vector<Preset>& getPresets()
{
    // Built once, on first use, on whichever thread asks first — which is the
    // message thread, because PresetManager is a message-thread object. After
    // this the table is read-only and handed out by const reference.
    static const std::vector<Preset> presets = []
    {
        std::vector<Preset> p;
        p.reserve (48);

        addSurfaceBank (p);
        addShallowBank (p);
        addDeepBlueBank (p);
        addAbyssBank (p);
        addDropletsBank (p);
        addCurrentsBank (p);
        addBioluminescenceBank (p);
        addStormsBank (p);

        return p;
    }();

    return presets;
}

int indexOfPreset (const juce::String& name)
{
    const auto& p = getPresets();

    for (size_t i = 0; i < p.size(); ++i)
        if (p[i].info.name.equalsIgnoreCase (name))
            return (int) i;

    return -1;
}

int getDefaultPresetIndex()
{
    static const int index = juce::jmax (0, indexOfPreset ("SUBMERGED DREAMS"));
    return index;
}

int getInitPresetIndex()
{
    static const int index = juce::jmax (0, indexOfPreset ("INIT DEEP SAW"));
    return index;
}

juce::StringArray getBankNames()
{
    return { kSurface, kShallow, kDeepBlue, kAbyss,
             kDroplets, kCurrents, kBiolum, kStorms };
}

juce::StringArray getCategoryNames()
{
    return { "Pad", "Key", "Pluck", "Bass", "Lead", "Arp", "Texture", "Drone", "FX" };
}

juce::StringArray getTagVocabulary()
{
    return { "Deep", "Wet", "Dark", "Bright", "Dreamy", "Glassy",
             "Organic", "Chaotic", "Calm", "Cinematic", "Submerged", "Surface" };
}

} // namespace ripples::factory
