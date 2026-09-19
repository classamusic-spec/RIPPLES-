#pragma once

/*
    RIPPLES — StereoCurrent.

    Slow, fluid motion of the stereo image: the sound appears to drift sideways
    and breathe in an unseen current. This is deliberately NOT a tremolo (the
    mono sum never changes level) and NOT a phaser (the mono sum is never
    comb-filtered).

    The whole effect is built in the mid/side domain and touches the SIDE
    channel only, so L+R is mathematically identical to the input. That makes it
    unconditionally mono-compatible: nothing it does can cancel in mono.

        M            -> unchanged
        S_out        =  S * widthGain(t)            (slow width breathing)
                     +  M * panDrift(t)             (exact mono-safe pan)
                     +  allpass(M) * mistDepth(t)   (wandering decorrelation)

    Adding k*M to the side channel pans the image (L = M(1+k), R = M(1-k)) while
    leaving L+R = 2M untouched, so the "drift" is free of any mono penalty.
*/

#include <juce_audio_basics/juce_audio_basics.h>

#include "Utilities/DSPConstants.h"
#include "Utilities/MathUtils.h"

namespace ripples
{

class StereoCurrent
{
public:
    struct Params
    {
        bool  enabled = true;
        float amount  = 0.3f;   ///< 0..1 depth of the motion
        float rate    = 0.1f;   ///< Hz — the base wander rate (0.1 Hz == a 10 s cycle)
        float width   = 0.5f;   ///< 0..1 static stereo width; 0.5 == unity
    };

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;
    void setParams (const Params& p) noexcept;
    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    static constexpr int kNumAllPass      = 4;
    static constexpr int kControlInterval = dsp::kModBlockSize;

    /** First-order allpass, H(z) = (a + z^-1) / (1 + a z^-1), TDF2. */
    struct AllPass1
    {
        float z = 0.0f;

        inline float process (float x, float a) noexcept
        {
            const float y = a * x + z;
            z = math::sanitise (x - a * y);
            return y;
        }

        void clear() noexcept { z = 0.0f; }
    };

    void updateControlRate() noexcept;

    double sampleRate_ = 44100.0;
    Params params_ {};

    // Three incommensurate phasors sum into a non-repeating wander.
    float phase_[3] {};
    float phaseInc_[3] {};

    AllPass1 ap_[kNumAllPass];
    float apCoeff_[kNumAllPass]  { 0.5f, 0.5f, 0.5f, 0.5f };
    float apTarget_[kNumAllPass] { 0.5f, 0.5f, 0.5f, 0.5f };
    float apBaseHz_[kNumAllPass] { 110.0f, 340.0f, 1150.0f, 3300.0f };

    float sideGain_ = 1.0f, sideGainTarget_ = 1.0f;
    float panDrift_ = 0.0f, panDriftTarget_ = 0.0f;
    float mistDepth_ = 0.0f, mistDepthTarget_ = 0.0f;

    float smoothCoeff_ = 0.01f;
    float enableGain_  = 1.0f;
    float enableCoeff_ = 0.01f;
    int   controlCounter_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StereoCurrent)
};

} // namespace ripples
