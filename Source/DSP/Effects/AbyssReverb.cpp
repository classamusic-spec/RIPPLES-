#include "DSP/Effects/AbyssReverb.h"

namespace ripples
{

namespace
{
    // Mutually prime-ish line lengths in ms: no common factor, so no flutter.
    constexpr float kLineMs[8] { 23.13f, 29.71f, 37.39f, 43.91f,
                                 53.29f, 61.71f, 71.33f, 79.87f };

    // Slow, incommensurate detune rates. Nothing here is fast enough to hear
    // as vibrato; it exists only to stop modes from standing still.
    constexpr float kModHz[8] { 0.083f, 0.117f, 0.141f, 0.173f,
                                0.211f, 0.239f, 0.271f, 0.311f };

    constexpr float kInputApMsL[4] { 4.77f, 3.59f, 12.73f, 9.31f };
    constexpr float kInputApMsR[4] { 5.31f, 3.97f, 13.61f, 10.17f };
    constexpr float kInputApG  [4] { 0.75f, 0.75f, 0.625f, 0.625f };

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

    /** Orthonormal 8-point Hadamard (fast Walsh transform + 1/sqrt(8)). */
    inline void hadamard8 (float* v) noexcept
    {
        for (int s = 1; s < 8; s <<= 1)
        {
            for (int i = 0; i < 8; i += (s << 1))
            {
                for (int j = i; j < i + s; ++j)
                {
                    const float a = v[j];
                    const float b = v[j + s];
                    v[j]     = a + b;
                    v[j + s] = a - b;
                }
            }
        }

        constexpr float norm = 0.35355339059327373f;   // 1 / sqrt(8)

        for (int i = 0; i < 8; ++i)
            v[i] *= norm;
    }
}

void AbyssReverb::prepare (double sampleRate, int maxBlockSize)
{
    juce::ignoreUnused (maxBlockSize);   // processed sample-by-sample, in place

    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;

    const float fs = (float) sampleRate_;

    const int predelayNeeded = (int) (dsp::kMaxPredelaySeconds * fs) + 64;
    predelay_[0].allocate (predelayNeeded);
    predelay_[1].allocate (predelayNeeded);

    for (int c = 0; c < 2; ++c)
    {
        for (int i = 0; i < kNumInputAllPass; ++i)
        {
            const float ms = (c == 0) ? kInputApMsL[i] : kInputApMsR[i];
            inputAp_[c][i].allocate (std::max (1, (int) (ms * 0.001f * fs)));
        }
    }

    const int lineNeeded = (int) ((kMaxLineMs * kMaxSizeScale + kMaxModMs + 4.0f) * 0.001f * fs) + 64;

    for (int i = 0; i < kNumLines; ++i)
    {
        line_[i].allocate (lineNeeded);
        baseSamples_[i] = kLineMs[i] * 0.001f * fs;
        modInc_[i]      = kModHz[i] / fs;
    }

    smoothCoeff_ = math::onePoleCoeff (dsp::kSmoothingSeconds, sampleRate_);
    delaySmooth_ = math::onePoleCoeff (0.03f, sampleRate_);
    enableCoeff_ = math::onePoleCoeff (dsp::kSmoothingSeconds, sampleRate_);

    setParams (params_);   // before reset(), so every smoothed value starts exact
    reset();
}

void AbyssReverb::reset() noexcept
{
    predelay_[0].clear();
    predelay_[1].clear();

    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < kNumInputAllPass; ++i)
            inputAp_[c][i].clear();

    for (int i = 0; i < kNumLines; ++i)
    {
        line_[i].clear();
        dampState_[i]   = 0.0f;
        lowCutState_[i] = 0.0f;
        modPhase_[i]    = 0.11f * (float) i;
    }

    controlCounter_ = 0;
    enableGain_     = params_.enabled ? 1.0f : 0.0f;

    updateControlRate();

    for (int i = 0; i < kNumLines; ++i)
    {
        delay_[i]    = delayTarget_[i];
        lineGain_[i] = lineGainTarget_[i];
    }

    predelaySamples_ = predelayTarget_;
    sizeScale_       = sizeTarget_;
    inGain_          = inGainTarget_;
    dryGain_         = dryTarget_;
    wetGain_         = wetTarget_;
}

void AbyssReverb::setParams (const Params& p) noexcept
{
    params_            = p;
    params_.size       = math::clamp (p.size,       0.0f, 1.0f);
    params_.decay      = math::clamp (p.decay,      0.0f, 1.0f);
    params_.damping    = math::clamp (p.damping,    0.0f, 1.0f);
    params_.modulation = math::clamp (p.modulation, 0.0f, 1.0f);
    params_.mix        = math::clamp (p.mix,        0.0f, 1.0f);
    params_.predelayMs = math::clamp (p.predelayMs, 0.0f,
                                      dsp::kMaxPredelaySeconds * 1000.0f);
    params_.lowCutHz   = math::clamp (p.lowCutHz,   20.0f,   1000.0f);
    params_.highCutHz  = math::clamp (p.highCutHz,  1000.0f, 20000.0f);

    const float fs = (float) sampleRate_;

    sizeTarget_      = 0.35f + params_.size * 1.65f;                  // 0.35x .. 2.0x
    rt60Seconds_     = 0.4f * std::pow (50.0f, params_.decay);        // 0.4 s .. 20 s
    predelayTarget_  = params_.predelayMs * 0.001f * fs;
    modDepthTarget_  = params_.modulation * kMaxModMs * 0.001f * fs;

    // One in-loop lowpass serves both DAMPING and the HIGH CUT.
    const float dampHz = math::normToFreq (1.0f - params_.damping, 700.0f, 19000.0f);
    const float lpHz   = std::min (dampHz, params_.highCutHz);
    const float nyq    = 0.45f * fs;

    dampCoeff_   = 1.0f - std::exp (-math::twoPi * std::min (lpHz, nyq) / fs);
    lowCutCoeff_ = 1.0f - std::exp (-math::twoPi * std::min (params_.lowCutHz, nyq) / fs);

    math::equalPowerMix (params_.mix, dryTarget_, wetTarget_);
}

