#include "DSP/Resonators/WaterResonator.h"

#include <cmath>

namespace ripples
{

namespace
{
    // A plain harmonic series (bowls, glass, tuned water) ...
    constexpr float kHarmonic[dsp::kNumResonatorBanks] =
        { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f };

    // ... morphing toward bell / circular-plate partials (metallic, submerged bells).
    constexpr float kInharmonic[dsp::kNumResonatorBanks] =
        { 1.0f, 2.76f, 5.40f, 8.93f, 13.34f, 18.64f, 24.70f, 31.50f };

    // Slow, mutually prime-ish drift rates in Hz, one per bank.
    constexpr float kMotionRates[dsp::kNumResonatorBanks] =
        { 0.031f, 0.047f, 0.067f, 0.083f, 0.109f, 0.131f, 0.167f, 0.191f };

    constexpr float kMaxPoleRadius   = 0.99995f;   // strictly < 1 — never self-oscillates
    constexpr float kMinRingSeconds  = 0.008f;
    constexpr float kMaxRingSeconds  = 8.0f;
    constexpr float kBankLimit       = 6.0f;       // in-loop saturator ceiling
    constexpr float kWetLimit        = 6.0f;       // output saturator ceiling
    constexpr float kWetMakeup       = 1.6f;
    constexpr float kStereoDetune    = 0.0035f;    // +/- 0.35% between L and R

    inline float softLimit (float x, float limit) noexcept
    {
        return math::fastTanh (x / limit) * limit;
    }
}

//==============================================================================
void WaterResonator::prepare (double newSampleRate, uint32_t seed)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;

    banks.assign ((size_t) (kNumBanks * kNumChannels), Bank {});

    RandomGenerator g (seed);

    for (int c = 0; c < kNumChannels; ++c)
    {
        for (int i = 0; i < kNumBanks; ++i)
        {
            auto& b = banks[(size_t) (c * kNumBanks + i)];

            // Fixed decorrelation: the two channels sit either side of the tuning.
            b.detune = 1.0f + (c == 0 ? -kStereoDetune : kStereoDetune)
                                * (1.0f + 0.4f * g.nextBipolar());

            // Organic inharmonicity, only heard once SCATTER is up.
            b.scatterJitter = g.nextBipolar() * 0.30f;
            b.gainTrim      = 1.0f + g.nextBipolar() * 0.22f;

            b.lfoPhase = g.nextFloat();
            b.lfoInc   = kMotionRates[i] * (float) dsp::kModBlockSize / (float) sampleRate;
        }
    }

    // Control-rate smoothing is applied once per kModBlockSize samples.
    const double ctlRate = sampleRate / (double) dsp::kModBlockSize;
    ctlCoeff    = math::clamp (math::onePoleCoeff (dsp::kSmoothingSeconds, ctlRate), 0.0f, 1.0f);
    mixCoeff    = math::clamp (math::onePoleCoeff (dsp::kSmoothingSeconds, sampleRate), 0.0f, 1.0f);

    reset();
}

void WaterResonator::reset() noexcept
{
    for (auto& b : banks)
        b.y1 = b.y2 = 0.0f;

    xz1.fill (0.0f);
    xz2.fill (0.0f);
    dampLpZ.fill (0.0f);

    sizeS    = params.size;
    decayS   = params.decay;
    dampingS = params.damping;
    scatterS = params.scatter;
    motionS  = params.motion;
    amountS  = params.amount;
    baseHz   = baseTargetHz;

    ctlCountdown = 0;
    updateCoefficients();

    dryGain = dryTarget;
    wetGain = wetTarget;
}

void WaterResonator::setParams (const Params& p) noexcept
{
    params.amount  = math::clamp (p.amount,  0.0f, 1.0f);
    params.size    = math::clamp (p.size,    0.0f, 1.0f);
    params.decay   = math::clamp (p.decay,   0.0f, 1.0f);
    params.damping = math::clamp (p.damping, 0.0f, 1.0f);
    params.scatter = math::clamp (p.scatter, 0.0f, 1.0f);
    params.motion  = math::clamp (p.motion,  0.0f, 1.0f);
}

void WaterResonator::setBaseFrequency (float hz) noexcept
{
    if (! std::isfinite (hz))
        return;

    baseTargetHz = math::clamp (hz, 20.0f, 8000.0f);
}

float WaterResonator::getBankFrequency (int channel, int bank) const noexcept
{
    const int idx = math::clamp (channel, 0, kNumChannels - 1) * kNumBanks
                  + math::clamp (bank, 0, kNumBanks - 1);
    return banks.empty() ? 0.0f : banks[(size_t) idx].frequency;
}

