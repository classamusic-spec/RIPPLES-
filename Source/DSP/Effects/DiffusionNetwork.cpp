#include "DSP/Effects/DiffusionNetwork.h"

namespace ripples
{

namespace
{
    constexpr float kStageMsL[4] { 10.7f, 17.3f, 26.9f, 41.3f };
    constexpr float kStageMsR[4] { 12.1f, 19.7f, 29.3f, 45.1f };

    // Deliberately slow and incommensurate: motion you feel, not motion you hear.
    constexpr float kModHz[4] { 0.071f, 0.103f, 0.137f, 0.179f };
}

namespace
{
    /** One-pole glide with a hard rate limit, in samples of delay per sample.
        The limit bounds how fast the read pointer can move relative to the
        write pointer, which is exactly the pitch warp you hear: with a limit of
        L the playback ratio stays inside [1-L, 1+L]. That is what turns a big
        jump in delay time into a musical tape glide instead of a scream or a
        reversal, and it is why a time change under high feedback cannot click.
    */
    inline float glideTowards (float current, float target, float coeff, float maxStep) noexcept
    {
        const float step = math::clamp ((target - current) * coeff, -maxStep, maxStep);
        return current + step;
    }
}

void DiffusionNetwork::prepare (double sampleRate, int maxBlockSize)
{
    juce::ignoreUnused (maxBlockSize);   // processed sample-by-sample, in place

    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;

    const float fs = (float) sampleRate_;
    const int needed = (int) (kMaxStageMs * kMaxSizeScale * 0.001f * fs) + 64;

    for (int c = 0; c < 2; ++c)
    {
        for (int s = 0; s < kNumStages; ++s)
        {
            line_[c][s].allocate (needed);
            const float ms = (c == 0) ? kStageMsL[s] : kStageMsR[s];
            baseSamples_[c][s] = ms * 0.001f * fs;
        }
    }

    for (int s = 0; s < kNumStages; ++s)
        modInc_[s] = kModHz[s] / fs;

    modDepthSamples_ = 0.00022f * fs;    // ~0.22 ms

    smoothCoeff_ = math::onePoleCoeff (dsp::kSmoothingSeconds, sampleRate_);
    delaySmooth_ = math::onePoleCoeff (0.06f, sampleRate_);
    enableCoeff_ = math::onePoleCoeff (dsp::kSmoothingSeconds, sampleRate_);

    setParams (params_);   // before reset(), so every smoothed value starts exact
    reset();
}

void DiffusionNetwork::reset() noexcept
{
    for (int c = 0; c < 2; ++c)
    {
        for (int s = 0; s < kNumStages; ++s)
        {
            line_[c][s].clear();
            dampState_[c][s] = 0.0f;
        }
    }

    for (int s = 0; s < kNumStages; ++s)
        modPhase_[s] = 0.13f * (float) s;

    controlCounter_ = 0;
    enableGain_     = params_.enabled ? 1.0f : 0.0f;

    updateControlRate();

    for (int c = 0; c < 2; ++c)
        for (int s = 0; s < kNumStages; ++s)
            delay_[c][s] = delayTarget_[c][s];

    sizeScale_ = sizeTarget_;
    apGain_    = apGainTarget_;
    dryGain_   = dryTarget_;
    wetGain_   = wetTarget_;
}

void DiffusionNetwork::setParams (const Params& p) noexcept
{
    params_         = p;
    params_.amount  = math::clamp (p.amount,  0.0f, 1.0f);
    params_.size    = math::clamp (p.size,    0.0f, 1.0f);
    params_.damping = math::clamp (p.damping, 0.0f, 1.0f);
    params_.mix     = math::clamp (p.mix,     0.0f, 1.0f);

    sizeTarget_   = 0.25f * std::pow (8.0f, params_.size);          // 0.25x .. 2.0x
    apGainTarget_ = 0.30f + params_.amount * 0.42f;                 // 0.30 .. 0.72

    const float fc  = math::normToFreq (1.0f - params_.damping, 900.0f, 19000.0f);
    const float nyq = 0.45f * (float) sampleRate_;
    dampCoeff_ = 1.0f - std::exp (-math::twoPi * std::min (fc, nyq) / (float) sampleRate_);

    math::equalPowerMix (params_.mix, dryTarget_, wetTarget_);
}

void DiffusionNetwork::updateControlRate() noexcept
{
    const float step = (float) kControlInterval;

    for (int s = 0; s < kNumStages; ++s)
        modPhase_[s] = math::wrapPhase (modPhase_[s] + modInc_[s] * step);

    for (int c = 0; c < 2; ++c)
    {
        for (int s = 0; s < kNumStages; ++s)
        {
            const float ph  = math::wrapPhase (modPhase_[s] + (c == 1 ? 0.37f : 0.0f));
            const float lfo = std::sin (math::twoPi * ph);

            delayTarget_[c][s] = baseSamples_[c][s] * sizeTarget_ + modDepthSamples_ * lfo;
        }
    }
}

void DiffusionNetwork::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0 || line_[0][0].size == 0)
        return;

    const float enableTarget = params_.enabled ? 1.0f : 0.0f;

    if (enableTarget <= 0.0f && enableGain_ <= 1.0e-5f)
    {
        enableGain_ = 0.0f;
        return;
    }

    float* left  = buffer.getWritePointer (0);
    float* right = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int n = 0; n < numSamples; ++n)
    {
        if (controlCounter_ <= 0)
        {
            updateControlRate();
            controlCounter_ = kControlInterval;
        }
        --controlCounter_;

        sizeScale_ += (sizeTarget_ - sizeScale_) * smoothCoeff_;
        apGain_    += (apGainTarget_ - apGain_) * smoothCoeff_;
        dryGain_   += (dryTarget_ - dryGain_)   * smoothCoeff_;
        wetGain_   += (wetTarget_ - wetGain_)   * smoothCoeff_;
        enableGain_ += (enableTarget - enableGain_) * enableCoeff_;

        const float inL = left[n];
        const float inR = right != nullptr ? right[n] : inL;
        const float in[2] { inL, inR };

        float wet[2];

        for (int c = 0; c < 2; ++c)
        {
            float v = in[c];

            for (int s = 0; s < kNumStages; ++s)
            {
                delay_[c][s] = glideTowards (delay_[c][s], delayTarget_[c][s],
                                             delaySmooth_, kStageSlew);

                float d = line_[c][s].readCubic (delay_[c][s]);

                dampState_[c][s] += (d - dampState_[c][s]) * dampCoeff_;
                dampState_[c][s]  = math::sanitise (dampState_[c][s]);
                d = dampState_[c][s];

                const float w = math::sanitise (v + apGain_ * d);
                line_[c][s].write (w);

                v = d - apGain_ * w;
            }

            wet[c] = math::sanitise (v);
        }

        const float outL = math::lerp (inL, inL * dryGain_ + wet[0] * wetGain_, enableGain_);
        const float outR = math::lerp (inR, inR * dryGain_ + wet[1] * wetGain_, enableGain_);

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
