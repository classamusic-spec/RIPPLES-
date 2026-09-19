#include "DSP/Effects/LiquidDelay.h"

namespace ripples
{

namespace
{
    // Mean allpass delays in ms. Different per channel so the two smears
    // decorrelate instead of sitting on top of each other.
    constexpr float kApMsL[3] { 2.7f, 4.9f,  7.3f };
    constexpr float kApMsR[3] { 3.1f, 5.3f,  8.1f };

    /** Linear below -3 dBFS, asymptotic to 1.0 above: the loop cannot diverge
        but ordinary programme material passes untouched. */
    inline float loopBound (float x) noexcept
    {
        const float a = std::fabs (x);
        if (a <= 0.7f)
            return x;

        const float y = 0.7f + 0.3f * math::fastTanh ((a - 0.7f) * 3.3333333f);
        return x < 0.0f ? -y : y;
    }
}

void LiquidDelay::prepare (double sampleRate, int maxBlockSize)
{
    juce::ignoreUnused (maxBlockSize);   // processed sample-by-sample, in place

    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;

    const float fs = (float) sampleRate_;
    const int needed = (int) ((dsp::kMaxDelaySeconds + 0.1f) * fs) + 8;

    line_[0].allocate (needed);
    line_[1].allocate (needed);

    for (int c = 0; c < 2; ++c)
    {
        float comp = 0.0f;

        for (int i = 0; i < kNumAllPass; ++i)
        {
            const float ms = (c == 0) ? kApMsL[i] : kApMsR[i];
            const int   n  = std::max (1, (int) (ms * 0.001f * fs));
            ap_[c][i].allocate (n);
            comp += (float) n;
        }

        apCompSamples_[c] = comp;
    }

    smoothCoeff_ = math::onePoleCoeff (dsp::kSmoothingSeconds, sampleRate_);
    enableCoeff_ = math::onePoleCoeff (dsp::kSmoothingSeconds, sampleRate_);
    timeCoeff_   = math::onePoleCoeff (0.20f, sampleRate_);   // tape-style glide

    const float hpHz = 35.0f;
    hpCoeff_ = 1.0f - std::exp (-math::twoPi * hpHz / fs);

    reset();
    setParams (params_);
}

void LiquidDelay::reset() noexcept
{
    line_[0].clear();
    line_[1].clear();

    for (int c = 0; c < 2; ++c)
    {
        for (int i = 0; i < kNumAllPass; ++i)
            ap_[c][i].clear();

        dampState_[c] = 0.0f;
        hpState_[c]   = 0.0f;
        modOffset_[c] = 0.0f;
        noiseValue_[c] = 0.0f;
        noiseTarget_[c] = 0.0f;
    }

    modPhase_[0]  = 0.0f;   modPhase_[1]  = 0.27f;
    modPhase2_[0] = 0.41f;  modPhase2_[1] = 0.83f;

    controlCounter_ = 0;
    enableGain_     = params_.enabled ? 1.0f : 0.0f;

    const float fs = (float) sampleRate_;
    const float t  = math::clamp (params_.timeSeconds, kMinTimeSeconds, dsp::kMaxDelaySeconds);

    for (int c = 0; c < 2; ++c)
    {
        const float chScale = (c == 1) ? (1.0f + 0.12f * math::clamp (params_.spread, 0.0f, 1.0f))
                                       : 1.0f;
        timeSmoothed_[c] = std::max (2.0f, t * chScale * fs - apCompSamples_[c]);
    }

    updateControlRate();

    dryGain_ = dryTarget_;
    wetGain_ = wetTarget_;
    fbGain_  = fbGainTarget_;
    apGain_  = apGainTarget_;
    rotCos_  = rotCosTarget_;
    rotSin_  = rotSinTarget_;
    modDepth_ = modDepthTarget_;
}

void LiquidDelay::setParams (const Params& p) noexcept
{
    params_             = p;
    params_.timeSeconds = math::clamp (p.timeSeconds, kMinTimeSeconds, dsp::kMaxDelaySeconds);
    params_.feedback    = math::clamp (p.feedback,  0.0f, 1.0f);
    params_.motion      = math::clamp (p.motion,    0.0f, 1.0f);
    params_.spread      = math::clamp (p.spread,    0.0f, 1.0f);
    params_.damping     = math::clamp (p.damping,   0.0f, 1.0f);
    params_.diffusion   = math::clamp (p.diffusion, 0.0f, 1.0f);
    params_.mix         = math::clamp (p.mix,       0.0f, 1.0f);

    timeTarget_   = params_.timeSeconds;
    fbGainTarget_ = params_.feedback * kMaxFeedback;
    apGainTarget_ = params_.diffusion * 0.62f;

    const float theta = params_.spread * math::halfPi;
    rotCosTarget_ = std::cos (theta);
    rotSinTarget_ = std::sin (theta);

    // Damping: 18 kHz wide open down to 400 Hz fully damped.
    const float fc = math::normToFreq (1.0f - params_.damping, 400.0f, 18000.0f);
    const float nyq = 0.45f * (float) sampleRate_;
    dampCoeff_ = 1.0f - std::exp (-math::twoPi * std::min (fc, nyq) / (float) sampleRate_);

    // Up to ~4 ms of deviation: a slow, singable flutter rather than vibrato.
    modDepthTarget_ = params_.motion * (0.0006f + 0.0034f * params_.motion)
                    * (float) sampleRate_;

    const float rateHz = 0.21f + params_.motion * 0.45f;
    modInc_  = rateHz / (float) sampleRate_;
    modInc2_ = rateHz * 2.37f / (float) sampleRate_;

    math::equalPowerMix (params_.mix, dryTarget_, wetTarget_);
}

void LiquidDelay::updateControlRate() noexcept
{
    const float step = (float) kControlInterval;

    for (int c = 0; c < 2; ++c)
    {
        modPhase_[c]  = math::wrapPhase (modPhase_[c]  + modInc_  * step);
        modPhase2_[c] = math::wrapPhase (modPhase2_[c] + modInc2_ * step);

        // Correlated random walk -> organic drift on top of the two sines.
        noiseTarget_[c] += (rng_.nextBipolar() - noiseTarget_[c]) * 0.04f;
        noiseValue_[c]  += (noiseTarget_[c] - noiseValue_[c]) * 0.01f;

        const float lfo = 0.60f * std::sin (math::twoPi * modPhase_[c])
                        + 0.25f * std::sin (math::twoPi * modPhase2_[c])
                        + 0.15f * noiseValue_[c];

        modOffset_[c] = lfo;
    }
}

void LiquidDelay::process (juce::AudioBuffer<float>& buffer) noexcept
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

