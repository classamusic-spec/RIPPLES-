#include "DSP/Modulation/TideLFO.h"

#include "Utilities/MathUtils.h"
#include "Utilities/DSPConstants.h"

#include <cmath>

namespace ripples
{

namespace
{
    /** Wraps a double phase into [0,1) without losing precision to a float cast. */
    inline double wrap01 (double p) noexcept
    {
        p -= std::floor (p);
        return p < 0.0 ? p + 1.0 : p;
    }

    /** Sine with phase distortion: still C-infinity and zero mean, but the rise
        and the fall are no longer mirror images. |result| <= 1. */
    inline float skewSine (double p, float skew) noexcept
    {
        const float x = math::twoPi * (float) wrap01 (p);
        return std::sin (x + skew * std::sin (x));
    }

    /** Smooth bound to (-1,1): unity slope below the knee, asymptotic above it.
        Used only by Flow, whose partials can briefly sum past one. */
    inline float softBound (float x) noexcept
    {
        constexpr float knee = 0.72f;
        const float a = std::fabs (x);
        if (a <= knee)
            return x;

        const float y = knee + (1.0f - knee) * std::tanh ((a - knee) / (1.0f - knee));
        return x < 0.0f ? -y : y;
    }

    // Flow's secondary accumulators run at mutually irrational ratios
    // (sqrt(2)-1 and 2-sqrt(3)) so the composite contour never repeats.
    constexpr double kFlowRatioB = 0.4142135623730951;
    constexpr double kFlowRatioC = 0.2679491924311227;

    constexpr float kShapeFadeSeconds = 0.012f;

    // Swell: the crest arrives late in the cycle, then the wave breaks.
    constexpr float kSwellCrest    = 0.78f;
    constexpr float kSwellRiseWarp = 1.35f;   // >1 : gathers slowly, then lifts
    constexpr float kSwellFallWarp = 0.70f;   // <1 : drops away immediately

    // DoubleWave: the second crest is shorter *and* smaller. The amplitude is
    // chosen as (1-split)/split so the slopes match at the seam and at the wrap,
    // which makes the whole shape C1 while still sounding uneven.
    constexpr float kDoubleSplit = 0.56f;
    constexpr float kDoubleAmp   = (1.0f - kDoubleSplit) / kDoubleSplit;
}

//==============================================================================
void TideLFO::prepare (double newSampleRate)
{
    sampleRate    = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    invSampleRate = 1.0 / sampleRate;

    fadeLengthSamples = (int) (kShapeFadeSeconds * sampleRate);
    if (fadeLengthSamples < 1)
        fadeLengthSamples = 1;

    setParams (params);
    reset (0.0f);
}

void TideLFO::reset (float phase01) noexcept
{
    const double p = wrap01 ((double) phase01);

    phase  = p;
    phaseB = wrap01 (p * kFlowRatioB);
    phaseC = wrap01 (p * kFlowRatioC);

    fadeRemaining  = 0;
    fadeFromShape  = params.shape;

    updateValues();
}

void TideLFO::setParams (const Params& p) noexcept
{
    if (p.shape != params.shape)
    {
        fadeFromShape = params.shape;
        fadeRemaining = fadeLengthSamples;
    }

    params = p;
    params.rateHz      = math::clamp (params.rateHz, kMinRateHz, kMaxRateHz);
    params.phaseOffset = math::clamp (params.phaseOffset, 0.0f, 1.0f);
    params.stereoPhase = math::clamp (params.stereoPhase, 0.0f, 1.0f);

    increment = (double) params.rateHz * invSampleRate;
}

//==============================================================================
float TideLFO::advance (int numSamples) noexcept
{
    if (numSamples > 0)
    {
        const double step = increment * (double) numSamples;

        phase  = wrap01 (phase  + step);
        phaseB = wrap01 (phaseB + step * kFlowRatioB);
        phaseC = wrap01 (phaseC + step * kFlowRatioC);

        if (fadeRemaining > 0)
        {
            fadeRemaining -= numSamples;
            if (fadeRemaining < 0)
                fadeRemaining = 0;
        }
    }

    updateValues();
    return value;
}

void TideLFO::updateValues() noexcept
{
    const double off    = (double) params.phaseOffset;
    const double stereo = (double) params.stereoPhase;

    float l = shapeValue (params.shape, phase + off,          phaseB + off,          phaseC + off);
    float r = shapeValue (params.shape, phase + off + stereo, phaseB + off + stereo, phaseC + off + stereo);

    if (fadeRemaining > 0)
    {
        const float mix = math::smoothstep (1.0f - (float) fadeRemaining / (float) fadeLengthSamples);

        const float oldL = shapeValue (fadeFromShape, phase + off,          phaseB + off,          phaseC + off);
        const float oldR = shapeValue (fadeFromShape, phase + off + stereo, phaseB + off + stereo, phaseC + off + stereo);

        l = math::lerp (oldL, l, mix);
        r = math::lerp (oldR, r, mix);
    }

    value  = math::clamp (math::sanitise (l), -1.0f, 1.0f);
    valueR = math::clamp (math::sanitise (r), -1.0f, 1.0f);
}

//==============================================================================
float TideLFO::shapeValue (TideShape s, double p1, double p2, double p3) const noexcept
{
    const float t = (float) wrap01 (p1);

    switch (s)
    {
        case TideShape::Sine:
            return std::sin (math::twoPi * t);

        case TideShape::Triangle:
            // Starts at 0 and rises, so it is phase-aligned with the sine and
            // returns to 0 at the wrap.
            if (t < 0.25f) return 4.0f * t;
            if (t < 0.75f) return 2.0f - 4.0f * t;
            return 4.0f * t - 4.0f;

        case TideShape::Swell:
        {
            // Long gathering rise, short break. Both segments are smootherstep
            // over a warped argument, so the slope is zero at -1, at the crest
            // and at the wrap: no corner anywhere in the cycle.
            if (t < kSwellCrest)
            {
                const float r = t / kSwellCrest;
                return -1.0f + 2.0f * math::smootherstep (std::pow (r, kSwellRiseWarp));
            }

            const float f = (t - kSwellCrest) / (1.0f - kSwellCrest);
            return 1.0f - 2.0f * math::smootherstep (std::pow (f, kSwellFallWarp));
        }

        case TideShape::DoubleWave:
        {
            // Two crests, the second one quicker and smaller.
            if (t < kDoubleSplit)
                return std::sin (math::twoPi * (t / kDoubleSplit));

            return kDoubleAmp * std::sin (math::twoPi * ((t - kDoubleSplit) / (1.0f - kDoubleSplit)));
        }

        case TideShape::Flow:
        {
            // Three skewed sines at irrational rate ratios: always the same
            // character, never the same contour twice.
            const float a = skewSine (p1,         0.45f);
            const float b = skewSine (p2 + 0.14, -0.30f);
            const float c = skewSine (p3 + 0.37,  0.22f);

            return softBound (1.12f * (0.70f * a + 0.21f * b + 0.11f * c));
        }

        case TideShape::NumShapes:
        default:
            break;
    }

    return 0.0f;
}

} // namespace ripples