//==============================================================================
void WaterResonator::updateCoefficients() noexcept
{
    if (banks.empty())
        return;

    // Smooth everything at control rate — no coefficient ever jumps.
    sizeS    += (params.size    - sizeS)    * ctlCoeff;
    decayS   += (params.decay   - decayS)   * ctlCoeff;
    dampingS += (params.damping - dampingS) * ctlCoeff;
    scatterS += (params.scatter - scatterS) * ctlCoeff;
    motionS  += (params.motion  - motionS)  * ctlCoeff;
    amountS  += (params.amount  - amountS)  * ctlCoeff;
    baseHz   += (baseTargetHz   - baseHz)   * ctlCoeff;

    math::equalPowerMix (amountS, dryTarget, wetTarget);

    // SIZE: +/- 2 octaves around the played note. Big bowl low, bubble high.
    const float f0 = math::clamp (baseHz * std::pow (2.0f, (0.5f - sizeS) * 4.0f),
                                  20.0f, (float) (sampleRate * 0.35));

    const float ringSeconds = math::clamp (kMinRingSeconds
                                             * std::pow (kMaxRingSeconds / kMinRingSeconds, decayS),
                                           kMinRingSeconds, kMaxRingSeconds);

    // DAMPING as a submerged-versus-glassy control: a one-pole over the whole
    // wet path on top of the per-mode high-frequency loss below.
    {
        const float fc = math::normToFreq (math::lerp (1.0f, 0.22f, dampingS), 20.0f, 20000.0f);
        dampLpCoeff = math::clamp (math::onePoleCoeff (1.0f / (math::twoPi * fc), sampleRate),
                                   0.0f, 1.0f);
    }

    const float nyquistLimit = (float) (sampleRate * 0.45);

    for (int c = 0; c < kNumChannels; ++c)
    {
        float gainSum = 0.0f;
        const int base = c * kNumBanks;

        for (int i = 0; i < kNumBanks; ++i)
        {
            auto& b = banks[(size_t) (base + i)];

            b.lfoPhase += b.lfoInc;
            b.lfoPhase -= std::floor (b.lfoPhase);

            // SCATTER: harmonic -> bell/plate, plus a little per-bank jitter.
            float ratio = math::lerp (kHarmonic[i], kInharmonic[i], scatterS);
            ratio *= 1.0f + b.scatterJitter * scatterS * 0.25f;
            ratio *= b.detune;

            // MOTION: a slow, decorrelated wander so the bank is never static.
            const float lfo = std::sin (math::twoPi * b.lfoPhase);
            ratio *= 1.0f + lfo * motionS * 0.035f;
            ratio  = std::max (ratio, 0.05f);

            float f = f0 * ratio;

            // Fade a partial out rather than parking it on Nyquist.
            const float rolloff = math::smoothstep ((nyquistLimit - f) / (0.18f * nyquistLimit));
            f = math::clamp (f, 10.0f, nyquistLimit);
            b.frequency = f;

            const float theta = math::twoPi * f / (float) sampleRate;

            // Higher partials lose energy faster — the essence of "submerged".
            const float ring = math::clamp (ringSeconds / (1.0f + dampingS * 5.0f * (ratio - 1.0f)),
                                            0.004f, kMaxRingSeconds);
            const float R = math::clamp (math::decayCoeff (ring, sampleRate), 0.0f, kMaxPoleRadius);

            b.b1 =  2.0f * R * std::cos (theta);
            b.b2 = -R * R;
            b.a0 =  0.5f * (1.0f - R * R);      // constant peak gain of 1

            const float level = 1.0f / std::pow ((float) (i + 1), 0.75f)
                                  * std::exp (-dampingS * 0.45f * (float) i);
            b.gain = level * rolloff * b.gainTrim;
            gainSum += b.gain;
        }

        // Normalise so loudness does not lurch when DAMPING or SCATTER moves.
        const float norm = 1.0f / std::max (gainSum, 1.0e-6f);
        for (int i = 0; i < kNumBanks; ++i)
            banks[(size_t) (base + i)].gain *= norm;
    }
}

//==============================================================================
void WaterResonator::processSample (float& l, float& r) noexcept
{
    if (banks.empty())
        return;

    if (--ctlCountdown <= 0)
    {
        updateCoefficients();
        ctlCountdown = dsp::kModBlockSize;
    }

    dryGain += (dryTarget - dryGain) * mixCoeff;
    wetGain += (wetTarget - wetGain) * mixCoeff;

    const float dry[kNumChannels] = { math::sanitise (l), math::sanitise (r) };
    float wet[kNumChannels] = { 0.0f, 0.0f };

    for (int c = 0; c < kNumChannels; ++c)
    {
        const float x = dry[c];

        // Zeros at DC and Nyquist: nothing can accumulate as offset.
        const float in = x - xz2[(size_t) c];
        xz2[(size_t) c] = xz1[(size_t) c];
        xz1[(size_t) c] = x;

        float sum = 0.0f;
        const int base = c * kNumBanks;

        for (int i = 0; i < kNumBanks; ++i)
        {
            auto& b = banks[(size_t) (base + i)];

            float y = b.a0 * in + b.b1 * b.y1 + b.b2 * b.y2;
            y = math::sanitise (softLimit (y, kBankLimit));   // bounded, never NaN

            b.y2 = b.y1;
            b.y1 = y;

            sum += y * b.gain;
        }

        // Submerged damping over the whole bank.
        auto& z = dampLpZ[(size_t) c];
        z = math::sanitise (z + (sum - z) * dampLpCoeff);
        wet[c] = math::sanitise (softLimit (z * kWetMakeup, kWetLimit));
    }

    l = math::sanitise (dry[0] * dryGain + wet[0] * wetGain);
    r = math::sanitise (dry[1] * dryGain + wet[1] * wetGain);
}

} // namespace ripples
