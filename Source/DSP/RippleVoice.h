#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "Parameters/ParameterEnums.h"
#include "Parameters/ParameterIDs.h"
#include "Utilities/DSPConstants.h"
#include "Utilities/RandomGenerator.h"

#include "DSP/Oscillators/BandLimitedOscillator.h"
#include "DSP/Oscillators/SubOscillator.h"
#include "DSP/Oscillators/WaterNoise.h"
#include "DSP/Filters/DepthFilter.h"
#include "DSP/Exciters/DropletEngine.h"
#include "DSP/Resonators/WaterResonator.h"
#include "DSP/Modulation/Envelope.h"
#include "DSP/Modulation/TideLFO.h"
#include "DSP/Modulation/CurrentModulator.h"
#include "DSP/Modulation/DriftModulator.h"
#include "DSP/Modulation/RippleModulator.h"
#include "DSP/Modulation/ModulationMatrix.h"

namespace ripples
{

//==============================================================================
/**
    Everything a voice needs for one block, in natural units.

    The synth fills this from the ParameterCache once per block and hands the
    same struct to every voice. Keeping it a plain aggregate means a voice never
    touches the APVTS, and the DSP modules stay independent of the parameter
    layer.
*/
struct VoiceParams
{
    // --- Oscillator A (TIDE) -------------------------------------------------
    BandLimitedOscillator::Params oscA;
    int   oscAOctave = 0, oscASemitone = 0;
    float oscAFine = 0.0f;          // cents
    float oscALevel = 1.0f;         // linear gain
    float oscAPan = 0.0f;           // -1..1

    // --- Oscillator B (CURRENT) ---------------------------------------------
    BandLimitedOscillator::Params oscB;
    int   oscBOctave = 0, oscBSemitone = 0;
    float oscBFine = 0.0f;
    float oscBLevel = 0.0f;
    float oscBPan = 0.0f;
    InteractionMode interMode = InteractionMode::Normal;
    float interAmount = 0.0f;       // 0..1

    // --- Sub / noise ---------------------------------------------------------
    SubOscillator::Params sub;
    float subLevel = 0.0f;
    WaterNoise::Params noise;
    float noiseLevel = 0.0f;

    // --- Filter --------------------------------------------------------------
    DepthFilter::Params filter;
    float filterKeyTrack = 0.0f;    // 0..1
    float filterEnvAmount = 0.0f;   // -1..1, bipolar

    // --- Envelopes -----------------------------------------------------------
    Envelope::Params ampEnv, modEnv;
    float ampVelocity = 0.5f;       // 0..1 velocity sensitivity
    float filterEnvVelocity = 0.5f;

    // --- Per-voice modulators ------------------------------------------------
    TideLFO::Params tide;           float tideDepth = 0.0f;
    CurrentModulator::Params current; float currentAmount = 0.0f;
    DriftModulator::Params drift;   float driftAmount = 0.0f;
    RippleModulator::Params ripple; float rippleDepth = 0.0f;
    RippleTrigger rippleTrigger = RippleTrigger::NoteOn;

    // --- Aquatic engines -----------------------------------------------------
    DropletEngine::Params droplets;
    WaterResonator::Params resonator;

    // --- Pitch / performance -------------------------------------------------
    float glideTime = 0.0f;         // seconds
    GlideMode glideMode = GlideMode::Off;
    float bendRangeSemis = 2.0f;
    float masterTuneCents = 0.0f;

    // --- Fluid field ---------------------------------------------------------
    float fluidX = 0.5f, fluidY = 0.5f;   // 0..1

    // --- Modulation matrix ---------------------------------------------------
    ModulationMatrix::Slot modSlots[pid::kNumModSlots] {};

    // --- Live MIDI controllers ----------------------------------------------
    float modWheel = 0.0f;          // 0..1
    float aftertouch = 0.0f;        // 0..1
    float pitchBend = 0.0f;         // -1..1
};

//==============================================================================
/**
    One polyphonic voice: the full per-voice signal path from oscillators through
    the Depth filter and Water resonator to the amp envelope and pan.

    Modulation runs at control rate (dsp::kModBlockSize samples) and audio at
    sample rate. Nothing here allocates after prepare().
*/
class RippleVoice
{
public:
    RippleVoice() = default;

