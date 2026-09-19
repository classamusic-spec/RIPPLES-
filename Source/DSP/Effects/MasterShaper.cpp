#include "DSP/Effects/MasterShaper.h"

namespace ripples
{

namespace
{
    inline float coeffFor (float seconds, double sampleRate) noexcept
    {
        if (seconds <= 0.0f)
            return 1.0f;

        return 1.0f - std::exp (-1.0f / (seconds * (float) sampleRate));
    }
}

void MasterShaper::prepare (double sampleRate, int maxBlockSize)
{
    juce::ignoreUnused (maxBlockSize);   // processed sample-by-sample, in place

    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;

    const float fs  = (float) sampleRate_;
    const float nyq = 0.49f * fs;

    const float gLow  = std::tan (math::pi * std::min (kLowShelfHz,  nyq) / fs);
    const float gHigh = std::tan (math::pi * std::min (kHighShelfHz, nyq) / fs);

    lowG1_  = gLow  / (1.0f + gLow);
    highG1_ = gHigh / (1.0f + gHigh);

    const float gMid = std::tan (math::pi * std::min (kMidHz, nyq) / fs);
    midK_  = 1.0f / kMidQ;
    midA1_ = 1.0f / (1.0f + gMid * (gMid + midK_));
    midA2_ = gMid * midA1_;
    midA3_ = gMid * midA2_;

    smoothCoeff_ = coeffFor (dsp::kSmoothingSeconds, sampleRate_);

    envRelCoeff_ = std::exp (-1.0f / (0.150f * fs));   // detector release
    attCoeff_    = coeffFor (0.0010f, sampleRate_);    // 1 ms
    relCoeff_    = coeffFor (0.2500f, sampleRate_);    // 250 ms
    glideCoeff_  = coeffFor (0.0030f, sampleRate_);    // S-curve smoother

    setParams (params_);   // before reset(), so every smoothed value starts exact
    reset();
}

void MasterShaper::reset() noexcept
{
    for (int c = 0; c < 2; ++c)
    {
        lowShelf_[c].clear();
        highShelf_[c].clear();
        midBand_[c].clear();
    }

    limEnv_        = 0.0f;
    limGain_       = 1.0f;
    limGainSmooth_ = 1.0f;

    lowGain_    = lowTarget_;
    midGain_    = midTarget_;
    highGain_   = highTarget_;
    driveMix_   = driveMixTarget_;
    driveGain_  = driveGainTarget_;
    driveMakeup_ = driveMakeupTarget_;
    outGain_    = outTarget_;
    ceiling_    = ceilingTarget_;

    peakL_.store (0.0f, std::memory_order_relaxed);
    peakR_.store (0.0f, std::memory_order_relaxed);
}

void MasterShaper::setParams (const Params& p) noexcept
{
    params_            = p;
    params_.lowGainDb  = math::clamp (p.lowGainDb,  -18.0f, 18.0f);
    params_.midGainDb  = math::clamp (p.midGainDb,  -18.0f, 18.0f);
    params_.highGainDb = math::clamp (p.highGainDb, -18.0f, 18.0f);
    params_.drive      = math::clamp (p.drive,        0.0f,  1.0f);
    params_.ceilingDb  = math::clamp (p.ceilingDb,  -24.0f,  0.0f);
    params_.outputDb   = math::clamp (p.outputDb,   -60.0f, 12.0f);

    lowTarget_  = math::decibelsToGain (params_.lowGainDb);
    midTarget_  = math::decibelsToGain (params_.midGainDb);
    highTarget_ = math::decibelsToGain (params_.highGainDb);

    const float d = 1.0f + params_.drive * 7.0f;
    driveGainTarget_   = d;
    driveMakeupTarget_ = std::pow (d, -0.7f);
    driveMixTarget_    = params_.drive;

    outTarget_     = math::decibelsToGain (params_.outputDb);
    ceilingTarget_ = math::decibelsToGain (params_.ceilingDb);
}

void MasterShaper::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    float* left  = buffer.getWritePointer (0);
    float* right = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    float blockPeakL = 0.0f;
    float blockPeakR = 0.0f;

