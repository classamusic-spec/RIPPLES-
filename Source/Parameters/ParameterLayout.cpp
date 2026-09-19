#include "Parameters/ParameterLayout.h"
#include "Parameters/ParameterIDs.h"
#include "Parameters/ParameterEnums.h"
#include "Utilities/DSPConstants.h"
#include "Utilities/MathUtils.h"

#include <cmath>

namespace ripples
{
namespace
{

using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
using FAttr  = juce::AudioParameterFloatAttributes;
using IAttr  = juce::AudioParameterIntAttributes;
using CAttr  = juce::AudioParameterChoiceAttributes;
using BAttr  = juce::AudioParameterBoolAttributes;

constexpr int kV = kParameterVersionHint;

/** One degree, as a fraction of a cycle. The oscillator start-phase knob has a
    single "Free" detent one step below 0 deg, so that free-running does not eat
    half the knob's travel. */
constexpr float kPhaseStep = 1.0f / 360.0f;

//==============================================================================
// Range helpers
//==============================================================================

/** Linear range, optional snapping interval. */
juce::NormalisableRange<float> linRange (float lo, float hi, float interval = 0.0f)
{
    return juce::NormalisableRange<float> (lo, hi, interval);
}

/** Range whose midpoint sits on `centre` — the musical way to skew a knob. */
juce::NormalisableRange<float> skewRange (float lo, float hi, float centre, float interval = 0.0f)
{
    juce::NormalisableRange<float> r (lo, hi, interval);
    r.setSkewForCentre (centre);
    return r;
}

/** 0..1 unipolar. */
juce::NormalisableRange<float> unitRange()     { return linRange (0.0f, 1.0f); }
/** -1..1 bipolar. */
juce::NormalisableRange<float> bipolarRange()  { return linRange (-1.0f, 1.0f); }

//==============================================================================
// Value <-> text
//==============================================================================

/** UTF-8 degree sign, built on demand (no static juce::String at namespace scope). */
juce::String degreeSign() { return juce::String::fromUTF8 ("\xc2\xb0"); }

juce::String freqToText (float hz, int)
{
    if (hz >= 10000.0f) return juce::String (hz / 1000.0f, 1) + " kHz";
    if (hz >= 1000.0f)  return juce::String (hz / 1000.0f, 2) + " kHz";
    if (hz >= 100.0f)   return juce::String (hz, 0) + " Hz";
    if (hz >= 10.0f)    return juce::String (hz, 1) + " Hz";
    return juce::String (hz, 2) + " Hz";
}

float textToFreq (const juce::String& text)
{
    const auto s = text.trim().toLowerCase();
    const float v = s.getFloatValue();
    return s.containsChar ('k') ? v * 1000.0f : v;
}

/** Modulator rates: sub-Hz values need more digits than audio frequencies. */
juce::String rateToText (float hz, int)
{
    if (hz < 1.0f)  return juce::String (hz, 3) + " Hz";
    if (hz < 10.0f) return juce::String (hz, 2) + " Hz";
    return juce::String (hz, 1) + " Hz";
}

/** Seconds stored natively, shown as ms below 1 s. */
juce::String secondsToText (float seconds, int)
{
    if (seconds < 0.0005f) return "0 ms";
    if (seconds < 0.1f)    return juce::String (seconds * 1000.0f, 1) + " ms";
    if (seconds < 1.0f)    return juce::String (seconds * 1000.0f, 0) + " ms";
    if (seconds < 10.0f)   return juce::String (seconds, 2) + " s";
    return juce::String (seconds, 1) + " s";
}

float textToSeconds (const juce::String& text)
{
    const auto s = text.trim().toLowerCase();
    const float v = s.getFloatValue();

    if (s.contains ("ms"))    return v * 0.001f;
    if (s.containsChar ('s')) return v;
    // No unit: a big bare number almost certainly means milliseconds.
    return v > 20.0f ? v * 0.001f : v;
}

/** Milliseconds stored natively (chorus delay, reverb predelay). */
juce::String msToText (float ms, int)
{
    return ms < 10.0f ? juce::String (ms, 2) + " ms"
                      : juce::String (ms, 1) + " ms";
}

float textToMs (const juce::String& text)
{
    const auto s = text.trim().toLowerCase();
    const float v = s.getFloatValue();

    if (s.contains ("ms"))    return v;
    if (s.containsChar ('s')) return v * 1000.0f;   // "0.4 s"
    return v;
}

/** Level-style dB: the bottom of the range reads as silence. */
juce::String levelDbToText (float db, int)
{
    if (db <= kMinLevelDb + 0.01f) return "-inf dB";
    return (db > 0.0f ? "+" : "") + juce::String (db, 1) + " dB";
}

/** Plain dB (EQ gains, ceiling, output). */
juce::String plainDbToText (float db, int)
{
    return (db > 0.0f ? "+" : "") + juce::String (db, 1) + " dB";
}

float textToDb (const juce::String& text)
{
    const auto s = text.trim().toLowerCase();
    if (s.contains ("inf")) return -100.0f;
    return s.getFloatValue();
}

juce::String percentToText (float v, int)
{
    return juce::String (v * 100.0f, 0) + " %";
}

juce::String bipolarPercentToText (float v, int)
{
    const auto body = juce::String (v * 100.0f, 0);
    return (v > 0.0f ? "+" + body : body) + " %";
}

float textToPercent (const juce::String& text)
{
    return text.trim().getFloatValue() * 0.01f;
}

juce::String centsToText (float cents, int)
{
    return (cents > 0.0f ? "+" : "") + juce::String (cents, 1) + " ct";
}

float textToCents (const juce::String& text)
{
    return text.trim().getFloatValue();
}

/** Osc start phase: negative means free-running. */
juce::String startPhaseToText (float v, int)
{
    if (v < 0.0f) return "Free";
    return juce::String (v * 360.0f, 0) + degreeSign();
}

float textToStartPhase (const juce::String& text)
{
    const auto s = text.trim().toLowerCase();
    if (s.startsWithChar ('f')) return -kPhaseStep;
    return juce::jlimit (0.0f, 1.0f, s.getFloatValue() / 360.0f);
}

/** 0..1 phase offset shown in degrees. */
juce::String phase01ToText (float v, int)
{
    return juce::String (v * 360.0f, 0) + degreeSign();
}

float textToPhase01 (const juce::String& text)
{
    return juce::jlimit (0.0f, 1.0f, text.trim().getFloatValue() / 360.0f);
}

juce::String panToText (float pan, int)
{
    if (std::abs (pan) < 0.005f) return "C";
    const auto amount = juce::String (std::abs (pan) * 100.0f, 0);
    return (pan < 0.0f ? "L " : "R ") + amount;
}

float textToPan (const juce::String& text)
{
    const auto s = text.trim().toUpperCase();
    if (s.startsWithChar ('C')) return 0.0f;

    const float magnitude = s.retainCharacters ("0123456789.").getFloatValue() * 0.01f;
    if (s.startsWithChar ('L')) return -magnitude;
    if (s.startsWithChar ('R')) return  magnitude;

    const float v = s.getFloatValue();
    return std::abs (v) <= 1.0f ? v : v * 0.01f;
}

juce::String cyclesToText (float v, int)
{
    return juce::String (v, 1) + " cyc";
}

float textToCycles (const juce::String& text) { return text.trim().getFloatValue(); }

juce::String semitoneToText (int v, int)
{
    return (v > 0 ? "+" : "") + juce::String (v) + " st";
}

juce::String octaveToText (int v, int)
{
    if (v == 0) return "0 oct";
    return (v > 0 ? "+" : "") + juce::String (v) + " oct";
}

juce::String voiceCountToText (int v, int)
{
    return juce::String (v) + (v == 1 ? " voice" : " voices");
}

juce::String bendRangeToText (int v, int)
{
    return juce::String (v) + " st";
}

int textToInt (const juce::String& text) { return text.trim().getIntValue(); }

juce::String onOffToText  (bool v, int) { return v ? "On" : "Off"; }
juce::String syncToText   (bool v, int) { return v ? "Sync" : "Free"; }
juce::String invertToText (bool v, int) { return v ? "Inverted" : "Normal"; }

//==============================================================================
// Attribute shorthands
//==============================================================================

/*  NOTE ON UNITS: every conversion below embeds the unit in the value string
    (getText()), and deliberately leaves juce's separate `label` empty. Hosts
    that append the label to the value text would otherwise show "1.20 kHz Hz".
*/
FAttr percentAttrs (bool bipolar = false)
{
    return FAttr().withStringFromValueFunction (bipolar ? &bipolarPercentToText : &percentToText)
                  .withValueFromStringFunction (&textToPercent);
}

FAttr freqAttrs()
{
    return FAttr().withStringFromValueFunction (&freqToText)
                  .withValueFromStringFunction (&textToFreq);
}

FAttr rateAttrs()
{
    return FAttr().withStringFromValueFunction (&rateToText)
                  .withValueFromStringFunction (&textToFreq);
}

FAttr secondsAttrs()
{
    return FAttr().withStringFromValueFunction (&secondsToText)
                  .withValueFromStringFunction (&textToSeconds);
}

FAttr msAttrs()
{
    return FAttr().withStringFromValueFunction (&msToText)
                  .withValueFromStringFunction (&textToMs);
}

FAttr levelDbAttrs()
{
    return FAttr().withStringFromValueFunction (&levelDbToText)
                  .withValueFromStringFunction (&textToDb);
}

FAttr plainDbAttrs()
{
    return FAttr().withStringFromValueFunction (&plainDbToText)
                  .withValueFromStringFunction (&textToDb);
}

FAttr centsAttrs()
{
    return FAttr().withStringFromValueFunction (&centsToText)
                  .withValueFromStringFunction (&textToCents);
}

FAttr panAttrs()
{
    return FAttr().withStringFromValueFunction (&panToText)
                  .withValueFromStringFunction (&textToPan);
}

//==============================================================================
// Adders
//==============================================================================

void addFloat (Layout& layout, const juce::String& id, const juce::String& name,
               juce::NormalisableRange<float> range, float defaultValue, FAttr attrs)
{
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, kV }, name, range, defaultValue, std::move (attrs)));
}

