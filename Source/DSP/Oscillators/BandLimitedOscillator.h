#pragma once

/*
    RIPPLES — TIDE / CURRENT oscillator.

    Nine waves, up to seven detuned unison voices with equal-power stereo
    spread, per-sample phase modulation and linear FM, and hard-sync
    master/slave behaviour.

    Anti-aliasing:
      Saw / Square / Pulse / Shark   polyBLEP on every jump
      Triangle                       polyBLAMP on both corners
      Sine / Hollow / Glass / Water  summed band-limited partials, the partial
                                     count falling as the frequency rises

    Realtime safety: every buffer is a fixed-size member array, nothing is
    allocated, locked or logged once prepare() has run.
*/

#include "Parameters/ParameterEnums.h"
#include "Utilities/RandomGenerator.h"
#include "Utilities/DSPConstants.h"

namespace ripples
{

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

    /** Largest number of summed partials used by the additive waves. */
    static constexpr int kMaxPartials = 16;

    /** Widest detune spread, in cents, at detune = 1. */
    static constexpr float kMaxDetuneCents = 50.0f;

private:
    //==========================================================================
    struct UnisonVoice
    {
        float phase       = 0.0f;   // 0..1 accumulator
        float startPhase  = 0.0f;   // phase restored by reset() / hardSync()
        float ratio       = 1.0f;   // detune ratio
        float invRatio    = 1.0f;
        float inc         = 0.0f;   // cycles per sample
        float gainL       = 1.0f;
        float gainR       = 1.0f;
        int   numPartials = 1;
    };

    void rebuildUnison (bool randomisePhases, RandomGenerator* rng) noexcept;
    void deriveShapeConstants() noexcept;
    void updatePartialTargets() noexcept;
    void snapPartialGains() noexcept;

    float renderWave (float phase, float dt, const UnisonVoice& uv) const noexcept;
    float renderPartials (float phase, int n) const noexcept;
    float renderTriangle (float p, float dt, float width, float invW, float invComp) const noexcept;
    float renderSaw (float p, float dt) const noexcept;
    float renderPulse (float p, float dt) const noexcept;
    float renderShark (float p, float dt) const noexcept;

    //==========================================================================
    double sampleRate_    = 44100.0;
    float  invSampleRate_ = 1.0f / 44100.0f;
    float  maxFreq_       = 21168.0f;   // 0.48 * sr
    float  maxPartialHz_  = 19845.0f;   // 0.45 * sr
    float  shapeCoeff_    = 0.005f;
    float  gainCoeff_     = 0.01f;
    float  controlSeconds_ = 32.0f / 44100.0f;

    Params  params_ {};
    OscWave wave_          = OscWave::Saw;
    bool    usesPartials_  = false;
    int     partialLimit_  = 1;

    UnisonVoice voices_[dsp::kMaxUnison] {};
    int   numVoices_   = 1;
    int   centreIndex_ = 0;
    bool  phasesReady_ = false;

    float baseFreq_   = 440.0f;
    float centreFreq_ = 440.0f;

    float shapeTarget_   = 0.5f;
    float shapeSmoothed_ = 0.5f;

    // Shape-derived constants, refreshed once per sample from the smoothed shape.
    float pulseWidth_ = 0.5f, pulseDc_ = 0.0f, pulseScale_ = 1.0f;
    float triWidth_   = 0.5f, triInvW_ = 2.0f, triInvComp_ = 2.0f;
    float sawTriMix_  = 1.0f, sawOctave_ = 0.0f;
    float sharkThr_   = 0.5f, sharkEdge_ = 0.75f, sharkDc_ = 0.0f, sharkScale_ = 1.0f;

    // Additive engine. The complex gain of partial k is (gainRe_, gainIm_);
    // the render loop evaluates y += re * sin(k.theta) + im * cos(k.theta).
    float gainRe_  [kMaxPartials] {};
    float gainIm_  [kMaxPartials] {};
    float targetRe_[kMaxPartials] {};
    float targetIm_[kMaxPartials] {};
    float modPhase_[kMaxPartials] {};   // slow amplitude motion (Water)
    float driftPhase_[kMaxPartials] {}; // partial detune / phase motion (Glass, Water)
    int   activePartials_ = 1;
    int   controlCounter_ = 0;

    RandomGenerator internalRng_ { 0x5EED0C51u };
};

} // namespace ripples