void AbyssReverb::updateControlRate() noexcept
{
    const float step = (float) kControlInterval;
    const float fs   = (float) sampleRate_;

    float gainSum = 0.0f;

    for (int i = 0; i < kNumLines; ++i)
    {
        modPhase_[i] = math::wrapPhase (modPhase_[i] + modInc_[i] * step);

        const float lfo    = std::sin (math::twoPi * modPhase_[i]);
        const float length = baseSamples_[i] * sizeTarget_;

        delayTarget_[i] = length + modDepthTarget_ * lfo;

        // Per-line gain for the requested RT60: g = 10^(-3 * L / (RT60 * fs)).
        const float g = std::exp (-6.907755279f * length / (rt60Seconds_ * fs));
        lineGainTarget_[i] = math::clamp (g, 0.0f, kMaxLineGain);

        gainSum += lineGainTarget_[i];
    }

    // Energy normalisation: a longer decay integrates more input, so scale the
    // injection down by sqrt(1 - g^2). Wet level then stays roughly constant
    // as DECAY is swept, and sustained input can never pile up without bound.
    const float gAvg = gainSum * (1.0f / (float) kNumLines);
    const float norm = std::sqrt (std::max (1.0e-4f, 1.0f - gAvg * gAvg));

    inGainTarget_ = 1.9f * norm;
}

void AbyssReverb::process (juce::AudioBuffer<float>& buffer) noexcept
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

    for (int n = 0; n < numSamples; ++n)
    {
        if (controlCounter_ <= 0)
        {
            updateControlRate();
            controlCounter_ = kControlInterval;
        }
        --controlCounter_;

        predelaySamples_ += (predelayTarget_ - predelaySamples_) * smoothCoeff_;
        sizeScale_       += (sizeTarget_ - sizeScale_) * smoothCoeff_;
        inGain_          += (inGainTarget_ - inGain_)  * smoothCoeff_;
        dryGain_         += (dryTarget_ - dryGain_)    * smoothCoeff_;
        wetGain_         += (wetTarget_ - wetGain_)    * smoothCoeff_;
        enableGain_      += (enableTarget - enableGain_) * enableCoeff_;

        const float inL = left[n];
        const float inR = right != nullptr ? right[n] : inL;

        // ---- predelay -------------------------------------------------------
        predelay_[0].write (math::sanitise (inL));
        predelay_[1].write (math::sanitise (inR));

        float preL = predelay_[0].readCubic (predelaySamples_ + 2.0f);
        float preR = predelay_[1].readCubic (predelaySamples_ + 2.0f);

        // ---- input diffusion ------------------------------------------------
        for (int i = 0; i < kNumInputAllPass; ++i)
        {
            preL = inputAp_[0][i].process (preL, kInputApG[i]);
            preR = inputAp_[1][i].process (preR, kInputApG[i]);
        }

        const float injL = preL * inGain_;
        const float injR = preR * inGain_;

        // ---- read, filter and decay every line ------------------------------
        float v[kNumLines];

        for (int i = 0; i < kNumLines; ++i)
        {
            delay_[i]    = glideTowards (delay_[i], delayTarget_[i], delaySmooth_, kDelaySlew);
            lineGain_[i] += (lineGainTarget_[i] - lineGain_[i]) * smoothCoeff_;

            float y = line_[i].readCubic (delay_[i]);

            // Damping / high cut.
            dampState_[i] += (y - dampState_[i]) * dampCoeff_;
            dampState_[i]  = math::sanitise (dampState_[i]);
            y = dampState_[i];

            // Low cut: subtract the lowpassed part, so LF can never accumulate.
            lowCutState_[i] += (y - lowCutState_[i]) * lowCutCoeff_;
            lowCutState_[i]  = math::sanitise (lowCutState_[i]);
            y -= lowCutState_[i];

            v[i] = math::sanitise (y * lineGain_[i]);
        }

        // Outputs are taken pre-mix, alternating sign for L/R decorrelation.
        const float wetL = 0.5f * (v[0] - v[2] + v[4] - v[6]);
        const float wetR = 0.5f * (v[1] - v[3] + v[5] - v[7]);

        // ---- orthonormal recirculation --------------------------------------
        hadamard8 (v);

        for (int i = 0; i < kNumLines; ++i)
        {
            const float inject = ((i & 1) == 0 ? injL : injR) * (((i & 2) == 0) ? 1.0f : -1.0f);
            const float w = math::clamp (math::sanitise (v[i] + inject),
                                         -kStateCeiling, kStateCeiling);
            line_[i].write (w);
        }

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
