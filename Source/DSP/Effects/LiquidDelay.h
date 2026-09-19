#pragma once

/*
    RIPPLES — LiquidDelay.

    Stereo fractional delay, up to dsp::kMaxDelaySeconds.

      * The read pointer is a 4-point Catmull-Rom (cubic) interpolator, and the
        delay time itself is one-pole smoothed, so MOTION bends pitch the way a
        tape machine does instead of clicking, and moving TIME while the
        feedback is ringing glides rather than glitches.
      * DAMPING is a one-pole lowpass inside the loop, so each repeat is darker
        than the last. A fixed 35 Hz highpass in the same loop stops low
        frequency energy from ever accumulating.
      * DIFFUSION runs three Schroeder allpasses inside the loop. Their mean
        delay is subtracted from the main read so the repeat time stays honest.
      * SPREAD offsets the two channel times and rotates the feedback matrix
        from "dual mono" to a full ping-pong swap. The matrix is a rotation, so
        it is energy preserving at every setting and never collapses to mono.
      * The loop gain is hard bounded below unity and the recirculating signal
        passes a soft bound, so it can sing near maximum without running away.
*/

#include <juce_audio_basics/juce_audio_basics.h>

#include "Utilities/DSPConstants.h"
#include "Utilities/MathUtils.h"
#include "Utilities/RandomGenerator.h"

#include <vector>

namespace ripples
{

class LiquidDelay
{
public:
    LiquidDelay() = default;

    struct Params
    {
        bool  enabled     = true;
        float timeSeconds = 0.4f;   ///< 0.025 .. dsp::kMaxDelaySeconds
        float feedback    = 0.4f;   ///< 0..1
        float motion      = 0.3f;   ///< 0..1 delay-time flutter
        float spread      = 0.4f;   ///< 0..1 L/R offset + ping-pong rotation
        float damping     = 0.5f;   ///< 0..1 darkens each repeat
        float diffusion   = 0.3f;   ///< 0..1 smears the repeats
        float mix         = 0.3f;   ///< 0..1 (equal power)
    };

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;
    void setParams (const Params& p) noexcept;
    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    static constexpr int   kNumAllPass      = 3;
    static constexpr int   kControlInterval = dsp::kModBlockSize;
    static constexpr float kMinTimeSeconds  = 0.025f;
    static constexpr float kMaxFeedback     = 0.995f;

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

    /** Fixed-length Schroeder allpass built on a plain circular buffer. */
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
    RandomGenerator rng_ { 0x51D3B1A7u };

    DelayLine    line_[2];
    AllPassDelay ap_[2][kNumAllPass];

    float apCompSamples_[2] {};        // mean delay of each allpass chain

    float timeTarget_ = 0.4f;          // seconds
    float timeSmoothed_[2] {};         // samples, per channel
    float timeCoeff_ = 0.001f;

    float modPhase_[2] {};
    float modPhase2_[2] {};
    float modInc_ = 0.0f, modInc2_ = 0.0f;
    float modDepth_ = 0.0f, modDepthTarget_ = 0.0f;
    float noiseValue_[2] {}, noiseTarget_[2] {};
    float modOffset_[2] {};            // smoothed modulation, in samples

    float dampState_[2] {}, dampCoeff_ = 0.5f;
    float hpState_[2] {},   hpCoeff_ = 0.01f;

    float fbGain_ = 0.0f, fbGainTarget_ = 0.0f;
    float apGain_ = 0.0f, apGainTarget_ = 0.0f;
    float rotCos_ = 1.0f, rotSin_ = 0.0f;
    float rotCosTarget_ = 1.0f, rotSinTarget_ = 0.0f;

    float dryGain_ = 1.0f, wetGain_ = 0.0f;
    float dryTarget_ = 1.0f, wetTarget_ = 0.0f;

    float smoothCoeff_ = 0.01f;
    float enableGain_  = 1.0f;
    float enableCoeff_ = 0.01f;
    int   controlCounter_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiquidDelay)
};

} // namespace ripples