    const float fs = (float) sampleRate_;
    const float spread = params_.spread;

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
        fbGain_    += (fbGainTarget_ - fbGain_)   * smoothCoeff_;
        apGain_    += (apGainTarget_ - apGain_)   * smoothCoeff_;
        rotCos_    += (rotCosTarget_ - rotCos_)   * smoothCoeff_;
        rotSin_    += (rotSinTarget_ - rotSin_)   * smoothCoeff_;
        modDepth_  += (modDepthTarget_ - modDepth_) * smoothCoeff_;
        enableGain_ += (enableTarget - enableGain_) * enableCoeff_;

        const float inL = left[n];
        const float inR = right != nullptr ? right[n] : inL;
        const float in[2] { inL, inR };

        float y[2];

        for (int c = 0; c < 2; ++c)
        {
            const float chScale = (c == 1) ? (1.0f + 0.12f * spread) : 1.0f;
            const float target  = std::max (2.0f, timeTarget_ * chScale * fs - apCompSamples_[c]);

            timeSmoothed_[c] += (target - timeSmoothed_[c]) * timeCoeff_;

            const float readDelay = timeSmoothed_[c] + modDepth_ * modOffset_[c];

            float v = line_[c].readCubic (readDelay);

            for (int i = 0; i < kNumAllPass; ++i)
                v = ap_[c][i].process (v, apGain_);

            // Damping lowpass, then a highpass so LF can never accumulate.
            dampState_[c] += (v - dampState_[c]) * dampCoeff_;
            dampState_[c]  = math::sanitise (dampState_[c]);
            v = dampState_[c];

            hpState_[c] += (v - hpState_[c]) * hpCoeff_;
            hpState_[c]  = math::sanitise (hpState_[c]);
            v -= hpState_[c];

            y[c] = math::sanitise (v);
        }

        // Rotation matrix: orthogonal at every spread, so the feedback can
        // never gain energy and never collapses the two channels together.
        const float fbL = fbGain_ * ( rotCos_ * y[0] + rotSin_ * y[1]);
        const float fbR = fbGain_ * (-rotSin_ * y[0] + rotCos_ * y[1]);

        line_[0].write (math::sanitise (loopBound (in[0] + fbL)));
        line_[1].write (math::sanitise (loopBound (in[1] + fbR)));

        const float outL = math::lerp (inL, inL * dryGain_ + y[0] * wetGain_, enableGain_);
        const float outR = math::lerp (inR, inR * dryGain_ + y[1] * wetGain_, enableGain_);

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