    for (int n = 0; n < numSamples; ++n)
    {
        lowGain_     += (lowTarget_  - lowGain_)  * smoothCoeff_;
        midGain_     += (midTarget_  - midGain_)  * smoothCoeff_;
        highGain_    += (highTarget_ - highGain_) * smoothCoeff_;
        driveMix_    += (driveMixTarget_ - driveMix_) * smoothCoeff_;
        driveGain_   += (driveGainTarget_ - driveGain_) * smoothCoeff_;
        driveMakeup_ += (driveMakeupTarget_ - driveMakeup_) * smoothCoeff_;
        outGain_     += (outTarget_ - outGain_) * smoothCoeff_;
        ceiling_     += (ceilingTarget_ - ceiling_) * smoothCoeff_;

        float s[2];
        s[0] = left[n];
        s[1] = right != nullptr ? right[n] : left[n];

        const int numToProcess = right != nullptr ? 2 : 1;

        for (int c = 0; c < numToProcess; ++c)
        {
            float x = s[c];

            // --- three band EQ ------------------------------------------------
            float lp = 0.0f, hp = 0.0f;

            lowShelf_[c].process (x, lowG1_, lp, hp);
            x = lp * lowGain_ + hp;

            const float bp = midBand_[c].bandPass (x, midA1_, midA2_, midA3_);
            x += (midGain_ - 1.0f) * midK_ * bp;

            highShelf_[c].process (x, highG1_, lp, hp);
            x = lp + hp * highGain_;

            // --- drive ---------------------------------------------------------
            if (driveMix_ > 1.0e-4f)
            {
                const float sat = math::fastTanh (x * driveGain_) * driveMakeup_;
                x = math::lerp (x, sat, driveMix_);
            }

            // --- output gain, pre-limiter --------------------------------------
            s[c] = math::sanitise (x * outGain_);
        }

        if (numToProcess == 1)
            s[1] = s[0];

        // --- stereo-linked safety limiter --------------------------------------
        const float peak = std::max (std::fabs (s[0]), std::fabs (s[1]));

        limEnv_ = peak > limEnv_ ? peak
                                 : peak + (limEnv_ - peak) * envRelCoeff_;
        limEnv_ = math::sanitise (limEnv_);

        const float target = limEnv_ > ceiling_ ? ceiling_ / std::max (limEnv_, 1.0e-9f)
                                                : 1.0f;

        limGain_ += (target - limGain_) * (target < limGain_ ? attCoeff_ : relCoeff_);
        limGainSmooth_ += (limGain_ - limGainSmooth_) * glideCoeff_;

        const float g = math::clamp (limGainSmooth_, 0.0f, 1.0f);

        // Soft knee: exactly linear below 85% of the ceiling, asymptotic above,
        // so the ceiling is a hard guarantee even without look-ahead. Clamping
        // against min(smoothed, target) means a ceiling the user drags DOWN
        // binds from the very first sample, while one dragged up glides.
        const float ceilNow = std::min (ceiling_, ceilingTarget_);
        const float knee = kKneeStart * ceilNow;
        const float span = std::max (ceilNow - knee, 1.0e-6f);

        for (int c = 0; c < 2; ++c)
        {
            float y = s[c] * g;
            const float a = std::fabs (y);

            if (a > knee)
            {
                const float shaped = knee + span * math::fastTanh ((a - knee) / span) * 0.99999f;
                y = y < 0.0f ? -shaped : shaped;
            }

            s[c] = math::sanitise (y);
        }

        blockPeakL = std::max (blockPeakL, std::fabs (s[0]));
        blockPeakR = std::max (blockPeakR, std::fabs (s[1]));

        if (right != nullptr)
        {
            left[n]  = s[0];
            right[n] = s[1];
        }
        else
        {
            left[n] = s[0];
        }
    }

    // Decaying peak hold for the UI meter (post-limiter, as specified).
    const float decay = std::exp (-(float) numSamples / (0.25f * (float) sampleRate_));

    const float heldL = peakL_.load (std::memory_order_relaxed) * decay;
    const float heldR = peakR_.load (std::memory_order_relaxed) * decay;

    peakL_.store (math::sanitise (std::max (blockPeakL, heldL)), std::memory_order_relaxed);
    peakR_.store (math::sanitise (std::max (blockPeakR, heldR)), std::memory_order_relaxed);
}

} // namespace ripples