    void prepare (double sampleRate, int maxBlockSize, uint32_t seed);
    void reset() noexcept;

    /** Called once per block, before rendering. */
    void setParams (const VoiceParams& p) noexcept { params = p; paramsDirty = true; }

    void noteOn (int midiNote, float velocity, bool legato) noexcept;
    void noteOff() noexcept;
    /** Immediate silence — used when stealing a voice that must free up now. */
    void kill() noexcept;
    /** Fast (few-ms) fade then free — the polite form of stealing. */
    void steal() noexcept;

    bool isActive() const noexcept     { return active; }
    bool isReleasing() const noexcept  { return releasing; }
    int  getCurrentNote() const noexcept { return currentNote; }
    uint64_t getStartOrder() const noexcept { return startOrder; }
    float getCurrentLevel() const noexcept { return lastAmpEnv; }

    void setStartOrder (uint64_t order) noexcept { startOrder = order; }

    /** Adds this voice's output into the given stereo buffer. */
    void renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples) noexcept;

    // --- Visualisation taps (read on the message thread; plain floats) -------
    float getAmpEnvValue() const noexcept  { return lastAmpEnv; }
    float getModEnvValue() const noexcept  { return lastModEnv; }
    float getTideValue() const noexcept    { return lastTide; }
    float getCurrentValue() const noexcept { return lastCurrent; }
    float getDriftValue() const noexcept   { return lastDrift; }
    float getRippleValue() const noexcept  { return lastRipple; }
    float getFilterCutoffHz() const noexcept { return lastCutoffHz; }

    /** Drains a droplet event for the UI, if the engine produced one. */
    bool consumeDropletEvent (float& intensity, float& pan) noexcept
    {
        return droplets.consumeDropletEvent (intensity, pan);
    }

private:
    void updateControlBlock() noexcept;
    void applyModulation (const float* destOffsets) noexcept;
    float computeOscFrequency (float baseHz, int octave, int semitone, float fineCents,
                               float pitchOffsetSemis) const noexcept;

    //--------------------------------------------------------------------------
    double sampleRate = 44100.0;

    VoiceParams params;
    bool paramsDirty = true;

    // Sound sources
    BandLimitedOscillator oscA, oscB;
    SubOscillator subOsc;
    WaterNoise noise;
    DropletEngine droplets;

    // Shaping
    DepthFilter filter;
    WaterResonator resonator;
    Envelope ampEnv, modEnv;

    // Per-voice modulation
    TideLFO tide;
    CurrentModulator current;
    DriftModulator drift;
    RippleModulator rippleMod;
    ModulationMatrix matrix;
    ModSourceValues sourceValues;
    float destOffsets[(size_t) ModDest::NumDests] {};

    RandomGenerator rng;

    // Note state
    bool  active = false, releasing = false;
    int   currentNote = -1;
    float velocity = 0.0f;
    float noteHz = 440.0f;
    float targetHz = 440.0f, currentHz = 440.0f, glideCoeff = 1.0f;
    float randomPerNote = 0.0f;
    uint64_t startOrder = 0;

    // Fast-fade state used by steal()
    bool  fadingOut = false;
    float fadeGain = 1.0f, fadeCoeff = 0.0f;

    // Control-rate bookkeeping
    int   modCounter = 0;

    // Smoothed / resolved per-block values (post modulation)
    float modCutoffHz = 1000.0f;
    float modResonance = 0.0f;
    float modOscALevel = 1.0f, modOscBLevel = 0.0f, modSubLevel = 0.0f, modNoiseLevel = 0.0f;
    float modOscAPanL = 0.7071f, modOscAPanR = 0.7071f;
    float modOscBPanL = 0.7071f, modOscBPanR = 0.7071f;
    float modAmplitude = 1.0f;
    float modInterAmount = 0.0f;
    float modPitchOffsetSemis = 0.0f;

    // Visualisation taps
    float lastAmpEnv = 0.0f, lastModEnv = 0.0f;
    float lastTide = 0.0f, lastCurrent = 0.0f, lastDrift = 0.0f, lastRipple = 0.0f;
    float lastCutoffHz = 1000.0f;

    JUCE_LEAK_DETECTOR (RippleVoice)
};

} // namespace ripples
