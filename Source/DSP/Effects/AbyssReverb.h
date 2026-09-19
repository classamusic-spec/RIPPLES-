#pragma once

/*
    RIPPLES — AbyssReverb.

    An 8 x 8 feedback delay network with Dattorro-style input diffusion:

        in -> predelay -> 4 series allpasses (per channel)
           -> 8 mutually prime, slowly detuned delay lines
           -> per-line damping lowpass + low cut + decay gain
           -> orthonormal 8-point Hadamard mix -> back into the lines

    Why it does not ring or flutter: eight incommensurate line lengths, an
    orthonormal mixing matrix (every reflection is redistributed to every other
    line, so no single path can dominate), four allpasses of input diffusion to
    break up the early pattern, and a slow per-line detune that keeps the modal
    pattern from standing still.

    Why it cannot blow up: the mixing matrix is orthonormal (unit gain, never
    more), every line gain is strictly below one, the in-loop filters only ever
    remove energy, and the injection gain is normalised by sqrt(1 - g^2) so a
    long decay does not also mean a huge steady state.

    Why it reaches true silence: every state variable in the loop goes through
    math::sanitise, which flushes anything below 1e-20 to exact zero, so the
    tail lands on 0.0f rather than grinding through denormals forever.
*/

#include <juce_audio_basics/juce_audio_basics.h>

#include "Utilities/DSPConstants.h"
#include "Utilities/MathUtils.h"

#include <vector>

namespace ripples
{

class AbyssReverb
{
public:
    AbyssReverb() = default;

    struct Params
    {
        bool  enabled    = true;
        float size       = 0.6f;      ///< 0..1 network scale (0.35x .. 2.0x)
        float decay      = 0.6f;      ///< 0..1 -> RT60 of 0.4 s .. 20 s
        float predelayMs = 20.0f;     ///< 0 .. dsp::kMaxPredelaySeconds * 1000
        float damping    = 0.5f;      ///< 0..1 HF absorption in the tail
        float lowCutHz   = 120.0f;    ///< in-loop high pass
        float highCutHz  = 9000.0f;   ///< in-loop low pass
        float modulation = 0.3f;      ///< 0..1 slow detune of the lines
        float mix        = 0.3f;      ///< 0..1 (equal power)
    };

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;
    void setParams (const Params& p) noexcept;
    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    static constexpr int   kNumLines        = 8;
    static constexpr int   kNumInputAllPass = 4;
    static constexpr int   kControlInterval = dsp::kModBlockSize;
    static constexpr float kMaxSizeScale    = 2.0f;
    static constexpr float kMaxLineMs       = 80.0f;
    static constexpr float kMaxModMs        = 2.5f;
    static constexpr float kMaxLineGain     = 0.9995f;
    static constexpr float kStateCeiling    = 8.0f;   // catastrophic backstop only
    /** Max line-length change per sample; far above the modulation rate,
        so it only ever tames a SIZE sweep into a smooth morph. */
    static constexpr float kDelaySlew       = 0.3f;

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

    struct AllPassDelay
    {
        std::vector<float> buf;
        int size = 0, mask = 0, writePos = 0, delay = 1;

        void allocate (int delaySamples)
        {
            delay = std::max (1, delaySamples);
            int n = 8;
            while (n < delay + 4) n <<= 1;
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

        inline float process (float x, float g) noexcept
        {
            const float delayed = buf[(size_t) ((writePos - delay) & mask)];
            const float v = math::sanitise (x + g * delayed);

            writePos = (writePos + 1) & mask;
            buf[(size_t) writePos] = v;

            return delayed - g * v;
        }
    };

    void updateControlRate() noexcept;

    double sampleRate_ = 44100.0;
    Params params_ {};

    DelayLine    predelay_[2];
    AllPassDelay inputAp_[2][kNumInputAllPass];

    DelayLine line_[kNumLines];
    float baseSamples_[kNumLines] {};
    float delay_[kNumLines] {}, delayTarget_[kNumLines] {};
    float lineGain_[kNumLines] {}, lineGainTarget_[kNumLines] {};
    float dampState_[kNumLines] {}, lowCutState_[kNumLines] {};
    float modPhase_[kNumLines] {}, modInc_[kNumLines] {};

    float predelaySamples_ = 0.0f, predelayTarget_ = 0.0f;
    float sizeScale_ = 1.0f, sizeTarget_ = 1.0f;
    float modDepthTarget_ = 0.0f;
    float rt60Seconds_ = 2.0f;

    float dampCoeff_ = 1.0f, lowCutCoeff_ = 0.01f;
    float inGain_ = 0.2f, inGainTarget_ = 0.2f;

    float dryGain_ = 1.0f, wetGain_ = 0.0f;
    float dryTarget_ = 1.0f, wetTarget_ = 0.0f;

    float smoothCoeff_ = 0.01f;
    float delaySmooth_ = 0.01f;
    float enableGain_  = 1.0f;
    float enableCoeff_ = 0.01f;
    int   controlCounter_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AbyssReverb)
};

} // namespace ripples
