#include "Parameters/ParameterCache.h"
#include "Parameters/ParameterLayout.h"
#include "Utilities/DSPConstants.h"
#include "Utilities/MathUtils.h"

#include <cmath>

namespace ripples
{
namespace
{

//==============================================================================
// Raw atomic reads. Relaxed ordering is correct here: each parameter is an
// independent scalar and the block boundary is the only synchronisation point
// that matters.
//==============================================================================

inline float readFloat (const std::atomic<float>* a) noexcept
{
    return a->load (std::memory_order_relaxed);
}

inline int readInt (const std::atomic<float>* a) noexcept
{
    return (int) std::lround (a->load (std::memory_order_relaxed));
}

inline bool readBool (const std::atomic<float>* a) noexcept
{
    return a->load (std::memory_order_relaxed) > 0.5f;
}

/** Reads a choice index and clamps it into the enum's legal range. */
template <typename EnumType>
inline EnumType readEnum (const std::atomic<float>* a, EnumType count) noexcept
{
    const int last  = (int) count - 1;
    const int index = (int) std::lround (a->load (std::memory_order_relaxed));
    return (EnumType) (index < 0 ? 0 : (index > last ? last : index));
}

/** Level-style dB to linear gain; the bottom of the range is true silence. */
inline float levelToGain (float db) noexcept
{
    return db <= kMinLevelDb + 0.01f ? 0.0f : math::decibelsToGain (db);
}

inline double divisionBeats (SyncDivision div) noexcept
{
    const int last  = (int) SyncDivision::NumDivisions - 1;
    const int index = (int) div;
    return syncDivisionBeats[(size_t) (index < 0 ? 0 : (index > last ? last : index))];
}

inline double safeBpm (double bpm) noexcept
{
    return (bpm > 1.0 && bpm < 1000.0) ? bpm : 120.0;
}

} // anonymous namespace

//==============================================================================
std::atomic<float>* ParameterCache::resolve (juce::AudioProcessorValueTreeState& apvts,
                                             juce::StringRef id)
{
    if (auto* ptr = apvts.getRawParameterValue (id))
        return ptr;

    // A null here means ParameterIDs.h and ParameterLayout.cpp have drifted
    // apart. Fall back to a dummy so the audio thread can never dereference
    // null, and assert loudly in a debug build.
    jassertfalse;
    return &fallback;
}

//==============================================================================
void ParameterCache::prepare (juce::AudioProcessorValueTreeState& apvts)
{
    // ---- OSC A -----------------------------------------------------------
    p.oscA.wave     = resolve (apvts, pid::oscAWave);
    p.oscA.shape    = resolve (apvts, pid::oscAShape);
    p.oscA.octave   = resolve (apvts, pid::oscAOctave);
    p.oscA.semitone = resolve (apvts, pid::oscASemitone);
    p.oscA.fine     = resolve (apvts, pid::oscAFine);
    p.oscA.phase    = resolve (apvts, pid::oscAPhase);
    p.oscA.unison   = resolve (apvts, pid::oscAUnison);
    p.oscA.detune   = resolve (apvts, pid::oscADetune);
    p.oscA.stereo   = resolve (apvts, pid::oscAStereo);
    p.oscA.pan      = resolve (apvts, pid::oscAPan);
    p.oscA.level    = resolve (apvts, pid::oscALevel);

    // ---- OSC B -----------------------------------------------------------
    p.oscB.wave     = resolve (apvts, pid::oscBWave);
    p.oscB.shape    = resolve (apvts, pid::oscBShape);
    p.oscB.octave   = resolve (apvts, pid::oscBOctave);
    p.oscB.semitone = resolve (apvts, pid::oscBSemitone);
    p.oscB.fine     = resolve (apvts, pid::oscBFine);
    p.oscB.phase    = resolve (apvts, pid::oscBPhase);
    p.oscB.unison   = resolve (apvts, pid::oscBUnison);
    p.oscB.detune   = resolve (apvts, pid::oscBDetune);
    p.oscB.stereo   = resolve (apvts, pid::oscBStereo);
    p.oscB.pan      = resolve (apvts, pid::oscBPan);
    p.oscB.level    = resolve (apvts, pid::oscBLevel);

    p.interMode = resolve (apvts, pid::oscBInterMode);
    p.interAmt  = resolve (apvts, pid::oscBInterAmt);

    // ---- SUB / NOISE -----------------------------------------------------
    p.subWave   = resolve (apvts, pid::subWave);
    p.subOctave = resolve (apvts, pid::subOctave);
    p.subLevel  = resolve (apvts, pid::subLevel);

    p.noiseType  = resolve (apvts, pid::noiseType);
    p.noiseLevel = resolve (apvts, pid::noiseLevel);
    p.noiseTone  = resolve (apvts, pid::noiseTone);

    // ---- FILTER ----------------------------------------------------------
    p.filtMode     = resolve (apvts, pid::filtMode);
    p.filtCutoff   = resolve (apvts, pid::filtCutoff);
    p.filtReso     = resolve (apvts, pid::filtReso);
    p.filtDrive    = resolve (apvts, pid::filtDrive);
    p.filtKeyTrack = resolve (apvts, pid::filtKeyTrack);
    p.filtEnvAmt   = resolve (apvts, pid::filtEnvAmt);
    p.filtMovement = resolve (apvts, pid::filtMovement);
    p.filtPressure = resolve (apvts, pid::filtPressure);

    // ---- ENVELOPES -------------------------------------------------------
    p.fenvA   = resolve (apvts, pid::fenvAttack);
    p.fenvD   = resolve (apvts, pid::fenvDecay);
    p.fenvS   = resolve (apvts, pid::fenvSustain);
    p.fenvR   = resolve (apvts, pid::fenvRelease);
    p.fenvVel = resolve (apvts, pid::fenvVelocity);

    p.aenvA   = resolve (apvts, pid::aenvAttack);
    p.aenvD   = resolve (apvts, pid::aenvDecay);
    p.aenvS   = resolve (apvts, pid::aenvSustain);
    p.aenvR   = resolve (apvts, pid::aenvRelease);
    p.aenvVel = resolve (apvts, pid::aenvVelocity);

    // ---- MACROS ----------------------------------------------------------
    p.macroDepth    = resolve (apvts, pid::macroDepth);
    p.macroWet      = resolve (apvts, pid::macroWet);
    p.macroRipple   = resolve (apvts, pid::macroRipple);
    p.macroCurrent  = resolve (apvts, pid::macroCurrent);
    p.macroDrops    = resolve (apvts, pid::macroDrops);
    p.macroPressure = resolve (apvts, pid::macroPressure);
    p.macroSpace    = resolve (apvts, pid::macroSpace);
    p.macroGlow     = resolve (apvts, pid::macroGlow);

    // ---- FLUID FIELD -----------------------------------------------------
    p.fluidX       = resolve (apvts, pid::fluidX);
    p.fluidY       = resolve (apvts, pid::fluidY);
    p.fluidXAmount = resolve (apvts, pid::fluidXAmount);
    p.fluidYAmount = resolve (apvts, pid::fluidYAmount);

    // ---- MODULATORS ------------------------------------------------------
    p.tideShape    = resolve (apvts, pid::tideShape);
    p.tideRate     = resolve (apvts, pid::tideRate);
    p.tideSync     = resolve (apvts, pid::tideSync);
    p.tideSyncRate = resolve (apvts, pid::tideSyncRate);
    p.tidePhase    = resolve (apvts, pid::tidePhase);
    p.tideDepth    = resolve (apvts, pid::tideDepth);
    p.tideStereo   = resolve (apvts, pid::tideStereo);

    p.currRate   = resolve (apvts, pid::currRate);
    p.currAmount = resolve (apvts, pid::currAmount);
    p.currSmooth = resolve (apvts, pid::currSmooth);
    p.currDrift  = resolve (apvts, pid::currDrift);
    p.currStereo = resolve (apvts, pid::currStereo);

    p.driftRate   = resolve (apvts, pid::driftRate);
    p.driftAmount = resolve (apvts, pid::driftAmount);
    p.driftStereo = resolve (apvts, pid::driftStereo);

    p.rippleRate     = resolve (apvts, pid::rippleRate);
    p.rippleDecay    = resolve (apvts, pid::rippleDecay);
    p.rippleDepth    = resolve (apvts, pid::rippleDepth);
    p.rippleCycles   = resolve (apvts, pid::rippleCycles);
    p.rippleSpread   = resolve (apvts, pid::rippleSpread);
    p.ripplePolarity = resolve (apvts, pid::ripplePolarity);
    p.rippleTrigger  = resolve (apvts, pid::rippleTrigger);

    // ---- DROPLETS / RESONATOR -------------------------------------------
    p.dropAmount  = resolve (apvts, pid::dropAmount);
    p.dropDensity = resolve (apvts, pid::dropDensity);
    p.dropSize    = resolve (apvts, pid::dropSize);
    p.dropTone    = resolve (apvts, pid::dropTone);
    p.dropSplash  = resolve (apvts, pid::dropSplash);
    p.dropGravity = resolve (apvts, pid::dropGravity);
    p.dropBounce  = resolve (apvts, pid::dropBounce);
    p.dropRandom  = resolve (apvts, pid::dropRandom);
    p.dropSpread  = resolve (apvts, pid::dropSpread);
    p.dropMode    = resolve (apvts, pid::dropMode);

    p.resoAmount  = resolve (apvts, pid::resoAmount);
    p.resoSize    = resolve (apvts, pid::resoSize);
    p.resoDecay   = resolve (apvts, pid::resoDecay);
    p.resoDamping = resolve (apvts, pid::resoDamping);
    p.resoScatter = resolve (apvts, pid::resoScatter);
    p.resoMotion  = resolve (apvts, pid::resoMotion);

    // ---- GLOBAL FX -------------------------------------------------------
    p.scurEnable = resolve (apvts, pid::scurEnable);
    p.scurAmount = resolve (apvts, pid::scurAmount);
    p.scurRate   = resolve (apvts, pid::scurRate);
    p.scurWidth  = resolve (apvts, pid::scurWidth);

    p.chorEnable   = resolve (apvts, pid::chorEnable);
    p.chorRate     = resolve (apvts, pid::chorRate);
    p.chorDepth    = resolve (apvts, pid::chorDepth);
    p.chorDelay    = resolve (apvts, pid::chorDelay);
    p.chorFeedback = resolve (apvts, pid::chorFeedback);
    p.chorWidth    = resolve (apvts, pid::chorWidth);
    p.chorMix      = resolve (apvts, pid::chorMix);

    p.dlyEnable    = resolve (apvts, pid::dlyEnable);
    p.dlyTime      = resolve (apvts, pid::dlyTime);
    p.dlySync      = resolve (apvts, pid::dlySync);
    p.dlySyncTime  = resolve (apvts, pid::dlySyncTime);
    p.dlyFeedback  = resolve (apvts, pid::dlyFeedback);
    p.dlyMotion    = resolve (apvts, pid::dlyMotion);
    p.dlySpread    = resolve (apvts, pid::dlySpread);
    p.dlyDamping   = resolve (apvts, pid::dlyDamping);
    p.dlyDiffusion = resolve (apvts, pid::dlyDiffusion);
    p.dlyMix       = resolve (apvts, pid::dlyMix);

    p.diffEnable  = resolve (apvts, pid::diffEnable);
    p.diffAmount  = resolve (apvts, pid::diffAmount);
    p.diffSize    = resolve (apvts, pid::diffSize);
    p.diffDamping = resolve (apvts, pid::diffDamping);
    p.diffMix     = resolve (apvts, pid::diffMix);

    p.verbEnable   = resolve (apvts, pid::verbEnable);
    p.verbSize     = resolve (apvts, pid::verbSize);
    p.verbDecay    = resolve (apvts, pid::verbDecay);
    p.verbPredelay = resolve (apvts, pid::verbPredelay);
    p.verbDamping  = resolve (apvts, pid::verbDamping);
    p.verbLowCut   = resolve (apvts, pid::verbLowCut);
    p.verbHighCut  = resolve (apvts, pid::verbHighCut);
    p.verbMod      = resolve (apvts, pid::verbMod);
    p.verbMix      = resolve (apvts, pid::verbMix);

    p.mastLow     = resolve (apvts, pid::mastLow);
    p.mastMid     = resolve (apvts, pid::mastMid);
    p.mastHigh    = resolve (apvts, pid::mastHigh);
    p.mastDrive   = resolve (apvts, pid::mastDrive);
    p.mastCeiling = resolve (apvts, pid::mastCeiling);
    p.mastOutput  = resolve (apvts, pid::mastOutput);

    // ---- GLOBAL / VOICE --------------------------------------------------
    p.glideTime  = resolve (apvts, pid::glideTime);
    p.glideMode  = resolve (apvts, pid::glideMode);
    p.voiceCount = resolve (apvts, pid::voiceCount);
    p.bendRange  = resolve (apvts, pid::bendRange);
    p.masterTune = resolve (apvts, pid::masterTune);

    // ---- MODULATION MATRIX ----------------------------------------------
    for (int slot = 0; slot < pid::kNumModSlots; ++slot)
    {
        p.mod[slot].source  = resolve (apvts, pid::modSourceID  (slot));
        p.mod[slot].dest    = resolve (apvts, pid::modDestID    (slot));
        p.mod[slot].amount  = resolve (apvts, pid::modAmountID  (slot));
        p.mod[slot].bipolar = resolve (apvts, pid::modBipolarID (slot));
    }

    // Prime the plain values so the first block never sees stale defaults.
    update();
}

//==============================================================================
void ParameterCache::cacheOsc (const OscPtrs& src, OscParams& dest) noexcept
{
    dest.wave     = readEnum (src.wave, OscWave::NumWaves);
    dest.shape    = readFloat (src.shape);
    dest.octave   = readInt (src.octave);
    dest.semitone = readInt (src.semitone);
    dest.fine     = readFloat (src.fine);
    dest.phase    = readFloat (src.phase);
    dest.unison   = juce::jlimit (1, dsp::kMaxUnison, readInt (src.unison));
    dest.detune   = readFloat (src.detune);
    dest.stereo   = readFloat (src.stereo);
    dest.pan      = readFloat (src.pan);
    dest.levelDb  = readFloat (src.level);
    dest.level    = levelToGain (dest.levelDb);

    dest.pitchOffsetSemitones = (float) (dest.octave * 12 + dest.semitone)
                                + dest.fine * 0.01f;
}

//==============================================================================
void ParameterCache::update() noexcept
{
    // ---- OSCILLATORS -----------------------------------------------------
    cacheOsc (p.oscA, oscA);
    cacheOsc (p.oscB, oscB);

    interaction.mode   = readEnum (p.interMode, InteractionMode::NumModes);
    interaction.amount = readFloat (p.interAmt);

    // ---- SUB -------------------------------------------------------------
    sub.wave    = readEnum (p.subWave, SubWave::NumWaves);
    sub.octave  = juce::jlimit (-2, -1, readInt (p.subOctave));
    sub.levelDb = readFloat (p.subLevel);
    sub.level   = levelToGain (sub.levelDb);

    // ---- NOISE -----------------------------------------------------------
    noise.type    = readEnum (p.noiseType, NoiseType::NumTypes);
    noise.levelDb = readFloat (p.noiseLevel);
    noise.level   = levelToGain (noise.levelDb);
    noise.tone    = readFloat (p.noiseTone);

    // ---- FILTER ----------------------------------------------------------
    filter.mode      = readEnum (p.filtMode, FilterMode::NumModes);
    filter.cutoffHz  = readFloat (p.filtCutoff);
    filter.resonance = readFloat (p.filtReso);
    filter.drive     = readFloat (p.filtDrive);
    filter.keyTrack  = readFloat (p.filtKeyTrack);
    filter.envAmount = readFloat (p.filtEnvAmt);
    filter.movement  = readFloat (p.filtMovement);
    filter.pressure  = readFloat (p.filtPressure);

    // ---- ENVELOPES (seconds) --------------------------------------------
    filterEnv.attack   = readFloat (p.fenvA);
    filterEnv.decay    = readFloat (p.fenvD);
    filterEnv.sustain  = readFloat (p.fenvS);
    filterEnv.release  = readFloat (p.fenvR);
    filterEnv.velocity = readFloat (p.fenvVel);

    ampEnv.attack   = readFloat (p.aenvA);
    ampEnv.decay    = readFloat (p.aenvD);
    ampEnv.sustain  = readFloat (p.aenvS);
    ampEnv.release  = readFloat (p.aenvR);
    ampEnv.velocity = readFloat (p.aenvVel);

    // ---- MACROS ----------------------------------------------------------
    macros.depth    = readFloat (p.macroDepth);
    macros.wet      = readFloat (p.macroWet);
    macros.ripple   = readFloat (p.macroRipple);
    macros.current  = readFloat (p.macroCurrent);
    macros.drops    = readFloat (p.macroDrops);
    macros.pressure = readFloat (p.macroPressure);
    macros.space    = readFloat (p.macroSpace);
    macros.glow     = readFloat (p.macroGlow);

    // ---- FLUID FIELD -----------------------------------------------------
    fluid.x       = readFloat (p.fluidX);
    fluid.y       = readFloat (p.fluidY);
    fluid.xAmount = readFloat (p.fluidXAmount);
    fluid.yAmount = readFloat (p.fluidYAmount);

    // ---- TIDE ------------------------------------------------------------
    tide.shape        = readEnum (p.tideShape, TideShape::NumShapes);
    tide.rateHz       = readFloat (p.tideRate);
    tide.sync         = readBool (p.tideSync);
    tide.syncDivision = readEnum (p.tideSyncRate, SyncDivision::NumDivisions);
    tide.phase        = readFloat (p.tidePhase);
    tide.depth        = readFloat (p.tideDepth);
    tide.stereo       = readFloat (p.tideStereo);

    // ---- CURRENT ---------------------------------------------------------
    current.rateHz     = readFloat (p.currRate);
    current.amount     = readFloat (p.currAmount);
    current.smoothness = readFloat (p.currSmooth);
    current.drift      = readFloat (p.currDrift);
    current.stereo     = readFloat (p.currStereo);

    // ---- DRIFT -----------------------------------------------------------
    drift.rateHz = readFloat (p.driftRate);
    drift.amount = readFloat (p.driftAmount);
    drift.stereo = readFloat (p.driftStereo);

    // ---- RIPPLE ----------------------------------------------------------
    rippleMod.rateHz  = readFloat (p.rippleRate);
    rippleMod.decay   = readFloat (p.rippleDecay);
    rippleMod.depth   = readFloat (p.rippleDepth);
    rippleMod.cycles  = readFloat (p.rippleCycles);
    rippleMod.spread  = readFloat (p.rippleSpread);
    rippleMod.invert  = readBool (p.ripplePolarity);
    rippleMod.trigger = readEnum (p.rippleTrigger, RippleTrigger::NumTriggers);

    // ---- DROPLETS --------------------------------------------------------
    droplets.amount  = readFloat (p.dropAmount);
    droplets.density = readFloat (p.dropDensity);
    droplets.size    = readFloat (p.dropSize);
    droplets.tone    = readFloat (p.dropTone);
    droplets.splash  = readFloat (p.dropSplash);
    droplets.gravity = readFloat (p.dropGravity);
    droplets.bounce  = readFloat (p.dropBounce);
    droplets.random  = readFloat (p.dropRandom);
    droplets.spread  = readFloat (p.dropSpread);
    droplets.mode    = readEnum (p.dropMode, DropletMode::NumModes);

    // ---- RESONATOR -------------------------------------------------------
    resonator.amount  = readFloat (p.resoAmount);
    resonator.size    = readFloat (p.resoSize);
    resonator.decay   = readFloat (p.resoDecay);
    resonator.damping = readFloat (p.resoDamping);
    resonator.scatter = readFloat (p.resoScatter);
    resonator.motion  = readFloat (p.resoMotion);

    // ---- STEREO CURRENT --------------------------------------------------
    stereoCurrent.enabled = readBool (p.scurEnable);
    stereoCurrent.amount  = readFloat (p.scurAmount);
    stereoCurrent.rateHz  = readFloat (p.scurRate);
    stereoCurrent.width   = readFloat (p.scurWidth);

    // ---- CHORUS ----------------------------------------------------------
    chorus.enabled  = readBool (p.chorEnable);
    chorus.rateHz   = readFloat (p.chorRate);
    chorus.depth    = readFloat (p.chorDepth);
    chorus.delayMs  = readFloat (p.chorDelay);
    chorus.feedback = readFloat (p.chorFeedback);
    chorus.width    = readFloat (p.chorWidth);
    chorus.mix      = readFloat (p.chorMix);

    // ---- DELAY -----------------------------------------------------------
    delay.enabled      = readBool (p.dlyEnable);
    delay.timeSeconds  = readFloat (p.dlyTime);
    delay.sync         = readBool (p.dlySync);
    delay.syncDivision = readEnum (p.dlySyncTime, SyncDivision::NumDivisions);
    delay.feedback     = readFloat (p.dlyFeedback);
    delay.motion       = readFloat (p.dlyMotion);
    delay.spread       = readFloat (p.dlySpread);
    delay.damping      = readFloat (p.dlyDamping);
    delay.diffusion    = readFloat (p.dlyDiffusion);
    delay.mix          = readFloat (p.dlyMix);

    // ---- DIFFUSION -------------------------------------------------------
    diffusion.enabled = readBool (p.diffEnable);
    diffusion.amount  = readFloat (p.diffAmount);
    diffusion.size    = readFloat (p.diffSize);
    diffusion.damping = readFloat (p.diffDamping);
    diffusion.mix     = readFloat (p.diffMix);

    // ---- REVERB ----------------------------------------------------------
    reverb.enabled    = readBool (p.verbEnable);
    reverb.size       = readFloat (p.verbSize);
    reverb.decay      = readFloat (p.verbDecay);
    reverb.predelayMs = readFloat (p.verbPredelay);
    reverb.damping    = readFloat (p.verbDamping);
    reverb.lowCutHz   = readFloat (p.verbLowCut);
    reverb.highCutHz  = readFloat (p.verbHighCut);
    reverb.modulation = readFloat (p.verbMod);
    reverb.mix        = readFloat (p.verbMix);

    // ---- MASTER ----------------------------------------------------------
    master.lowGainDb   = readFloat (p.mastLow);
    master.midGainDb   = readFloat (p.mastMid);
    master.highGainDb  = readFloat (p.mastHigh);
    master.drive       = readFloat (p.mastDrive);
    master.ceilingDb   = readFloat (p.mastCeiling);
    master.outputDb    = readFloat (p.mastOutput);
    master.ceilingGain = math::decibelsToGain (master.ceilingDb);
    master.outputGain  = math::decibelsToGain (master.outputDb);

    // ---- GLOBAL / VOICE --------------------------------------------------
    global.glideSeconds    = readFloat (p.glideTime);
    global.glideMode       = readEnum (p.glideMode, GlideMode::NumModes);
    global.voiceCount      = juce::jlimit (1, dsp::kMaxVoices, readInt (p.voiceCount));
    global.bendRange       = juce::jlimit (0, 24, readInt (p.bendRange));
    global.masterTuneCents = readFloat (p.masterTune);
    global.masterTuneRatio = math::semitonesToRatio (global.masterTuneCents * 0.01f);

    // ---- MODULATION MATRIX ----------------------------------------------
    for (int slot = 0; slot < pid::kNumModSlots; ++slot)
    {
        auto& out = modSlots[slot];
        const auto& in = p.mod[slot];

        out.source  = (int) readEnum (in.source, ModSource::NumSources);
        out.dest    = (int) readEnum (in.dest,   ModDest::NumDests);
        out.amount  = readFloat (in.amount);
        out.bipolar = readBool (in.bipolar);
    }
}

//==============================================================================
float ParameterCache::syncedRateHz (SyncDivision div, double bpm) const noexcept
{
    return (float) (safeBpm (bpm) / (60.0 * divisionBeats (div)));
}

float ParameterCache::syncedTimeSeconds (SyncDivision div, double bpm) const noexcept
{
    return (float) (divisionBeats (div) * 60.0 / safeBpm (bpm));
}

float ParameterCache::tideRateHz (double bpm) const noexcept
{
    return tide.sync ? syncedRateHz (tide.syncDivision, bpm) : tide.rateHz;
}

float ParameterCache::delayTimeSeconds (double bpm) const noexcept
{
    const float t = delay.sync ? syncedTimeSeconds (delay.syncDivision, bpm)
                               : delay.timeSeconds;

    return juce::jlimit (0.001f, dsp::kMaxDelaySeconds, t);
}

} // namespace ripples
