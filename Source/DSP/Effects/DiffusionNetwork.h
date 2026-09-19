#pragma once

/*
    RIPPLES — DiffusionNetwork.

    Four modulated Schroeder allpasses per channel, mutually prime and
    decorrelated between L and R. This is the "mist": it scrambles phase and
    smears transients into a haze without adding a tail, so a dry sound becomes
    atmospheric long before the reverb is reached.

    Level neutrality comes from three things: the allpass sections are unity
    gain by construction, the dry/wet blend is an equal-power crossfade, and the
    in-loop damping is the only element that removes energy at all.

    A very small, very slow modulation is always present. It costs nothing and
    it stops the fixed comb pattern of a static allpass chain from ringing on
    sustained material.
*/

#include <juce_audio_basics/juce_audio_basics.h>

#include "Utilities/DSPConstants.h"
#include "Utilities/MathUtils.h"

#include <vector>

namespace ripples
{

class DiffusionNetwork
{
public:
    DiffusionNetwork() = default;

    struct Params
    {
        bool  enabled = true;
        float amount  = 0.4f;   ///< 0..1 allpass coefficient (how thick the blur is)
        float size    = 0.5f;   ///< 0..1 network scale, 0.25x .. 2.0x
        float damping = 0.4f;   ///< 0..1 darkens the diffused signal
        float mix     = 0.3f;   ///< 0..1 (equal power)
    };

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;
    void setParams (const Params& p) noexcept;
    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    static constexpr int   kNumStages       = 4;
    static constexpr int   kControlInterval = dsp::kModBlockSize;
    static constexpr float kMaxStageMs      = 46.0f;    // longest base length
    static constexpr float kMaxSizeScale    = 2.0f;
    static constexpr float kStageSlew       = 0.5f;   // tames a SIZE sweep

    struct DelayLine
    {
        std::vector<float> buf;
        int size = 0, mask = 0, writePos = 0;

        void allocate (int minSamples)
        {
            int n = 8;
            while (n < minSamples) n <<= 1;
            buf.assign ((size_t) n, 0.0f);
            size = n;
            mask = n - 1;
            writePos = 0;
        }

        void clear() noexcept
        {
            std::fill (buf.begin(), buf.end(), 0.0f);
            writePos = 0;
        }

        inline void write (float x) noexcept
        {
            writePos = (writePos + 1) & mask;
            buf[(size_t) writePos] = x;
        }

        inline float readCubic (float delaySamples) const noexcept
        {
            const float d = math::clamp (delaySamples, 2.0f, (float) (size - 4));
            const float readPos = (float) writePos - d;
            const int   i0 = (int) std::floor (readPos);
            const float f  = readPos - (float) i0;

            const float ym1 = buf[(size_t) ((i0 - 1) & mask)];
            const float y0  = buf[(size_t) ( i0      & mask)];
            const float y1  = buf[(size_t) ((i0 + 1) & mask)];
            const float y2  = buf[(size_t) ((i0 + 2) & mask)];

            const float c1 = 0.5f * (y1 - ym1);
            const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
            const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);

            return ((c3 * f + c2) * f + c1) * f + y0;
        }
    };

    void updateControlRate() noexcept;

    double sampleRate_ = 44100.0;
    Params params_ {};

    DelayLine line_[2][kNumStages];
    float dampState_[2][kNumStages] {};

    float baseSamples_[2][kNumStages] {};   // stage length at scale 1.0
    float delay_[2][kNumStages] {};         // smoothed read length, samples
    float delayTarget_[2][kNumStages] {};

    float modPhase_[kNumStages] {};
    float modInc_[kNumStages] {};
    float modDepthSamples_ = 0.0f;

    float sizeScale_ = 1.0f, sizeTarget_ = 1.0f;
    float apGain_ = 0.0f, apGainTarget_ = 0.0f;
    float dampCoeff_ = 1.0f;

    float dryGain_ = 1.0f, wetGain_ = 0.0f;
    float dryTarget_ = 1.0f, wetTarget_ = 0.0f;

    float smoothCoeff_ = 0.01f;
    float delaySmooth_ = 0.01f;
    float enableGain_  = 1.0f;
    float enableCoeff_ = 0.01f;
    int   controlCounter_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DiffusionNetwork)
};

} // namespace ripples
