#include "DSP/Oscillators/SubOscillator.h"
#include "Utilities/MathUtils.h"

#include <cmath>

namespace ripples
{

namespace
{
    /** Read-only sine table, built once at static-init time. */
    struct SubSineTable
    {
        static constexpr int kSize = 4096;
        static constexpr int kMask = kSize - 1;
        float t[kSize + 1];

        SubSineTable() noexcept
        {
            for (int i = 0; i <= kSize; ++i)
                t[i] = std::sin (math::twoPi * (float) i / (float) kSize);
        }

        inline float operator() (float cycles) const noexcept
        {
            const float x  = cycles * (float) kSize;
            const float fl = std::floor (x);
            const float fr = x - fl;
            const int   i  = ((int) fl) & kMask;
            return t[i] + (t[i + 1] - t[i]) * fr;
        }
    };

    const SubSineTable subSine;
}

//==============================================================================
void SubOscillator::prepare (double sampleRate)
{
    sampleRate_    = sampleRate > 0.0 ? sampleRate : 44100.0;
    invSampleRate_ = (float) (1.0 / sampleRate_);
    maxFreq_       = (float) (sampleRate_ * 0.48);

    setParams (params_);
    setFrequency (baseHz_);
    reset();
}

//==============================================================================
void SubOscillator::reset() noexcept
{
    phase_ = 0.0f;
}

//==============================================================================
void SubOscillator::setParams (const Params& p) noexcept
{
    params_ = p;
    params_.wave = (p.wave >= SubWave::NumWaves || p.wave < SubWave::Sine) ? SubWave::Sine : p.wave;

    // Anything other than -2 is treated as one octave down, so a stray value
    // can never send the sub somewhere silly.
    octaveRatio_ = (p.octave <= -2) ? 0.25f : 0.5f;

    setFrequency (baseHz_);
}

//==============================================================================
void SubOscillator::setFrequency (float baseHz) noexcept
{
    if (! (baseHz > 0.0f))      // also catches NaN
        baseHz = 0.001f;

    baseHz_ = math::clamp (baseHz, 0.001f, maxFreq_);

    const float hz = math::clamp (baseHz_ * octaveRatio_, 0.001f, maxFreq_);
    inc_ = math::clamp (hz * invSampleRate_, 0.0f, 0.49f);
}

//==============================================================================
float SubOscillator::processSample() noexcept
{
    const float p  = phase_;
    const float dt = inc_;

    float y = 0.0f;

    switch (params_.wave)
    {
        case SubWave::Triangle:
        {
            y = (p < 0.5f) ? (4.0f * p - 1.0f) : (3.0f - 4.0f * p);

            // Slope flips by 8 per cycle at both corners; polyBLAMP rounds them.
            const float slopeStep = 8.0f * dt;
            y += 0.5f * slopeStep * math::polyBlamp (p, dt);
            y -= 0.5f * slopeStep * math::polyBlamp (math::wrapPhase (p - 0.5f), dt);
            break;
        }

        case SubWave::Square:
        {
            y = (p < 0.5f) ? 1.0f : -1.0f;
            y += math::polyBlep (p, dt);
            y -= math::polyBlep (math::wrapPhase (p - 0.5f), dt);
            break;
        }

        case SubWave::Sine:
        case SubWave::NumWaves:
        default:
            y = subSine (p);
            break;
    }

    phase_ += dt;
    if (phase_ >= 1.0f)
        phase_ -= std::floor (phase_);

    return math::sanitise (y);
}

} // namespace ripples
