#pragma once

/*
    RIPPLES — Parameter identifiers.

    This is the single source of truth for every automatable parameter in the
    plug-in. Nothing else may invent a parameter string. Adding a parameter means
    adding it here first, then to ParameterLayout.cpp, then to ParameterCache.

    Naming convention:  <module><Parameter>   e.g. "oscATide", "filtCutoff".
    IDs are stable and must never change once shipped — presets key off them.
*/

#include <juce_core/juce_core.h>

namespace ripples::pid
{

//==============================================================================
// Versioning — bumped when the parameter set changes in a way presets care about.
inline constexpr int kParameterVersion = 1;

//==============================================================================
// OSCILLATOR A — "TIDE"
inline constexpr auto oscAWave      = "oscAWave";
inline constexpr auto oscAShape     = "oscAShape";
inline constexpr auto oscAOctave    = "oscAOctave";
inline constexpr auto oscASemitone  = "oscASemitone";
inline constexpr auto oscAFine      = "oscAFine";
inline constexpr auto oscAPhase     = "oscAPhase";
inline constexpr auto oscAUnison    = "oscAUnison";
inline constexpr auto oscADetune    = "oscADetune";
inline constexpr auto oscAStereo    = "oscAStereo";
inline constexpr auto oscAPan       = "oscAPan";
inline constexpr auto oscALevel     = "oscALevel";

//==============================================================================
// OSCILLATOR B — "CURRENT"
inline constexpr auto oscBWave      = "oscBWave";
inline constexpr auto oscBShape     = "oscBShape";
inline constexpr auto oscBOctave    = "oscBOctave";
inline constexpr auto oscBSemitone  = "oscBSemitone";
inline constexpr auto oscBFine      = "oscBFine";
inline constexpr auto oscBPhase     = "oscBPhase";
inline constexpr auto oscBUnison    = "oscBUnison";
inline constexpr auto oscBDetune    = "oscBDetune";
inline constexpr auto oscBStereo    = "oscBStereo";
inline constexpr auto oscBPan       = "oscBPan";
inline constexpr auto oscBLevel     = "oscBLevel";
inline constexpr auto oscBInterMode = "oscBInterMode";
inline constexpr auto oscBInterAmt  = "oscBInterAmt";

//==============================================================================
// SUB OSCILLATOR
inline constexpr auto subWave       = "subWave";
inline constexpr auto subOctave     = "subOctave";
inline constexpr auto subLevel      = "subLevel";

//==============================================================================
// WATER NOISE
inline constexpr auto noiseType     = "noiseType";
inline constexpr auto noiseLevel    = "noiseLevel";
inline constexpr auto noiseTone     = "noiseTone";

//==============================================================================
// DEPTH FILTER
inline constexpr auto filtMode      = "filtMode";
inline constexpr auto filtCutoff    = "filtCutoff";
inline constexpr auto filtReso      = "filtReso";
inline constexpr auto filtDrive     = "filtDrive";
inline constexpr auto filtKeyTrack  = "filtKeyTrack";
inline constexpr auto filtEnvAmt    = "filtEnvAmt";
inline constexpr auto filtMovement  = "filtMovement";
inline constexpr auto filtPressure  = "filtPressure";

//==============================================================================
// FILTER (MOD) ENVELOPE
inline constexpr auto fenvAttack    = "fenvAttack";
inline constexpr auto fenvDecay     = "fenvDecay";
inline constexpr auto fenvSustain   = "fenvSustain";
inline constexpr auto fenvRelease   = "fenvRelease";
inline constexpr auto fenvVelocity  = "fenvVelocity";

//==============================================================================
// AMP ENVELOPE
inline constexpr auto aenvAttack    = "aenvAttack";
inline constexpr auto aenvDecay     = "aenvDecay";
inline constexpr auto aenvSustain   = "aenvSustain";
inline constexpr auto aenvRelease   = "aenvRelease";
inline constexpr auto aenvVelocity  = "aenvVelocity";

//==============================================================================
// EIGHT PRIMARY MACROS
inline constexpr auto macroDepth    = "macroDepth";
inline constexpr auto macroWet      = "macroWet";
inline constexpr auto macroRipple   = "macroRipple";
inline constexpr auto macroCurrent  = "macroCurrent";
inline constexpr auto macroDrops    = "macroDrops";
inline constexpr auto macroPressure = "macroPressure";
inline constexpr auto macroSpace    = "macroSpace";
inline constexpr auto macroGlow     = "macroGlow";

//==============================================================================
// FLUID FIELD  (X: calm<->chaos, Y: surface<->depth, plus per-preset depths)
inline constexpr auto fluidX        = "fluidX";
inline constexpr auto fluidY        = "fluidY";
inline constexpr auto fluidXAmount  = "fluidXAmount";
inline constexpr auto fluidYAmount  = "fluidYAmount";

//==============================================================================
// TIDE MODULATOR (smooth deterministic LFO)
inline constexpr auto tideShape     = "tideShape";
inline constexpr auto tideRate      = "tideRate";
inline constexpr auto tideSync      = "tideSync";
inline constexpr auto tideSyncRate  = "tideSyncRate";
inline constexpr auto tidePhase     = "tidePhase";
inline constexpr auto tideDepth     = "tideDepth";
inline constexpr auto tideStereo    = "tideStereo";

//==============================================================================
// CURRENT MODULATOR (smooth correlated randomness)
inline constexpr auto currRate      = "currRate";
inline constexpr auto currAmount    = "currAmount";
inline constexpr auto currSmooth    = "currSmooth";
inline constexpr auto currDrift     = "currDrift";
inline constexpr auto currStereo    = "currStereo";

//==============================================================================
// DRIFT MODULATOR (very slow random motion)
inline constexpr auto driftRate     = "driftRate";
inline constexpr auto driftAmount   = "driftAmount";
inline constexpr auto driftStereo   = "driftStereo";

//==============================================================================
// RIPPLE MODULATOR (triggered damped wave)
inline constexpr auto rippleRate     = "rippleRate";
inline constexpr auto rippleDecay    = "rippleDecay";
inline constexpr auto rippleDepth    = "rippleDepth";
inline constexpr auto rippleCycles   = "rippleCycles";
inline constexpr auto rippleSpread   = "rippleSpread";
inline constexpr auto ripplePolarity = "ripplePolarity";
inline constexpr auto rippleTrigger  = "rippleTrigger";

//==============================================================================
// DROPLET ENGINE
inline constexpr auto dropAmount    = "dropAmount";
inline constexpr auto dropDensity   = "dropDensity";
inline constexpr auto dropSize      = "dropSize";
inline constexpr auto dropTone      = "dropTone";
inline constexpr auto dropSplash    = "dropSplash";
inline constexpr auto dropGravity   = "dropGravity";
inline constexpr auto dropBounce    = "dropBounce";
inline constexpr auto dropRandom    = "dropRandom";
inline constexpr auto dropSpread    = "dropSpread";
inline constexpr auto dropMode      = "dropMode";

//==============================================================================
// WATER RESONATOR
inline constexpr auto resoAmount    = "resoAmount";
inline constexpr auto resoSize      = "resoSize";
inline constexpr auto resoDecay     = "resoDecay";
inline constexpr auto resoDamping   = "resoDamping";
inline constexpr auto resoScatter   = "resoScatter";
inline constexpr auto resoMotion    = "resoMotion";

//==============================================================================
// GLOBAL FX — STEREO CURRENT
inline constexpr auto scurEnable    = "scurEnable";
inline constexpr auto scurAmount    = "scurAmount";
inline constexpr auto scurRate      = "scurRate";
inline constexpr auto scurWidth     = "scurWidth";

// GLOBAL FX — LIQUID CHORUS
inline constexpr auto chorEnable    = "chorEnable";
inline constexpr auto chorRate      = "chorRate";
inline constexpr auto chorDepth     = "chorDepth";
inline constexpr auto chorDelay     = "chorDelay";
inline constexpr auto chorFeedback  = "chorFeedback";
inline constexpr auto chorWidth     = "chorWidth";
inline constexpr auto chorMix       = "chorMix";

// GLOBAL FX — LIQUID DELAY
inline constexpr auto dlyEnable     = "dlyEnable";
inline constexpr auto dlyTime       = "dlyTime";
inline constexpr auto dlySync       = "dlySync";
inline constexpr auto dlySyncTime   = "dlySyncTime";
inline constexpr auto dlyFeedback   = "dlyFeedback";
inline constexpr auto dlyMotion     = "dlyMotion";
inline constexpr auto dlySpread     = "dlySpread";
inline constexpr auto dlyDamping    = "dlyDamping";
inline constexpr auto dlyDiffusion  = "dlyDiffusion";
inline constexpr auto dlyMix        = "dlyMix";

// GLOBAL FX — DIFFUSION
inline constexpr auto diffEnable    = "diffEnable";
inline constexpr auto diffAmount    = "diffAmount";
inline constexpr auto diffSize      = "diffSize";
inline constexpr auto diffDamping   = "diffDamping";
inline constexpr auto diffMix       = "diffMix";

// GLOBAL FX — ABYSS REVERB
inline constexpr auto verbEnable    = "verbEnable";
inline constexpr auto verbSize      = "verbSize";
inline constexpr auto verbDecay     = "verbDecay";
inline constexpr auto verbPredelay  = "verbPredelay";
inline constexpr auto verbDamping   = "verbDamping";
inline constexpr auto verbLowCut    = "verbLowCut";
inline constexpr auto verbHighCut   = "verbHighCut";
inline constexpr auto verbMod       = "verbMod";
inline constexpr auto verbMix       = "verbMix";

// MASTER SHAPER
inline constexpr auto mastLow       = "mastLow";
inline constexpr auto mastMid       = "mastMid";
inline constexpr auto mastHigh      = "mastHigh";
inline constexpr auto mastDrive     = "mastDrive";
inline constexpr auto mastCeiling   = "mastCeiling";
inline constexpr auto mastOutput    = "mastOutput";

//==============================================================================
// GLOBAL / VOICE
inline constexpr auto glideTime     = "glideTime";
inline constexpr auto glideMode     = "glideMode";
inline constexpr auto voiceCount    = "voiceCount";
inline constexpr auto bendRange     = "bendRange";
inline constexpr auto masterTune    = "masterTune";

//==============================================================================
// MODULATION MATRIX — 12 slots, IDs built at compile time.
inline constexpr int kNumModSlots = 12;

// These return stable juce::String IDs: "mod0Source", "mod0Dest", ...
inline juce::String modSourceID (int slot) { return "mod" + juce::String (slot) + "Source"; }
inline juce::String modDestID   (int slot) { return "mod" + juce::String (slot) + "Dest"; }
inline juce::String modAmountID (int slot) { return "mod" + juce::String (slot) + "Amount"; }
inline juce::String modBipolarID(int slot) { return "mod" + juce::String (slot) + "Bipolar"; }

} // namespace ripples::pid
