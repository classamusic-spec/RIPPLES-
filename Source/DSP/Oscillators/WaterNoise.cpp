#include "DSP/Oscillators/WaterNoise.h"
#include "Utilities/MathUtils.h"
#include "Utilities/DSPConstants.h"

#include <cmath>

namespace ripples
{

namespace
{
    constexpr int kControlInterval = 32;

    /** Paul Kellet's pink filter, specified at 44.1 kHz. prepare() re-fits the
        poles (and their gains) to the running sample rate so the -3 dB/oct
        slope holds at 48, 96 and 192 kHz too. */
    constexpr float kPinkPole44[6] { 0.99886f, 0.99332f, 0.96900f, 0.86650f, 0.55000f, -0.76160f };
    constexpr float kPinkGain44[6] { 0.0555179f, 0.0750759f, 0.1538520f, 0.3104856f, 0.5329522f, -0.0168980f };

    /** Per-type output trim at tone = 0, 0.5 and 1, measured offline so that
        every colour arrives at the filter at the same working level. */
    constexpr float kTypeGain[(size_t) NoiseType::NumTypes][3]
    {
        { 1.059f, 0.519f, 0.268f },     // White
        { 0.147f, 0.231f, 0.186f },     // Pink
        { 0.421f, 0.417f, 0.573f },     // Deep
        { 2.731f, 2.091f, 0.863f },     // Surf
        { 1.047f, 1.337f, 0.964f },     // Bubble
        { 3.417f, 0.716f, 0.421f }      // Air
    };

    /** sin(2.pi.p) to about 1%, which is plenty for a 0.1 Hz swell and costs
        no table and no libm call. */
    inline float fastSin01 (float p) noexcept
    {
        const float x = 2.0f * math::wrapPhase (p) - 1.0f;          // -1..1
        const float y = -4.0f * x * (1.0f - std::fabs (x));
        return y * (0.775f + 0.225f * std::fabs (y));
    }

    inline float onePoleFromHz (float hz, float invSampleRate) noexcept
    {
        const float c = 1.0f - std::exp (-math::twoPi * hz * invSampleRate);
        return math::clamp (c, 0.0f, 1.0f);
    }

    /** Coefficient for a topology-preserving (bilinear) one-pole. Unlike the
        naive form its corner lands on the requested frequency and its
        complementary highpass keeps full gain right up to Nyquist, so the
        brightness of the tone tilt and of AIR is the same at 44.1 kHz as it is
        at 192 kHz. */
    inline float tptCoeff (float hz, double sampleRate) noexcept
    {
        const float w = math::clamp ((float) (hz / sampleRate), 1.0e-5f, 0.49f);
        const float g = std::tan (math::pi * w);
        return g / (1.0f + g);
    }

