#pragma once

/*
    RIPPLES — LiquidChorus.

    Three modulated short delay taps per channel (six voices total) reading a
    fractional delay with 4-point Catmull-Rom interpolation. The tap times are
    generated at control rate and then one-pole smoothed per sample, so the read
    pointer never steps: no zipper noise, no quantisation grit, just a smooth
    Doppler glide.

    Small depth and a 6-12 ms base delay give a gentle widening; large depth
    with a longer base delay and some feedback gives the deep underwater warble.
*/

#include <juce_audio_basics/juce_audio_basics.h>

#include "Utilities/DSPConstants.h"
#include "Utilities/MathUtils.h"

#include <vector>

namespace ripples
{

class LiquidChorus
{
public:
    LiquidChorus() = default;

    struct Params
    {
        bool  enabled  = true;
        float rate     = 0.4f;    ///< Hz
        float depth    = 0.4f;    ///< 0..1 modulation depth
        float delayMs  = 12.0f;   ///< base delay in milliseconds
        float feedback = 0.15f;   ///< 0..1 (internally bounded well below unity)
        float width    = 0.6f;    ///< 0..1 stereo spread of the modulation
        float mix      = 0.35f;   ///< 0..1 dry/wet (equal power)
    };

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;
    void setParams (const Params& p) noexcept;
    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    static constexpr int   kNumTaps         = 3;
    static constexpr int   kControlInterval = dsp::kModBlockSize;
    static constexpr float kMaxBufferSec    = 0.14f;   // 40 ms base + 20 ms mod + guard
    static constexpr float kMaxBaseMs       = 40.0f;
    static constexpr float kMaxModMs        = 20.0f;
    /** Deliberately generous: the deepest, fastest modulation needs about
        1.0, so this never touches the warble, only a dragged knob. */
    static constexpr float kTapSlew         = 2.0f;

    /** Power-of-two circular delay line with cubic (Catmull-Rom) reads. */
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

    DelayLine line_[2];

    float phase_ = 0.0f, phaseInc_ = 0.0f;
    float slowPhase_ = 0.0f, slowInc_ = 0.0f;

    float tapDelay_[2][kNumTaps] {};        // smoothed, in samples
    float tapTarget_[2][kNumTaps] {};

    float fbState_[2] {};                   // feedback lowpass state
    float fbCoeff_ = 0.4f;
    float fbGain_ = 0.0f, fbGainTarget_ = 0.0f;
    float crossFeed_ = 0.0f;

    float dryGain_ = 1.0f, wetGain_ = 0.0f;
    float dryTarget_ = 1.0f, wetTarget_ = 0.0f;
    float widthGain_ = 1.0f, widthTarget_ = 1.0f;

    float smoothCoeff_ = 0.01f;
    float delaySmooth_ = 0.01f;
    float enableGain_  = 1.0f;
    float enableCoeff_ = 0.01f;
    int   controlCounter_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiquidChorus)
};

} // namespace ripples
