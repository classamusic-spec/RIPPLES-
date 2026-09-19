#include "DSP/Effects/StereoCurrent.h"

namespace ripples
{

void StereoCurrent::prepare (double sampleRate, int maxBlockSize)
{
    juce::ignoreUnused (maxBlockSize);   // sample-accurate, in-place: no scratch needed

    sampleRate_  = sampleRate > 0.0 ? sampleRate : 44100.0;
    smoothCoeff_ = math::onePoleCoeff (dsp::kSlowSmoothingSec, sampleRate_);
    enableCoeff_ = math::onePoleCoeff (dsp::kSmoothingSeconds, sampleRate_);

    setParams (params_);   // before reset(), so every smoothed value starts exact
    reset();
}

void StereoCurrent::reset() noexcept
{
    for (auto& a : ap_)
        a.clear();

    // Stagger the starting phases so the very first block already has motion.
    phase_[0] = 0.0f;
    phase_[1] = 0.37f;
    phase_[2] = 0.71f;

    controlCounter_ = 0;
    enableGain_     = params_.enabled ? 1.0f : 0.0f;

    updateControlRate();

    sideGain_  = sideGainTarget_;
    panDrift_  = panDriftTarget_;
    mistDepth_ = mistDepthTarget_;

    for (int i = 0; i < kNumAllPass; ++i)
        apCoeff_[i] = apTarget_[i];
}

void StereoCurrent::setParams (const Params& p) noexcept
{
    params_         = p;
    params_.amount  = math::clamp (p.amount, 0.0f, 1.0f);
    params_.width   = math::clamp (p.width,  0.0f, 1.0f);
    params_.rate    = math::clamp (p.rate,   0.002f, 4.0f);

    const float base = params_.rate / (float) sampleRate_;
    phaseInc_[0] = base;
    phaseInc_[1] = base * 0.733f;
    phaseInc_[2] = base * 1.317f;
}

void StereoCurrent::updateControlRate() noexcept
{
    const float step = (float) kControlInterval;

    for (int i = 0; i < 3; ++i)
        phase_[i] = math::wrapPhase (phase_[i] + phaseInc_[i] * step);

    const float s0 = std::sin (math::twoPi * phase_[0]);
    const float s1 = std::sin (math::twoPi * phase_[1]);
    const float s2 = std::sin (math::twoPi * phase_[2]);

    const float amount = params_.amount;

    // Two incommensurate sines never repeat exactly -> an organic wander.
    const float pan  = 0.62f * s0 + 0.38f * s1;
    const float wid  = 0.55f * s1 + 0.45f * s2;
    const float mist = 0.60f * s2 + 0.40f * s0;

    // Static width: 0.5 == unity side gain, 1.0 == double width.
    const float baseWidth = params_.width * 2.0f;

    sideGainTarget_  = baseWidth * (1.0f + 0.45f * amount * wid);
    panDriftTarget_  = 0.42f * amount * pan;
    mistDepthTarget_ = 0.45f * amount * mist;

    // Slowly sweeping allpass corners scatter the derived side content so the
    // image wanders spectrally instead of just left/right.
    const float nyquist = 0.49f * (float) sampleRate_;

    for (int i = 0; i < kNumAllPass; ++i)
    {
        const float lfo = (i & 1) == 0 ? s2 : s1;
        const float hz  = math::clamp (apBaseHz_[i] * (1.0f + 0.55f * amount * lfo),
                                       20.0f, nyquist);
        const float g   = std::tan (math::pi * hz / (float) sampleRate_);
        apTarget_[i]    = math::clamp ((1.0f - g) / (1.0f + g), -0.995f, 0.995f);
    }
}

void StereoCurrent::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    const float enableTarget = params_.enabled ? 1.0f : 0.0f;

    if (enableTarget <= 0.0f && enableGain_ <= 1.0e-5f)
    {
        enableGain_ = 0.0f;         // fully bypassed: leave the buffer untouched
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

        sideGain_  += (sideGainTarget_  - sideGain_)  * smoothCoeff_;
        panDrift_  += (panDriftTarget_  - panDrift_)  * smoothCoeff_;
        mistDepth_ += (mistDepthTarget_ - mistDepth_) * smoothCoeff_;
        enableGain_ += (enableTarget - enableGain_) * enableCoeff_;

        for (int i = 0; i < kNumAllPass; ++i)
            apCoeff_[i] += (apTarget_[i] - apCoeff_[i]) * smoothCoeff_;

        const float inL = left[n];
        const float inR = right != nullptr ? right[n] : inL;

        const float mid  = 0.5f * (inL + inR);
        const float side = 0.5f * (inL - inR);

        float scattered = mid;
        for (int i = 0; i < kNumAllPass; ++i)
            scattered = ap_[i].process (scattered, apCoeff_[i]);

        // Everything below lives in SIDE only -> L+R is bit-for-bit the input.
        const float wetSide = side * sideGain_
                            + mid * panDrift_
                            + scattered * mistDepth_;

        const float outL = math::lerp (inL, mid + wetSide, enableGain_);
        const float outR = math::lerp (inR, mid - wetSide, enableGain_);

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