/** 0..1 knob displayed as a percentage. */
void addPercent (Layout& layout, const juce::String& id, const juce::String& name, float defaultValue)
{
    addFloat (layout, id, name, unitRange(), defaultValue, percentAttrs (false));
}

/** -1..1 knob displayed as a signed percentage. */
void addBipolarPercent (Layout& layout, const juce::String& id, const juce::String& name, float defaultValue)
{
    addFloat (layout, id, name, bipolarRange(), defaultValue, percentAttrs (true));
}

void addInt (Layout& layout, const juce::String& id, const juce::String& name,
             int lo, int hi, int defaultValue, IAttr attrs)
{
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { id, kV }, name, lo, hi, defaultValue, std::move (attrs)));
}

void addChoice (Layout& layout, const juce::String& id, const juce::String& name,
                const juce::StringArray& items, int defaultIndex)
{
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { id, kV }, name, items, defaultIndex, CAttr()));
}

void addBool (Layout& layout, const juce::String& id, const juce::String& name,
              bool defaultValue, juce::String (*toText) (bool, int) = &onOffToText)
{
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { id, kV }, name, defaultValue,
        BAttr().withStringFromValueFunction (toText)));
}

//==============================================================================
// One oscillator's worth of parameters (TIDE / CURRENT share the same shape).
//==============================================================================

