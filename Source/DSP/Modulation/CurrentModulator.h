#pragma once

#include "Utilities/RandomGenerator.h"

#include <cstdint>

namespace ripples
{

/**
    CURRENT — smooth correlated randomness. Never sample-and-hold.

    The model is a damped particle floating in moving water: a random force is
    integrated into a velocity, the velocity is damped, and the velocity is
    integrated into a position. Because the output is the *second* integral of
    the noise it is continuous and has a continuous derivative, so it drifts and
    turns instead of stepping. There is no held value anywhere in here.

    Concretely each channel runs a stochastic damped oscillator

        v += (-2*zeta*w*v - w*w*x) * dt + g * n * sqrt(dt)
        x += v * dt

    with the force gain g = sd * 2 * sqrt(zeta * w^3), which is exactly the
    value that keeps the stationary standard deviation of x at `sd` whatever the
    rate or the damping. That is why turning `rate` or `smoothness` changes the
    *character* of the motion and never its depth.

    Integration is sub-stepped so the behaviour is identical for any
    `numSamples`, and the output is bounded by a soft knee — a hard clip would
    flatten the crests and read as mechanical.

      rate        how fast it wanders (Hz-ish)
      smoothness  damping: 0 restless and granular, 1 gliding and liquid
      drift       amount of an additional very slow bias wander
      stereo      0 = the two channels are the same current, 1 = independent
*/
class CurrentModulator
{
public:
    struct Params
    {
        float rate       = 0.5f;   // Hz-ish, how fast it wanders
        float smoothness = 0.5f;   // 0..1, damping
        float drift      = 0.0f;   // 0..1, slow bias wander
        float stereo     = 0.5f;   // 0..1 decorrelation between L/R values
    };

    void prepare (double sampleRate, uint32_t seed);
    void reset() noexcept;
    void setParams (const Params& p) noexcept;

    /** Steps the current forward by numSamples and returns the new left value. */
    float advance (int numSamples) noexcept;

    float getValue()  const noexcept { return value; }
    float getValueR() const noexcept { return valueR; }

    static constexpr float kMinRateHz = 0.005f;
    static constexpr float kMaxRateHz = 20.0f;

private:
    struct Particle
    {
        float position = 0.0f;
        float velocity = 0.0f;
        float smoothed = 0.0f;

        void clear() noexcept { position = velocity = smoothed = 0.0f; }
    };

    void recompute() noexcept;
    static void integrate (Particle& p, float noise, float dt, float sqrtDt,
                           float twoZetaOmega, float omegaSq, float forceGain) noexcept;
    static void tidy (Particle& p) noexcept;

    double sampleRate    = 44100.0;
    double invSampleRate = 1.0 / 44100.0;

    Params params {};
    RandomGenerator rng;

    Particle mainL, mainR;
    Particle biasL, biasR;

    // Main-layer coefficients.
    float omega        = 1.0f;
    float twoZetaOmega = 1.0f;
    float omegaSq      = 1.0f;
    float forceGain    = 0.0f;
    float smoothOmega  = 1.0f;

    // Slow bias-layer coefficients.
    float biasTwoZetaOmega = 1.0f;
    float biasOmegaSq      = 1.0f;
    float biasForceGain    = 0.0f;

    // Stereo decorrelation mix (unit-variance preserving).
    float corrGain      = 1.0f, indepGain = 0.0f;
    float biasCorrGain  = 1.0f, biasIndepGain = 0.0f;

    float value = 0.0f, valueR = 0.0f;
};

} // namespace ripples
