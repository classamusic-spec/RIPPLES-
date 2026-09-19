#pragma once

#include "Utilities/RandomGenerator.h"

#include <cstdint>

namespace ripples
{

/**
    DRIFT — the same damped-particle motion as CURRENT, geared right down.

    Periods run from about twenty seconds out to eight minutes. It exists so a
    pad or a drone is never static: the filter, the detune and the stereo image
    keep moving long after the ear has stopped noticing why.

    Two layers are stacked an octave-and-a-bit apart in rate. The lower one
    carries the long journey; the quieter upper one guarantees there is always
    *some* motion, so the output never sits on a flat stretch during a slow
    passage of the lower layer. Both are second-order, so the value and its
    slope are continuous — a drift that jumped would be worse than no drift.

      rate    Hz-ish: 0.002 (an eight minute journey) .. 0.5 (a two second sway)
      stereo  0 = both channels drift together, 1 = independent
*/
class DriftModulator
{
public:
    struct Params
    {
        float rate   = 0.05f;   // Hz-ish
        float stereo = 0.5f;    // 0..1 decorrelation between L/R values
    };

    void prepare (double sampleRate, uint32_t seed);
    void reset() noexcept;
    void setParams (const Params& p) noexcept;

    /** Steps the drift forward by numSamples and returns the new left value. */
    float advance (int numSamples) noexcept;

    float getValue()  const noexcept { return value; }
    float getValueR() const noexcept { return valueR; }

    static constexpr float kMinRateHz = 0.002f;    // ~8 minute period
    static constexpr float kMaxRateHz = 0.5f;

private:
    struct Particle
    {
        float position = 0.0f;
        float velocity = 0.0f;

        void clear() noexcept { position = velocity = 0.0f; }
    };

    struct Layer
    {
        Particle left, right;
        float twoZetaOmega = 1.0f;
        float omegaSq      = 1.0f;
        float forceGain    = 0.0f;
        float omega        = 1.0f;

        void clear() noexcept { left.clear(); right.clear(); }
    };

    void recompute() noexcept;
    void stepLayer (Layer& layer, float dt) noexcept;
    static void tidy (Particle& p) noexcept;

    double sampleRate    = 44100.0;
    double invSampleRate = 1.0 / 44100.0;

    Params params {};
    RandomGenerator rng;

    Layer slow, fine;

    float corrGain = 1.0f, indepGain = 0.0f;

    float value = 0.0f, valueR = 0.0f;
};

} // namespace ripples