struct OscIDs
{
    const char* wave; const char* shape; const char* octave; const char* semitone;
    const char* fine; const char* phase;  const char* unison; const char* detune;
    const char* stereo; const char* pan;  const char* level;
};

struct OscDefaults
{
    OscWave wave; float shape; int octave; int semitone; float fine; float phase;
    int unison; float detune; float stereo; float pan; float levelDb;
};

void addOscillator (Layout& layout, const OscIDs& ids, const juce::String& prefix, const OscDefaults& d)
{
    addChoice (layout, ids.wave, prefix + " Wave", toStringArray (oscWaveNames), (int) d.wave);

    addPercent (layout, ids.shape, prefix + " Shape", d.shape);

    addInt (layout, ids.octave, prefix + " Octave", -3, 3, d.octave,
            IAttr().withStringFromValueFunction (&octaveToText)
                   .withValueFromStringFunction (&textToInt));

    addInt (layout, ids.semitone, prefix + " Semitone", -12, 12, d.semitone,
            IAttr().withStringFromValueFunction (&semitoneToText)
                   .withValueFromStringFunction (&textToInt));

    addFloat (layout, ids.fine, prefix + " Fine", linRange (-100.0f, 100.0f, 0.1f), d.fine, centsAttrs());

    addFloat (layout, ids.phase, prefix + " Phase",
              linRange (-kPhaseStep, 1.0f, kPhaseStep), d.phase,
              FAttr().withStringFromValueFunction (&startPhaseToText)
                     .withValueFromStringFunction (&textToStartPhase));

    addInt (layout, ids.unison, prefix + " Unison", 1, dsp::kMaxUnison, d.unison,
            IAttr().withStringFromValueFunction (&voiceCountToText)
                   .withValueFromStringFunction (&textToInt));

    addPercent (layout, ids.detune, prefix + " Detune", d.detune);
    addPercent (layout, ids.stereo, prefix + " Stereo", d.stereo);

    addFloat (layout, ids.pan, prefix + " Pan", bipolarRange(), d.pan, panAttrs());

    addFloat (layout, ids.level, prefix + " Level",
              skewRange (kMinLevelDb, kMaxLevelDb, -12.0f, 0.1f), d.levelDb, levelDbAttrs());
}

} // anonymous namespace

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    Layout layout;

    //==========================================================================
    // OSCILLATOR A — TIDE.  INIT DEEP SAW: a plain saw at unity.
    {
        const OscIDs ids { pid::oscAWave, pid::oscAShape, pid::oscAOctave, pid::oscASemitone,
                           pid::oscAFine, pid::oscAPhase, pid::oscAUnison, pid::oscADetune,
                           pid::oscAStereo, pid::oscAPan, pid::oscALevel };

        const OscDefaults defaults { OscWave::Saw, 0.5f, 0, 0, 0.0f, -kPhaseStep,
                                     1, 0.25f, 0.5f, 0.0f, 0.0f };

        addOscillator (layout, ids, "Osc A", defaults);
    }

    //==========================================================================
    // OSCILLATOR B — CURRENT.  Second saw, detuned the other way, slightly quieter.
    {
        const OscIDs ids { pid::oscBWave, pid::oscBShape, pid::oscBOctave, pid::oscBSemitone,
                           pid::oscBFine, pid::oscBPhase, pid::oscBUnison, pid::oscBDetune,
                           pid::oscBStereo, pid::oscBPan, pid::oscBLevel };

        const OscDefaults defaults { OscWave::Saw, 0.5f, 0, 0, 7.0f, -kPhaseStep,
                                     1, 0.25f, 0.5f, 0.0f, -1.5f };

        addOscillator (layout, ids, "Osc B", defaults);
    }

    addChoice  (layout, pid::oscBInterMode, "Osc B Interaction",
                toStringArray (interactionNames), (int) InteractionMode::Normal);
    addPercent (layout, pid::oscBInterAmt, "Osc B Interaction Amount", 0.5f);

    //==========================================================================
    // SUB OSCILLATOR — quiet sine, one octave down.
    addChoice (layout, pid::subWave, "Sub Wave", toStringArray (subWaveNames), (int) SubWave::Sine);

    addInt (layout, pid::subOctave, "Sub Octave", -2, -1, -1,
            IAttr().withStringFromValueFunction (&octaveToText)
                   .withValueFromStringFunction (&textToInt));

    addFloat (layout, pid::subLevel, "Sub Level",
              skewRange (kMinLevelDb, kMaxLevelDb, -12.0f, 0.1f), -12.0f, levelDbAttrs());

    //==========================================================================
    // WATER NOISE — silent at init.
    addChoice (layout, pid::noiseType, "Noise Type", toStringArray (noiseTypeNames), (int) NoiseType::White);

    addFloat (layout, pid::noiseLevel, "Noise Level",
              skewRange (kMinLevelDb, kMaxLevelDb, -12.0f, 0.1f), kMinLevelDb, levelDbAttrs());

    addPercent (layout, pid::noiseTone, "Noise Tone", 0.5f);

    //==========================================================================
    // DEPTH FILTER — LP24, moderate cutoff, moderate envelope.
    addChoice (layout, pid::filtMode, "Filter Mode", toStringArray (filterModeNames), (int) FilterMode::LP24);

    addFloat (layout, pid::filtCutoff, "Filter Cutoff",
              skewRange (dsp::kMinCutoffHz, dsp::kMaxCutoffHz, 1000.0f), 1200.0f, freqAttrs());

    addPercent        (layout, pid::filtReso,      "Filter Resonance", 0.15f);
    addPercent        (layout, pid::filtDrive,     "Filter Drive",     0.12f);
    addPercent        (layout, pid::filtKeyTrack,  "Filter Key Track", 0.5f);
    addBipolarPercent (layout, pid::filtEnvAmt,    "Filter Env Amount", 0.35f);
    addPercent        (layout, pid::filtMovement,  "Filter Movement",  0.5f);
    addPercent        (layout, pid::filtPressure,  "Filter Pressure",  0.0f);

    //==========================================================================
    // FILTER (MOD) ENVELOPE — snappy attack, long-ish decay to a low sustain.
    addFloat (layout, pid::fenvAttack, "Filter Attack",
              skewRange (dsp::kMinEnvTimeSec, dsp::kMaxAttackSec, 0.15f), 0.002f, secondsAttrs());
    addFloat (layout, pid::fenvDecay, "Filter Decay",
              skewRange (dsp::kMinEnvTimeSec, dsp::kMaxDecaySec, 0.5f), 0.60f, secondsAttrs());
    addPercent (layout, pid::fenvSustain, "Filter Sustain", 0.30f);
    addFloat (layout, pid::fenvRelease, "Filter Release",
              skewRange (dsp::kMinEnvTimeSec, dsp::kMaxReleaseSec, 0.6f), 0.40f, secondsAttrs());
    addPercent (layout, pid::fenvVelocity, "Filter Velocity", 0.30f);

    //==========================================================================
    // AMP ENVELOPE — fast but click-free, high sustain, medium release.
    addFloat (layout, pid::aenvAttack, "Amp Attack",
              skewRange (dsp::kMinEnvTimeSec, dsp::kMaxAttackSec, 0.15f), 0.005f, secondsAttrs());
    addFloat (layout, pid::aenvDecay, "Amp Decay",
              skewRange (dsp::kMinEnvTimeSec, dsp::kMaxDecaySec, 0.5f), 1.00f, secondsAttrs());
    addPercent (layout, pid::aenvSustain, "Amp Sustain", 0.80f);
    addFloat (layout, pid::aenvRelease, "Amp Release",
              skewRange (dsp::kMinEnvTimeSec, dsp::kMaxReleaseSec, 0.6f), 0.35f, secondsAttrs());
    addPercent (layout, pid::aenvVelocity, "Amp Velocity", 0.50f);

    //==========================================================================
    // EIGHT PRIMARY MACROS — all neutral at init.
    addPercent (layout, pid::macroDepth,    "Depth",    0.0f);
    addPercent (layout, pid::macroWet,      "Wet",      0.0f);
    addPercent (layout, pid::macroRipple,   "Ripple",   0.0f);
    addPercent (layout, pid::macroCurrent,  "Current",  0.0f);
    addPercent (layout, pid::macroDrops,    "Drops",    0.0f);
    addPercent (layout, pid::macroPressure, "Pressure", 0.0f);
    addPercent (layout, pid::macroSpace,    "Space",    0.0f);
    addPercent (layout, pid::macroGlow,     "Glow",     0.0f);

    //==========================================================================
    // FLUID FIELD — node centred, moderate per-preset depths.
    addBipolarPercent (layout, pid::fluidX, "Fluid X", 0.0f);
    addBipolarPercent (layout, pid::fluidY, "Fluid Y", 0.0f);
    addPercent        (layout, pid::fluidXAmount, "Fluid X Amount", 0.5f);
    addPercent        (layout, pid::fluidYAmount, "Fluid Y Amount", 0.5f);

    //==========================================================================
    // TIDE MODULATOR
    addChoice (layout, pid::tideShape, "Tide Shape", toStringArray (tideShapeNames), (int) TideShape::Sine);
    addFloat  (layout, pid::tideRate, "Tide Rate", skewRange (0.01f, 40.0f, 2.0f), 1.0f, rateAttrs());
    addBool   (layout, pid::tideSync, "Tide Sync", false, &syncToText);
    addChoice (layout, pid::tideSyncRate, "Tide Sync Rate",
               toStringArray (syncDivisionNames), (int) SyncDivision::Quarter);
    addFloat  (layout, pid::tidePhase, "Tide Phase", unitRange(), 0.0f,
               FAttr().withStringFromValueFunction (&phase01ToText)
                      .withValueFromStringFunction (&textToPhase01));
    addPercent (layout, pid::tideDepth,  "Tide Depth",  0.5f);
    addPercent (layout, pid::tideStereo, "Tide Stereo", 0.0f);

    //==========================================================================
    // CURRENT MODULATOR
    addFloat   (layout, pid::currRate, "Current Rate", skewRange (0.01f, 20.0f, 1.0f), 0.5f, rateAttrs());
    addPercent (layout, pid::currAmount, "Current Amount", 0.35f);
    addPercent (layout, pid::currSmooth, "Current Smoothness", 0.5f);
    addPercent (layout, pid::currDrift,  "Current Drift", 0.20f);
    addPercent (layout, pid::currStereo, "Current Stereo", 0.5f);

    //==========================================================================
    // DRIFT MODULATOR
    addFloat   (layout, pid::driftRate, "Drift Rate", skewRange (0.001f, 2.0f, 0.05f), 0.05f, rateAttrs());
    addPercent (layout, pid::driftAmount, "Drift Amount", 0.30f);
    addPercent (layout, pid::driftStereo, "Drift Stereo", 0.5f);

    //==========================================================================
    // RIPPLE MODULATOR
    addFloat   (layout, pid::rippleRate, "Ripple Rate", skewRange (0.1f, 50.0f, 4.0f), 4.0f, rateAttrs());
    addPercent (layout, pid::rippleDecay, "Ripple Decay", 0.5f);
    addPercent (layout, pid::rippleDepth, "Ripple Depth", 0.5f);
    addFloat   (layout, pid::rippleCycles, "Ripple Cycles", skewRange (1.0f, 32.0f, 6.0f, 0.5f), 4.0f,
                FAttr().withStringFromValueFunction (&cyclesToText)
                       .withValueFromStringFunction (&textToCycles));
    addPercent (layout, pid::rippleSpread, "Ripple Spread", 0.0f);
    addBool    (layout, pid::ripplePolarity, "Ripple Polarity", false, &invertToText);
    addChoice  (layout, pid::rippleTrigger, "Ripple Trigger",
                toStringArray (rippleTriggerNames), (int) RippleTrigger::NoteOn);

    //==========================================================================
    // DROPLET ENGINE — silent at init, everything else musically parked.
    addPercent (layout, pid::dropAmount,  "Droplet Amount",  0.0f);
    addPercent (layout, pid::dropDensity, "Droplet Density", 0.30f);
    addPercent (layout, pid::dropSize,    "Droplet Size",    0.50f);
    addPercent (layout, pid::dropTone,    "Droplet Tone",    0.50f);
    addPercent (layout, pid::dropSplash,  "Droplet Splash",  0.30f);
    addPercent (layout, pid::dropGravity, "Droplet Gravity", 0.50f);
    addPercent (layout, pid::dropBounce,  "Droplet Bounce",  0.30f);
    addPercent (layout, pid::dropRandom,  "Droplet Random",  0.50f);
    addPercent (layout, pid::dropSpread,  "Droplet Spread",  0.50f);
    addChoice  (layout, pid::dropMode,    "Droplet Mode",
                toStringArray (dropletModeNames), (int) DropletMode::Atmospheric);

    //==========================================================================
    // WATER RESONATOR — off at init.
    addPercent (layout, pid::resoAmount,  "Resonator Amount",  0.0f);
    addPercent (layout, pid::resoSize,    "Resonator Size",    0.50f);
    addPercent (layout, pid::resoDecay,   "Resonator Decay",   0.50f);
    addPercent (layout, pid::resoDamping, "Resonator Damping", 0.50f);
    addPercent (layout, pid::resoScatter, "Resonator Scatter", 0.30f);
    addPercent (layout, pid::resoMotion,  "Resonator Motion",  0.20f);

    //==========================================================================
    // GLOBAL FX — STEREO CURRENT (bypassed at init)
    addBool    (layout, pid::scurEnable, "Stereo Current Enable", false);
    addPercent (layout, pid::scurAmount, "Stereo Current Amount", 0.30f);
    addFloat   (layout, pid::scurRate, "Stereo Current Rate",
                skewRange (0.005f, 5.0f, 0.15f), 0.10f, rateAttrs());
    addPercent (layout, pid::scurWidth, "Stereo Current Width", 0.50f);

    //==========================================================================
    // GLOBAL FX — LIQUID CHORUS (bypassed at init)
    addBool    (layout, pid::chorEnable, "Chorus Enable", false);
    addFloat   (layout, pid::chorRate, "Chorus Rate", skewRange (0.01f, 10.0f, 0.5f), 0.40f, rateAttrs());
    addPercent (layout, pid::chorDepth, "Chorus Depth", 0.40f);
    addFloat   (layout, pid::chorDelay, "Chorus Delay", skewRange (1.0f, 50.0f, 12.0f), 12.0f, msAttrs());
    addPercent (layout, pid::chorFeedback, "Chorus Feedback", 0.15f);
    addPercent (layout, pid::chorWidth, "Chorus Width", 0.60f);
    addPercent (layout, pid::chorMix, "Chorus Mix", 0.35f);

    //==========================================================================
    // GLOBAL FX — LIQUID DELAY (bypassed at init)
    addBool    (layout, pid::dlyEnable, "Delay Enable", false);
    addFloat   (layout, pid::dlyTime, "Delay Time",
                skewRange (0.001f, dsp::kMaxDelaySeconds, 0.35f), 0.40f, secondsAttrs());
    addBool    (layout, pid::dlySync, "Delay Sync", false, &syncToText);
    addChoice  (layout, pid::dlySyncTime, "Delay Sync Time",
                toStringArray (syncDivisionNames), (int) SyncDivision::Eighth);
    addPercent (layout, pid::dlyFeedback,  "Delay Feedback",  0.40f);
    addPercent (layout, pid::dlyMotion,    "Delay Motion",    0.30f);
    addPercent (layout, pid::dlySpread,    "Delay Spread",    0.40f);
    addPercent (layout, pid::dlyDamping,   "Delay Damping",   0.50f);
    addPercent (layout, pid::dlyDiffusion, "Delay Diffusion", 0.30f);
    addPercent (layout, pid::dlyMix,       "Delay Mix",       0.30f);

    //==========================================================================
    // GLOBAL FX — DIFFUSION (bypassed at init)
    addBool    (layout, pid::diffEnable,  "Diffusion Enable",  false);
    addPercent (layout, pid::diffAmount,  "Diffusion Amount",  0.40f);
    addPercent (layout, pid::diffSize,    "Diffusion Size",    0.50f);
    addPercent (layout, pid::diffDamping, "Diffusion Damping", 0.40f);
    addPercent (layout, pid::diffMix,     "Diffusion Mix",     0.30f);

    //==========================================================================
    // GLOBAL FX — ABYSS REVERB (bypassed at init)
    addBool    (layout, pid::verbEnable, "Reverb Enable", false);
    addPercent (layout, pid::verbSize,   "Reverb Size",   0.60f);
    addPercent (layout, pid::verbDecay,  "Reverb Decay",  0.60f);
    addFloat   (layout, pid::verbPredelay, "Reverb Predelay",
                skewRange (0.0f, dsp::kMaxPredelaySeconds * 1000.0f, 40.0f), 20.0f, msAttrs());
    addPercent (layout, pid::verbDamping, "Reverb Damping", 0.50f);
    addFloat   (layout, pid::verbLowCut, "Reverb Low Cut",
                skewRange (20.0f, 1000.0f, 150.0f), 120.0f, freqAttrs());
    addFloat   (layout, pid::verbHighCut, "Reverb High Cut",
                skewRange (1000.0f, dsp::kMaxCutoffHz, 6000.0f), 9000.0f, freqAttrs());
    addPercent (layout, pid::verbMod, "Reverb Modulation", 0.30f);
    addPercent (layout, pid::verbMix, "Reverb Mix", 0.30f);

    //==========================================================================
    // MASTER SHAPER
    addFloat (layout, pid::mastLow,  "Master Low",  linRange (-12.0f, 12.0f, 0.1f), 0.0f, plainDbAttrs());
    addFloat (layout, pid::mastMid,  "Master Mid",  linRange (-12.0f, 12.0f, 0.1f), 0.0f, plainDbAttrs());
    addFloat (layout, pid::mastHigh, "Master High", linRange (-12.0f, 12.0f, 0.1f), 0.0f, plainDbAttrs());
    addPercent (layout, pid::mastDrive, "Master Drive", 0.0f);
    addFloat (layout, pid::mastCeiling, "Master Ceiling", linRange (-12.0f, 0.0f, 0.1f), -0.3f, plainDbAttrs());
    addFloat (layout, pid::mastOutput,  "Master Output",
              linRange (kMinOutputDb, kMaxOutputDb, 0.1f), 0.0f, plainDbAttrs());

    //==========================================================================
    // GLOBAL / VOICE
    addFloat  (layout, pid::glideTime, "Glide Time", skewRange (0.0f, 2.0f, 0.15f), 0.0f, secondsAttrs());
    addChoice (layout, pid::glideMode, "Glide Mode", toStringArray (glideModeNames), (int) GlideMode::Off);

    addInt (layout, pid::voiceCount, "Voices", 1, dsp::kMaxVoices, dsp::kMaxVoices,
            IAttr().withStringFromValueFunction (&voiceCountToText)
                   .withValueFromStringFunction (&textToInt));

    addInt (layout, pid::bendRange, "Bend Range", 0, 24, 2,
            IAttr().withStringFromValueFunction (&bendRangeToText)
                   .withValueFromStringFunction (&textToInt));

    addFloat (layout, pid::masterTune, "Master Tune", linRange (-100.0f, 100.0f, 0.1f), 0.0f, centsAttrs());

    //==========================================================================
    // MODULATION MATRIX — 12 empty slots.
    const auto sourceNames = toStringArray (modSourceNames);
    const auto destNames   = toStringArray (modDestNames);

    for (int slot = 0; slot < pid::kNumModSlots; ++slot)
    {
        const auto label = "Mod " + juce::String (slot + 1) + " ";

        addChoice (layout, pid::modSourceID (slot), label + "Source", sourceNames, (int) ModSource::None);
        addChoice (layout, pid::modDestID   (slot), label + "Dest",   destNames,   (int) ModDest::None);
        addBipolarPercent (layout, pid::modAmountID (slot), label + "Amount", 0.0f);
        addBool   (layout, pid::modBipolarID (slot), label + "Bipolar", false);
    }

    return layout;
}

} // namespace ripples
