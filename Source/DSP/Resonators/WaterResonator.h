#pragma once

#include "Utilities/DSPConstants.h"
#include "Utilities/MathUtils.h"
#include "Utilities/RandomGenerator.h"

#include <array>
#include <cstdint>
#include <vector>

namespace ripples
{

/**
    RESONATOR — a parallel bank of dsp::kNumResonatorBanks constant-peak-gain
    two-pole resonators per channel, placed after the filter.

    Each bank is

        H(z) = a0 (1 - z^-2) / (1 - 2 R cos(theta) z^-1 + R^2 z^-2),  a0 = (1-R^2)/2

    which has a peak gain of exactly 1 at its resonance no matter how long the
    ring time is.  That normalisation — not a limiter — is what makes the bank
    safe at maximum DECAY with full-scale input; the in-loop saturator and the
    denormal/NaN guard are belt and braces on top of it.

    SIZE sets the fundamental (multiplying the note frequency passed to
    setBaseFrequency), DECAY the ring time, DAMPING the high-frequency loss over
    time (glassy versus submerged), SCATTER bends the bank away from a harmonic
    series toward bell / plate inharmonicity, and MOTION drifts the tuning so the
    bank never sounds frozen.  Left and right banks are detuned and modulated
    independently so the result opens up instead of sitting in the middle.

    Target timbres: bubbles, glass, bowls, submerged bells, metallic
    reflections, cavern resonance.

    processSample() replaces l/r with the dry/wet mix set by AMOUNT.
*/
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

    //== Extra helpers (not part of the contract; safe to ignore) ==============
    int   getNumBanks() const noexcept { return dsp::kNumResonatorBanks; }
    float getBankFrequency (int channel, int bank) const noexcept;

private:
    static constexpr int kNumBanks = dsp::kNumResonatorBanks;
    static constexpr int kNumChannels = 2;

    struct Bank
    {
        float y1 = 0.0f, y2 = 0.0f;          // recursion state
        float a0 = 0.0f, b1 = 0.0f, b2 = 0.0f;
        float gain = 0.0f;
        float detune = 1.0f;                 // fixed per-channel decorrelation
        float scatterJitter = 0.0f;          // organic inharmonicity, scaled by SCATTER
        float gainTrim = 1.0f;
        float lfoPhase = 0.0f, lfoInc = 0.0f;
        float frequency = 220.0f;            // reporting only
    };

    void updateCoefficients() noexcept;

    double sampleRate = 44100.0;
    float  maxPoleRadius = 0.99995f;   // strictly < 1 at every sample rate

    Params params {};
    // Control-rate smoothed copies.
    float sizeS = 0.5f, decayS = 0.5f, dampingS = 0.5f, scatterS = 0.3f, motionS = 0.2f;
    float amountS = 0.0f;
    float baseTargetHz = 220.0f, baseHz = 220.0f;
    float ctlCoeff = 0.2f;

    float dryGain = 1.0f, wetGain = 0.0f;           // per-sample smoothed
    float dryTarget = 1.0f, wetTarget = 0.0f;
    float mixCoeff = 0.01f;

    std::vector<Bank> banks;                        // allocated in prepare()
    std::array<float, kNumChannels> xz1 {}, xz2 {};
    std::array<float, kNumChannels> dampLpZ {};
    float dampLpCoeff = 1.0f;

    int ctlCountdown = 0;
};

} // namespace ripples