    /** Runs one TPT one-pole and returns its lowpass output; x - result is the
        matching highpass. */
    inline float tptLowpass (float& state, float x, float coeff) noexcept
    {
        const float v  = (x - state) * coeff;
        const float lp = v + state;
        state = lp + v;
        return lp;
    }
}

//==============================================================================
void WaterNoise::prepare (double sampleRate, uint32_t seed)
{
    sampleRate_     = sampleRate > 0.0 ? sampleRate : 44100.0;
    invSampleRate_  = (float) (1.0 / sampleRate_);
    controlSeconds_ = (float) kControlInterval * invSampleRate_;

    // Re-fit the pink cascade to this sample rate: keep each stage's corner
    // frequency (pole^(44100/sr)) and its low-frequency gain.
    const float exponent = (float) (44100.0 / sampleRate_);
    for (int i = 0; i < 6; ++i)
    {
        const float mag  = std::fabs (kPinkPole44[i]);
        const float sign = kPinkPole44[i] < 0.0f ? -1.0f : 1.0f;
        const float newMag = math::clamp (std::pow (mag, exponent), 0.0f, 0.99999f);

        pinkPole_[i] = sign * newMag;
        pinkGain_[i] = kPinkGain44[i] * ((1.0f - newMag) / (1.0f - mag));
    }

    // Band limit the raw source to a fixed 19 kHz, whatever the sample rate.
    // Without it every fixed-corner filter downstream (the air highpass above
    // all) would gain a whole extra octave of energy at 96 or 192 kHz, and the
    // noise would arrive at the filter far louder than it does at 44.1 kHz.
    {
        const float g = math::clamp (onePoleFromHz (19000.0f, invSampleRate_), 1.0e-4f, 1.0f);
        const float a = 1.0f - g;
        const float a2 = a * a;
        const float variance = (g * g * g * g) * (1.0f + a2)
                             / std::max (1.0e-9f, (1.0f - a2) * (1.0f - a2) * (1.0f - a2));
        bandLimitCoef_ = g;
        whiteGain_ = 1.0f / std::sqrt (std::max (1.0e-9f, variance));
    }

    tiltCoef_ = tptCoeff (700.0f, sampleRate_);
    dcCoef_   = std::exp (-math::twoPi * 5.0f * invSampleRate_);
    toneCoef_ = math::clamp (math::onePoleCoeff (0.015f, sampleRate_), 0.0f, 1.0f);
    blipProb_ = math::clamp (22.0f * invSampleRate_, 0.0f, 1.0f);     // a couple of dozen bubbles a second

    left_  = Channel {};
    right_ = Channel {};
    left_.rng.seed (seed * 2654435761u + 1u);
    right_.rng.seed (seed * 2246822519u + 0x9E3779B9u);

    left_.surfPhase  = 0.0f;
    right_.surfPhase = 0.37f;          // the two sides never swell together
    left_.deepPhase  = 0.11f;
    right_.deepPhase = 0.63f;

    toneTarget_ = toneSmoothed_ = math::clamp (params_.tone, 0.0f, 1.0f);
    controlCounter_ = 0;

    updateControl (left_,  params_.type, toneSmoothed_);
    updateControl (right_, params_.type, toneSmoothed_);
}

//==============================================================================
void WaterNoise::reset() noexcept
{
    // The noise itself keeps running — a continuous source has nothing to
    // retrigger, and clearing the filters would fade the low colours in on
    // every note. Only the event pool and any stray non-finite state go.
    const auto clearChannel = [] (Channel& ch) noexcept
    {
        for (auto& b : ch.blips)
        {
            b.active = false;
            b.re = b.im = 0.0f;
        }

        for (auto& v : ch.white)  v = math::sanitise (v);
        for (auto& v : ch.pink)   v = math::sanitise (v);
        for (auto& v : ch.deepLp) v = math::sanitise (v);
        for (auto& v : ch.airLp)  v = math::sanitise (v);

        ch.svfIc1 = math::sanitise (ch.svfIc1);
        ch.svfIc2 = math::sanitise (ch.svfIc2);
        ch.tiltLp = math::sanitise (ch.tiltLp);
        ch.dcX    = math::sanitise (ch.dcX);
        ch.dcY    = math::sanitise (ch.dcY);
        ch.surfAmp = ch.surfAmpTarget;
    };

    clearChannel (left_);
    clearChannel (right_);

    controlCounter_ = 0;
}

//==============================================================================
void WaterNoise::setParams (const Params& p) noexcept
{
    params_ = p;
    params_.type = (p.type >= NoiseType::NumTypes || p.type < NoiseType::White) ? NoiseType::White : p.type;
    toneTarget_  = std::isfinite (p.tone) ? math::clamp (p.tone, 0.0f, 1.0f) : 0.5f;
}

//==============================================================================
void WaterNoise::processSample (float& outL, float& outR) noexcept
{
    toneSmoothed_ += (toneTarget_ - toneSmoothed_) * toneCoef_;

    const bool doControl = (controlCounter_ <= 0);
    if (doControl)
        controlCounter_ = kControlInterval;
    --controlCounter_;

    const NoiseType type = params_.type;
    const float tone = toneSmoothed_;
    const float gain = gainForTone (type, tone);

    outL = math::sanitise (generate (left_,  type, tone, doControl) * gain);
    outR = math::sanitise (generate (right_, type, tone, doControl) * gain);
}

//==============================================================================
float WaterNoise::gainForTone (NoiseType type, float tone) const noexcept
{
    const size_t t = (size_t) math::clamp ((int) type, 0, (int) NoiseType::NumTypes - 1);
    const float x = math::clamp (tone, 0.0f, 1.0f) * 2.0f;

    if (x <= 1.0f)
        return math::lerp (kTypeGain[t][0], kTypeGain[t][1], x);

    return math::lerp (kTypeGain[t][1], kTypeGain[t][2], x - 1.0f);
}

//==============================================================================
float WaterNoise::pinkStep (Channel& ch, float white) const noexcept
{
    ch.pink[0] = pinkPole_[0] * ch.pink[0] + white * pinkGain_[0];
    ch.pink[1] = pinkPole_[1] * ch.pink[1] + white * pinkGain_[1];
    ch.pink[2] = pinkPole_[2] * ch.pink[2] + white * pinkGain_[2];
    ch.pink[3] = pinkPole_[3] * ch.pink[3] + white * pinkGain_[3];
    ch.pink[4] = pinkPole_[4] * ch.pink[4] + white * pinkGain_[4];
    ch.pink[5] = pinkPole_[5] * ch.pink[5] + white * pinkGain_[5];

    const float out = ch.pink[0] + ch.pink[1] + ch.pink[2] + ch.pink[3]
                    + ch.pink[4] + ch.pink[5] + ch.pink[6] + white * 0.5362f;

    ch.pink[6] = white * 0.115926f;
    return out * 0.7f;
}

//==============================================================================
void WaterNoise::updateControl (Channel& ch, NoiseType type, float tone) noexcept
{
    switch (type)
    {
        case NoiseType::Deep:
        {
            // A slow swell on the cutoff keeps the rumble moving like water
            // rather than sitting there like a tone generator.
            ch.deepPhase = math::wrapPhase (ch.deepPhase + 0.07f * controlSeconds_);
            const float centre = math::lerp (48.0f, 260.0f, tone);
            const float hz = math::clamp (centre * (1.0f + 0.18f * fastSin01 (ch.deepPhase)),
                                          10.0f, (float) (sampleRate_ * 0.2));
            ch.deepCoef = onePoleFromHz (hz, invSampleRate_);
            break;
        }

        case NoiseType::Surf:
        {
            ch.surfPhase  = math::wrapPhase (ch.surfPhase + 0.11f * controlSeconds_);
            ch.surfWander += 0.04f * (ch.rng.nextBipolar() - ch.surfWander);

            const float centre = math::lerp (320.0f, 2600.0f, tone);
            const float fc = math::clamp (centre * (1.0f + 0.45f * ch.surfWander),
                                          60.0f, (float) (sampleRate_ * 0.45));

            const float g = std::tan (math::pi * math::clamp (fc * invSampleRate_, 1.0e-5f, 0.49f));
            const float k = 1.0f / 0.85f;                       // gentle Q — a wash, not a whistle
            ch.svfA1 = 1.0f / (1.0f + g * (g + k));
            ch.svfA2 = g * ch.svfA1;
            ch.svfA3 = g * ch.svfA2;
            ch.svfK  = k;

            // Breathing: waves arriving, not a static band of noise.
            ch.surfAmpTarget = 0.30f + 0.70f * (0.5f + 0.5f * fastSin01 (ch.surfPhase));

            ch.svfIc1 = math::sanitise (ch.svfIc1);
            ch.svfIc2 = math::sanitise (ch.svfIc2);
            break;
        }

        case NoiseType::Bubble:
        {
            // Bubbles rise as they shrink, so each blip glides upward while it
            // decays. The angle is refreshed per control block, not per sample.
            for (auto& b : ch.blips)
            {
                if (! b.active)
                    continue;

                b.freq = math::clamp (b.freq * b.glide, 20.0f, (float) (sampleRate_ * 0.45));
                const float w = math::twoPi * b.freq * invSampleRate_;
                b.cosW = std::cos (w);
                b.sinW = std::sin (w);
            }
            break;
        }

        case NoiseType::Air:
        {
            const float hz = math::lerp (2400.0f, 9000.0f, tone);
            ch.airCoef = tptCoeff (math::clamp (hz, 40.0f, (float) (sampleRate_ * 0.45)), sampleRate_);
            break;
        }

        case NoiseType::White:
        case NoiseType::Pink:
        case NoiseType::NumTypes:
        default:
            break;
    }
}

//==============================================================================
float WaterNoise::generate (Channel& ch, NoiseType type, float tone, bool doControl) noexcept
{
    if (doControl)
        updateControl (ch, type, tone);

    const float raw = ch.rng.nextBipolar();
    ch.white[0] += bandLimitCoef_ * (raw - ch.white[0]);
    ch.white[1] += bandLimitCoef_ * (ch.white[0] - ch.white[1]);
    const float white = ch.white[1] * whiteGain_;

    float y = 0.0f;

    switch (type)
    {
        case NoiseType::Pink:
            y = pinkStep (ch, white);
            break;

        case NoiseType::Deep:
        {
            const float x = pinkStep (ch, white);
            const float c = ch.deepCoef;
            ch.deepLp[0] += c * (x - ch.deepLp[0]);
            ch.deepLp[1] += c * (ch.deepLp[0] - ch.deepLp[1]);
            ch.deepLp[2] += c * (ch.deepLp[1] - ch.deepLp[2]);
            y = math::antiDenormal (ch.deepLp[2]);
            break;
        }

        case NoiseType::Surf:
        {
            const float v3 = white - ch.svfIc2;
            const float v1 = ch.svfA1 * ch.svfIc1 + ch.svfA2 * v3;
            const float v2 = ch.svfIc2 + ch.svfA2 * ch.svfIc1 + ch.svfA3 * v3;
            ch.svfIc1 = 2.0f * v1 - ch.svfIc1;
            ch.svfIc2 = 2.0f * v2 - ch.svfIc2;

            ch.surfAmp += (ch.surfAmpTarget - ch.surfAmp) * 0.002f;
            y = v1 * ch.svfK * ch.surfAmp;
            break;
        }

        case NoiseType::Bubble:
        {
            if (ch.rng.nextFloat() < blipProb_)
            {
                for (auto& b : ch.blips)
                {
                    if (b.active)
                        continue;

                    const float lo = math::lerp (180.0f, 700.0f, tone);
                    const float hi = math::lerp (900.0f, 4000.0f, tone);
                    const float f  = math::clamp (lo * std::pow (hi / lo, ch.rng.nextFloat()),
                                                  20.0f, (float) (sampleRate_ * 0.45));
                    const float decaySec = ch.rng.nextRange (0.030f, 0.110f);

                    const float w = math::twoPi * f * invSampleRate_;
                    b.freq   = f;
                    b.cosW   = std::cos (w);
                    b.sinW   = std::sin (w);
                    b.decay  = math::decayCoeff (decaySec, sampleRate_);
                    b.glide  = 1.0f + ch.rng.nextRange (0.004f, 0.020f);   // per control block
                    b.re     = ch.rng.nextRange (0.30f, 0.70f);
                    b.im     = 0.0f;
                    b.active = true;
                    break;
                }
            }

            float sum = 0.0f;
            for (auto& b : ch.blips)
            {
                if (! b.active)
                    continue;

                const float nr = b.re * b.cosW - b.im * b.sinW;
                const float ni = b.re * b.sinW + b.im * b.cosW;
                b.re = nr * b.decay;
                b.im = ni * b.decay;

                sum += b.im;

                if (std::fabs (b.re) + std::fabs (b.im) < 1.0e-5f)
                {
                    b.active = false;
                    b.re = b.im = 0.0f;
                }
            }
            y = sum;
            break;
        }

        case NoiseType::Air:
        {
            // Two complementary one-poles in series: a clean 12 dB/oct highpass.
            const float c = ch.airCoef;
            const float h1 = white - tptLowpass (ch.airLp[0], white, c);
            y = h1 - tptLowpass (ch.airLp[1], h1, c);
            break;
        }

        case NoiseType::White:
        case NoiseType::NumTypes:
        default:
            y = white;
            break;
    }

    // --- tone: a tilt around 700 Hz, flat at 0.5 -----------------------------
    const float tilted = tptLowpass (ch.tiltLp, y, tiltCoef_);
    const float hp = y - tilted;

    float weight = tone;
    if (type == NoiseType::Deep)        weight = 0.5f + (tone - 0.5f) * 0.35f;
    else if (type == NoiseType::Bubble) weight = 0.5f + (tone - 0.5f) * 0.50f;

    y = 2.0f * ((1.0f - weight) * tilted + weight * hp);

    // --- DC blocker ----------------------------------------------------------
    const float dcOut = y - ch.dcX + dcCoef_ * ch.dcY;
    ch.dcX = y;
    ch.dcY = math::sanitise (dcOut);

    return ch.dcY;
}

} // namespace ripples
