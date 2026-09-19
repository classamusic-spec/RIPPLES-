#pragma once

/*
    RIPPLES — MasterShaper.

    The last thing the signal touches: a gentle three band EQ, an optional
    drive, and a safety limiter.

      * The EQ is built from TPT one-pole shelves and a Cytomic-style SVF bell,
        so the band gains are plain mixing coefficients. They can be smoothed
        per sample and never need a coefficient recalculation, which means no
        zipper noise and no chance of a biquad blowing up mid-sweep.
      * The limiter is a SAFETY device, not an effect. Below the ceiling its
        gain is exactly 1.0 and the signal is untouched. Above it, a branching
        smoother (1 ms attack / 250 ms release) followed by a second 3 ms
        smoother produces an S-curve gain envelope: no stair-stepping, and on a
        sustained pad the reduction sits still instead of pumping.
      * Because there is no look-ahead, a very fast transient can outrun the
        attack. A soft knee that is perfectly linear below 85% of the ceiling
        and asymptotic to it above catches those, so the output is
        mathematically guaranteed to stay under the ceiling at all times.
      * Output gain is applied BEFORE the limiter, so the ceiling is always the
        real last word.
*/

#include <juce_audio_basics/juce_audio_basics.h>

#include "Utilities/DSPConstants.h"
#include "Utilities/MathUtils.h"

#include <atomic>

namespace ripples
{

class MasterShaper
{
public:
    MasterShaper() = default;

    struct Params
    {
        float lowGainDb  = 0.0f;    ///< low shelf,  +/-18 dB
        float midGainDb  = 0.0f;    ///< mid bell,   +/-18 dB
        float highGainDb = 0.0f;    ///< high shelf, +/-18 dB
        float drive      = 0.0f;    ///< 0..1
        float ceilingDb  = -0.3f;   ///< limiter ceiling, -24..0 dB
        float outputDb   = 0.0f;    ///< -60..+12 dB, applied pre-limiter
    };

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;
    void setParams (const Params& p) noexcept;
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    float getPeakL() const noexcept { return peakL_.load (std::memory_order_relaxed); }
    float getPeakR() const noexcept { return peakR_.load (std::memory_order_relaxed); }

private:
    static constexpr float kLowShelfHz  = 180.0f;
    static constexpr float kMidHz       = 900.0f;
    static constexpr float kMidQ        = 0.8f;
    static constexpr float kHighShelfHz = 4200.0f;
    static constexpr float kKneeStart   = 0.85f;   // fraction of the ceiling

    /** TPT one-pole: supplies simultaneous LP and HP for the shelves. */
    struct OnePoleTPT
    {
        float s = 0.0f;

        inline void process (float x, float G1, float& lp, float& hp) noexcept
        {
            const float v = (x - s) * G1;
            lp = v + s;
            s  = math::sanitise (lp + v);
            hp = x - lp;
        }

        void clear() noexcept { s = 0.0f; }
    };

    /** Cytomic SVF, used for the bell's bandpass output. */
    struct SvfTPT
    {
        float ic1 = 0.0f, ic2 = 0.0f;

        inline float bandPass (float x, float a1, float a2, float a3) noexcept
        {
            const float v3 = x - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;

            ic1 = math::sanitise (2.0f * v1 - ic1);
            ic2 = math::sanitise (2.0f * v2 - ic2);

            return v1;
        }

        void clear() noexcept { ic1 = ic2 = 0.0f; }
    };

    double sampleRate_ = 44100.0;
    Params params_ {};

    // --- EQ ---------------------------------------------------------------
    OnePoleTPT lowShelf_[2], highShelf_[2];
    SvfTPT     midBand_[2];

    float lowG1_ = 0.1f, highG1_ = 0.5f;
    float midA1_ = 0.5f, midA2_ = 0.1f, midA3_ = 0.05f, midK_ = 1.25f;

    float lowGain_ = 1.0f,  lowTarget_ = 1.0f;
    float midGain_ = 1.0f,  midTarget_ = 1.0f;
    float highGain_ = 1.0f, highTarget_ = 1.0f;

    // --- drive ------------------------------------------------------------
    float driveMix_ = 0.0f,    driveMixTarget_ = 0.0f;
    float driveGain_ = 1.0f,   driveGainTarget_ = 1.0f;
    float driveMakeup_ = 1.0f, driveMakeupTarget_ = 1.0f;

    // --- output / limiter --------------------------------------------------
    float outGain_ = 1.0f, outTarget_ = 1.0f;
    float ceiling_ = 0.9661f, ceilingTarget_ = 0.9661f;

    float limEnv_ = 0.0f;
    float limGain_ = 1.0f, limGainSmooth_ = 1.0f;
    float envRelCoeff_ = 0.0f;
    float attCoeff_ = 1.0f, relCoeff_ = 0.0f, glideCoeff_ = 1.0f;

    float smoothCoeff_ = 0.01f;

    std::atomic<float> peakL_ { 0.0f };
    std::atomic<float> peakR_ { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterShaper)
};

} // namespace ripples
