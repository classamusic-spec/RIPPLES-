#include "DSP/Effects/LiquidChorus.h"

namespace ripples
{

void LiquidChorus::prepare (double sampleRate, int maxBlockSize)
{
    juce::ignoreUnused (maxBlockSize);   // processed sample-by-sample, in place

    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;

    const int needed = (int) (kMaxBufferSec * (float) sampleRate_) + 8;
    line_[0].allocate (needed);
    line_[1].allocate (needed);

    smoothCoeff_ = math::onePoleCoeff (dsp::kSmoothingSeconds, sampleRate_);
    delaySmooth_ = math::onePoleCoeff (0.004f, sampleRate_);      // ~4 ms glide
    enableCoeff_ = math::onePoleCoeff (dsp::kSmoothingSeconds, sampleRate_);

    // Gentle 6 kHz one-pole in the feedback path keeps repeats from getting spiky.
    const float fc = std::min (6000.0f, 0.45f * (float) sampleRate_);
    fbCoeff_ = 1.0f - std::exp (-math::twoPi * fc / (float) sampleRate_);

    reset();
    setParams (params_);
}

void LiquidChorus::reset() noexcept
{
    line_[0].clear();
    line_[1].clear();

    fbState_[0] = fbState_[1] = 0.0f;

    phase_     = 0.0f;
    slowPhase_ = 0.123f;

    controlCounter_ = 0;
    enableGain_     = params_.enabled ? 1.0f : 0.0f;

    updateControlRate();

    for (int c = 0; c < 2; ++c)
        for (int t = 0; t < kNumTaps; ++t)
            tapDelay_[c][t] = tapTarget_[c][t];

    dryGain_   = dryTarget_;
    wetGain_   = wetTarget_;
    widthGain_ = widthTarget_;
    fbGain_    = fbGainTarget_;
}

void LiquidChorus::setParams (const Params& p) noexcept
{
    params_          = p;
    params_.rate     = math::clamp (p.rate,     0.01f, 8.0f);
    params_.depth    = math::clamp (p.depth,    0.0f,  1.0f);
    params_.delayMs  = math::clamp (p.delayMs,  0.5f,  kMaxBaseMs);
    params_.feedback = math::clamp (p.feedback, 0.0f,  1.0f);
    params_.width    = math::clamp (p.width,    0.0f,  1.0f);
    params_.mix      = math::clamp (p.mix,      0.0f,  1.0f);

    phaseInc_ = params_.rate / (float) sampleRate_;
    slowInc_  = (params_.rate * 0.19f + 0.013f) / (float) sampleRate_;

    // Hard-bounded: 0.68 worst case, and the loop is soft-saturated as well.
    fbGainTarget_ = params_.feedback * 0.68f;
    crossFeed_    = 0.22f * params_.width;

    math::equalPowerMix (params_.mix, dryTarget_, wetTarget_);

    widthTarget_ = 1.0f + params_.width * 0.6f;   // side gain applied to the wet only
}

void LiquidChorus::updateControlRate() noexcept
{
    const float step = (float) kControlInterval;

    phase_     = math::wrapPhase (phase_     + phaseInc_ * step);
    slowPhase_ = math::wrapPhase (slowPhase_ + slowInc_  * step);

    const float msToSamples = (float) sampleRate_ * 0.001f;
    const float baseMs      = params_.delayMs;

    // Never let the modulation drive the tap below ~0.4 ms.
    const float modMs = math::clamp (params_.depth * std::min (baseMs * 0.9f, kMaxModMs),
                                     0.0f, baseMs - 0.4f);

    // A second, much slower phasor drifts the tap spread so the chorus never
    // settles into a static comb — the "liquid" part.
    const float drift = std::sin (math::twoPi * slowPhase_) * 0.12f;

    for (int c = 0; c < 2; ++c)
    {
        const float chOffset = (c == 1) ? 0.5f * params_.width : 0.0f;

        for (int t = 0; t < kNumTaps; ++t)
        {
            const float tapOffset = (float) t / (float) kNumTaps;
            const float ph = math::wrapPhase (phase_ + tapOffset + chOffset);
            const float lfo = std::sin (math::twoPi * ph);

            const float ms = baseMs * (1.0f + drift * (float) (t + 1) * 0.25f)
                           + modMs * lfo;

            tapTarget_[c][t] = math::clamp (ms, 0.2f, kMaxBaseMs + kMaxModMs) * msToSamples;
        }
    }
}

void LiquidChorus::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0 || line_[0].size == 0)
        return;

    const float enableTarget = params_.enabled ? 1.0f : 0.0f;

    if (enableTarget <= 0.0f && enableGain_ <= 1.0e-5f)
    {
        enableGain_ = 0.0f;
        return;
    }

    float* left  = buffer.getWritePointer (0);
    float* right = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    // 1/kNumTaps would lose 10 dB of correlated low end; this keeps the wet
    // signal at roughly dry level for a typical programme.
    constexpr float tapGain = 0.52f;

    for (int n = 0; n < numSamples; ++n)
    {
        if (controlCounter_ <= 0)
        {
            updateControlRate();
            controlCounter_ = kControlInterval;
        }
        --controlCounter_;

        dryGain_   += (dryTarget_   - dryGain_)   * smoothCoeff_;
        wetGain_   += (wetTarget_   - wetGain_)   * smoothCoeff_;
        widthGain_ += (widthTarget_ - widthGain_) * smoothCoeff_;
        fbGain_    += (fbGainTarget_ - fbGain_)   * smoothCoeff_;
        enableGain_ += (enableTarget - enableGain_) * enableCoeff_;

        const float inL = left[n];
        const float inR = right != nullptr ? right[n] : inL;
        const float in[2] = { inL, inR };

        float wet[2] { 0.0f, 0.0f };
        float firstTap[2] { 0.0f, 0.0f };

        for (int c = 0; c < 2; ++c)
        {
            float sum = 0.0f;

            for (int t = 0; t < kNumTaps; ++t)
            {
                tapDelay_[c][t] += (tapTarget_[c][t] - tapDelay_[c][t]) * delaySmooth_;
                const float tap = line_[c].readCubic (tapDelay_[c][t]);

                if (t == 0)
                    firstTap[c] = tap;

                sum += tap;
            }

            wet[c] = sum * tapGain;
        }

        for (int c = 0; c < 2; ++c)
        {
            const int other = 1 - c;

            float fb = firstTap[c] * (1.0f - crossFeed_) + firstTap[other] * crossFeed_;

            // One-pole damping, then a soft bound: the loop can never run away.
            fbState_[c] += (fb - fbState_[c]) * fbCoeff_;
            fbState_[c]  = math::sanitise (fbState_[c]);
            fb = math::fastTanh (fbState_[c] * fbGain_);

            line_[c].write (math::sanitise (in[c] + fb));
        }

        // Widen the wet signal only, so the dry image is never disturbed.
        const float wetMid  = 0.5f * (wet[0] + wet[1]);
        const float wetSide = 0.5f * (wet[0] - wet[1]) * widthGain_;

        const float wetL = wetMid + wetSide;
        const float wetR = wetMid - wetSide;

        const float outL = math::lerp (inL, inL * dryGain_ + wetL * wetGain_, enableGain_);
        const float outR = math::lerp (inR, inR * dryGain_ + wetR * wetGain_, enableGain_);

        if (right != nullptr)
        {
            left[n]  = math::sanitise (outL);
            right[n] = math::sanitise (outR);
        }
        else
        {
            left[n] = math::sanitise (0.5f * (outL + outR));
        }
    }
}

} // namespace ripples
