#include "DSP/Oscillators/BandLimitedOscillator.h"
#include "Utilities/MathUtils.h"

#include <cmath>

namespace ripples
{

namespace
{
    /** How often the additive engine recomputes its partial gains. The gains
        are then glided towards those targets every sample, so nothing steps. */
    constexpr int kControlInterval = 32;

    /** Peak trim per additive wave, chosen so that every shape stays inside
        +/-1 without squashing the level. */
    constexpr float kSineMakeup   = 1.00f;
    constexpr float kHollowMakeup = 1.28f;
    constexpr float kGlassMakeup  = 1.45f;
    constexpr float kWaterMakeup  = 1.20f;

    //==========================================================================
    /** Shared, read-only sine table. Built once at static-init time, so no
        oscillator allocates and the audio thread only ever reads it. */
    struct SineTable
    {
        static constexpr int kSize = 8192;
        static constexpr int kMask = kSize - 1;
        float t[kSize + 1];

        SineTable() noexcept
        {
            for (int i = 0; i <= kSize; ++i)
                t[i] = std::sin (math::twoPi * (float) i / (float) kSize);
        }

        /** Sine of a phase given in cycles. Wraps, so modest negative or
            greater-than-one phases are fine. */
        inline float operator() (float cycles) const noexcept
        {
            const float x  = cycles * (float) kSize;
            const float fl = std::floor (x);
            const float fr = x - fl;
            const int   i  = ((int) fl) & kMask;
            return t[i] + (t[i + 1] - t[i]) * fr;
        }
    };

    const SineTable sineTable;

    /** log(k) for every harmonic, so the partial tilt needs an exp() and not a
        pow() per partial. */
    struct HarmonicLogs
    {
        float logK[BandLimitedOscillator::kMaxPartials + 2];

        HarmonicLogs() noexcept
        {
            logK[0] = 0.0f;
            for (int k = 1; k < (int) (sizeof (logK) / sizeof (float)); ++k)
                logK[k] = std::log ((float) k);
        }
    };

    const HarmonicLogs harmonicLogs;

    /** Low-discrepancy phase offsets: unison voices never start bunched up
        (a comb-filtered attack) and never sit perfectly evenly spaced
        (which would cancel the fundamental when detune is zero). */
    inline float goldenOffset (int i) noexcept
    {
        return math::wrapPhase (0.61803399f * (float) i);
    }
}

//==============================================================================
void BandLimitedOscillator::prepare (double sampleRate)
{
    sampleRate_     = sampleRate > 0.0 ? sampleRate : 44100.0;
    invSampleRate_  = (float) (1.0 / sampleRate_);
    maxFreq_        = (float) (sampleRate_ * 0.48);
    maxPartialHz_   = (float) (sampleRate_ * 0.45);
    shapeCoeff_     = math::clamp (math::onePoleCoeff (0.004f, sampleRate_), 0.0f, 1.0f);
    gainCoeff_      = math::clamp (math::onePoleCoeff (0.003f, sampleRate_), 0.0f, 1.0f);
    controlSeconds_ = (float) kControlInterval * invSampleRate_;
    controlCounter_ = 0;
    phasesReady_    = false;

    internalRng_.seed (0x5EED0C51u);

    shapeTarget_   = math::clamp (params_.shape, 0.0f, 1.0f);
    shapeSmoothed_ = shapeTarget_;

    for (int k = 0; k < kMaxPartials; ++k)
    {
        gainRe_[k] = gainIm_[k] = 0.0f;
        targetRe_[k] = targetIm_[k] = 0.0f;
        modPhase_[k]   = math::wrapPhase (0.61803399f * (float) (k + 1));
        driftPhase_[k] = math::wrapPhase (0.38196601f * (float) (k + 1) + 0.25f);
    }

    setParams (params_);
    setFrequency (baseFreq_);

    for (auto& uv : voices_)
        uv.phase = uv.startPhase;

    phasesReady_ = true;

    deriveShapeConstants();
    updatePartialTargets();
    snapPartialGains();
}

//==============================================================================
void BandLimitedOscillator::reset (RandomGenerator& rng)
{
    rebuildUnison (true, &rng);

    shapeSmoothed_  = shapeTarget_;
    controlCounter_ = 0;
    phasesReady_    = true;

    deriveShapeConstants();
    updatePartialTargets();
    snapPartialGains();
}

//==============================================================================
void BandLimitedOscillator::setParams (const Params& p) noexcept
{
    const bool layoutChanged = (p.unison != params_.unison)
                            || (p.detune != params_.detune)
                            || (p.stereo != params_.stereo)
                            || (p.startPhase != params_.startPhase);
    const bool waveChanged   = (p.wave != params_.wave);

    params_      = p;
    shapeTarget_ = math::clamp (p.shape, 0.0f, 1.0f);

    wave_ = (p.wave >= OscWave::NumWaves || p.wave < OscWave::Sine) ? OscWave::Saw : p.wave;

    switch (wave_)
    {
        case OscWave::Sine:   usesPartials_ = true;  partialLimit_ = 3;             break;
        case OscWave::Hollow:
        case OscWave::Glass:
        case OscWave::Water:  usesPartials_ = true;  partialLimit_ = kMaxPartials;  break;
        default:              usesPartials_ = false; partialLimit_ = 1;             break;
    }

    if (waveChanged)
    {
        controlCounter_ = 0;
        activePartials_ = 1;
    }

    if (layoutChanged || ! phasesReady_)
        rebuildUnison (false, nullptr);
}

//==============================================================================
void BandLimitedOscillator::setFrequency (float hz) noexcept
{
    if (! (hz > 0.0f))          // also catches NaN
        hz = 0.001f;

    hz = math::clamp (hz, 0.001f, maxFreq_);

    baseFreq_   = hz;
    centreFreq_ = hz * voices_[centreIndex_].ratio;

    const float invHz       = 1.0f / hz;
    const float maxPartials = maxPartialHz_ * invHz;

    for (int v = 0; v < numVoices_; ++v)
    {
        auto& uv = voices_[v];
        uv.inc = math::clamp (hz * uv.ratio * invSampleRate_, 0.0f, 0.49f);
        uv.numPartials = math::clamp ((int) (maxPartials * uv.invRatio), 1, partialLimit_);
    }
}

//==============================================================================
bool BandLimitedOscillator::processSample (float& outL, float& outR,
                                           float phaseMod, float fmRatio) noexcept
{
    // --- smoothed shape ------------------------------------------------------
    shapeSmoothed_ += (shapeTarget_ - shapeSmoothed_) * shapeCoeff_;
    deriveShapeConstants();

    // --- additive engine: control-rate targets, per-sample glide -------------
    if (usesPartials_)
    {
        if (controlCounter_ <= 0)
        {
            updatePartialTargets();
            controlCounter_ = kControlInterval;
        }
        --controlCounter_;

        const float c = gainCoeff_;
        for (int k = 0; k < partialLimit_; ++k)
        {
            gainRe_[k] += (targetRe_[k] - gainRe_[k]) * c;
            gainIm_[k] += (targetIm_[k] - gainIm_[k]) * c;
        }
    }

    const float pm = std::isfinite (phaseMod) ? math::clamp (phaseMod, -64.0f, 64.0f) : 0.0f;
    const float fm = std::isfinite (fmRatio)  ? math::clamp (fmRatio, -16.0f, 16.0f)  : 1.0f;

    float sumL = 0.0f, sumR = 0.0f;
    bool  wrapped = false;

    for (int v = 0; v < numVoices_; ++v)
    {
        auto& uv = voices_[v];

        const float dt  = math::clamp (uv.inc * fm, -0.49f, 0.49f);
        const float adt = std::fabs (dt);

        const float y = renderWave (uv.phase + pm, adt, uv);

        float next = uv.phase + dt;
        if (next >= 1.0f || next < 0.0f)
        {
            next -= std::floor (next);
            if (v == centreIndex_)
                wrapped = true;
        }
        uv.phase = next;

        sumL += y * uv.gainL;
        sumR += y * uv.gainR;
    }

    outL = math::sanitise (sumL);
    outR = math::sanitise (sumR);
    return wrapped;
}

//==============================================================================
void BandLimitedOscillator::hardSync() noexcept
{
    for (int v = 0; v < numVoices_; ++v)
        voices_[v].phase = voices_[v].startPhase;
}

//==============================================================================
void BandLimitedOscillator::rebuildUnison (bool retrigger, RandomGenerator* rng) noexcept
{
    const int n = math::clamp (params_.unison, 1, dsp::kMaxUnison);
    numVoices_   = n;
    centreIndex_ = n / 2;

    const float spreadCents = math::clamp (params_.detune, 0.0f, 1.0f) * kMaxDetuneCents;
    const float stereo      = math::clamp (params_.stereo, 0.0f, 1.0f);

    // Equal power across the stack: the unison level is constant as voices are
    // added, and a single centred voice comes out at unity in both channels.
    const float level = 1.41421356f / std::sqrt ((float) n);

    const float start   = params_.startPhase;
    const bool  freeRun = ! (start >= 0.0f);

    for (int i = 0; i < dsp::kMaxUnison; ++i)
    {
        auto& uv = voices_[i];

        const float u = (n == 1 || i >= n)
                          ? 0.0f
                          : ((float) i / (float) (n - 1)) * 2.0f - 1.0f;   // -1..1

        // Mild S-curve: the outer pairs beat faster than the inner ones, which
        // is what makes a seven-voice stack sound like water rather than a chord.
        const float shaped = u * (0.55f + 0.45f * std::fabs (u));
        const float cents  = shaped * spreadCents;

        uv.ratio    = std::pow (2.0f, cents * (1.0f / 1200.0f));
        uv.invRatio = 1.0f / uv.ratio;

        float l = 1.0f, r = 1.0f;
        math::equalPowerPan (u * stereo, l, r);
        uv.gainL = l * level;
        uv.gainR = r * level;

        // The centre voice is the hard-sync master, so it lands exactly on
        // startPhase; the rest fan out around it.
        const int   ring   = ((i - centreIndex_) + dsp::kMaxUnison) % dsp::kMaxUnison;
        const float offset = freeRun ? (rng != nullptr ? rng->nextFloat() : internalRng_.nextFloat())
                                     : goldenOffset (ring);

        uv.startPhase = freeRun ? offset : math::wrapPhase (start + offset);

        if (retrigger)
            uv.phase = freeRun ? uv.phase : uv.startPhase;
    }

    if (retrigger && freeRun)
    {
        // Free-running: keep whatever phase the oscillator was already at, but
        // make sure it is valid the very first time round.
        if (! phasesReady_)
            for (auto& uv : voices_)
                uv.phase = uv.startPhase;
    }

    // Keep the increments and partial counts in step with the new ratios.
    setFrequency (baseFreq_);
}

//==============================================================================
void BandLimitedOscillator::deriveShapeConstants() noexcept
{
    const float sh = math::clamp (shapeSmoothed_, 0.0f, 1.0f);

    switch (wave_)
    {
        case OscWave::Triangle:
        {
            triWidth_   = math::clamp (math::lerp (0.12f, 0.88f, sh), 0.06f, 0.94f);
            triInvW_    = 1.0f / triWidth_;
            triInvComp_ = 1.0f / (1.0f - triWidth_);
            break;
        }

        case OscWave::Saw:
        {
            // 0.0 .. 0.5  soft (triangle) -> plain saw
            // 0.5 .. 1.0  plain saw -> saw plus an octave partner (bright, thin)
            sawTriMix_ = math::clamp (sh * 2.0f, 0.0f, 1.0f);
            sawOctave_ = math::clamp (sh * 2.0f - 1.0f, 0.0f, 1.0f);
            break;
        }

        case OscWave::Square:
        case OscWave::Pulse:
        {
            const float w = (wave_ == OscWave::Square) ? math::lerp (0.15f, 0.85f, sh)
                                                       : math::lerp (0.48f, 0.05f, sh);
            pulseWidth_ = math::clamp (w, 0.02f, 0.98f);
            pulseDc_    = 2.0f * pulseWidth_ - 1.0f;                                 // analytic DC
            pulseScale_ = 0.5f / std::max (pulseWidth_, 1.0f - pulseWidth_);         // peak trim
            break;
        }

        case OscWave::Shark:
        {
            // Saw folded back on itself above a threshold: one jump, one corner.
            const float thr = math::clamp (math::lerp (0.92f, 0.08f, sh), 0.02f, 0.98f);
            sharkThr_  = thr;
            sharkEdge_ = (thr + 1.0f) * 0.5f;                       // phase of the fold
            sharkDc_   = -0.5f * (1.0f - thr) * (1.0f - thr);       // analytic DC
            const float hi = thr - sharkDc_;
            const float lo = -1.0f - sharkDc_;
            sharkScale_ = 1.0f / std::max (0.05f, std::max (std::fabs (hi), std::fabs (lo)));
            break;
        }

        default:
            break;      // the additive waves take their shape in updatePartialTargets()
    }
}

//==============================================================================
void BandLimitedOscillator::updatePartialTargets() noexcept
{
    const float sh    = math::clamp (shapeSmoothed_, 0.0f, 1.0f);
    const int   limit = math::clamp (partialLimit_, 1, kMaxPartials);

    float amp[kMaxPartials] {};
    float phi[kMaxPartials] {};     // per-partial phase offset, in cycles

    float makeup = kSineMakeup;

    switch (wave_)
    {
        case OscWave::Sine:
        {
            // A pure sine up to the centre of the control, then a gentle bloom
            // of low harmonics: warm, never buzzy.
            const float h = math::clamp ((sh - 0.5f) * 2.0f, 0.0f, 1.0f);
            amp[0] = 1.0f;
            if (limit > 1) amp[1] = 0.34f * h;
            if (limit > 2) amp[2] = 0.16f * h * h;
            makeup = kSineMakeup;
            break;
        }

        case OscWave::Hollow:
        {
            // Odd harmonics only — a clarinet heard from under the surface.
            const float tilt = math::lerp (2.9f, 1.05f, sh);
            for (int k = 0; k < limit; k += 2)
                amp[k] = std::exp (-tilt * harmonicLogs.logK[k + 1]);

            if (limit > 1)
                amp[1] = 0.04f * sh;        // a breath of second harmonic, so it is not sterile

            makeup = kHollowMakeup;
            break;
        }

        case OscWave::Glass:
        {
            const float tilt   = math::lerp (1.60f, 0.55f, sh);
            const float inharm = 0.00006f + 0.00024f * sh;
            const float f0     = centreFreq_;

            for (int k = 0; k < limit; ++k)
            {
                const int   h    = k + 1;
                const float dh   = (float) (h - 6);
                const float bell = 1.0f + 0.85f * std::exp (-0.10f * dh * dh);

                amp[k] = std::exp (-tilt * harmonicLogs.logK[h]) * bell;

                // Stiff-string inharmonicity: partial h sits a little sharp of
                // h * f0. Running it as a slow phase drift keeps one phase
                // accumulator per voice while the partials still de-tune.
                float offsetHz = inharm * (float) (h * h * h) * f0;
                offsetHz = math::clamp (offsetHz, -400.0f, 400.0f);
                driftPhase_[k] = math::wrapPhase (driftPhase_[k] + offsetHz * controlSeconds_);
                phi[k] = driftPhase_[k];

                // Faint shimmer so a sustained note keeps ringing rather than freezing.
                modPhase_[k] = math::wrapPhase (modPhase_[k] + (0.11f + 0.037f * (float) h) * controlSeconds_);
                amp[k] *= 0.88f + 0.12f * sineTable (modPhase_[k]);
            }

            makeup = kGlassMakeup;
            break;
        }

        case OscWave::Water:
        default:
        {
            // Soft spectrum with slow internal motion: the partials drift in
            // level and in phase, so a held note never sits still.
            const float rate  = math::lerp (0.02f, 0.38f, sh);    // Hz
            const float depth = 0.22f + 0.48f * sh;
            const float decay = math::lerp (0.62f, 0.30f, sh);

            for (int k = 0; k < limit; ++k)
            {
                const float h = (float) (k + 1);

                modPhase_[k]   = math::wrapPhase (modPhase_[k]   + rate * (0.53f + 0.19f * h) * controlSeconds_);
                driftPhase_[k] = math::wrapPhase (driftPhase_[k] + rate * (0.31f + 0.13f * h) * controlSeconds_);

                const float wobble = 1.0f - depth + depth * (0.5f + 0.5f * sineTable (modPhase_[k]));
                amp[k] = std::exp (-decay * (float) k) * math::clamp (wobble, 0.0f, 1.5f);
                phi[k] = 0.42f * sineTable (driftPhase_[k]);
            }

            makeup = kWaterMakeup;
            break;
        }
    }

    // --- band limit: drop the partials this note cannot carry, and fade the
    //     last few in so the count can change without a click -----------------
    const int   nCentre   = math::clamp (voices_[centreIndex_].numPartials, 1, limit);
    const float fadeStart = 0.68f * (float) nCentre;
    const float fadeSpan  = std::max (1.0f, (float) nCentre - fadeStart);

    float sum = 0.0f;
    for (int k = 0; k < limit; ++k)
    {
        if (k >= nCentre)
        {
            amp[k] = 0.0f;
            continue;
        }

        const float h = (float) (k + 1);
        if (h > fadeStart)
            amp[k] *= 1.0f - math::smoothstep ((h - fadeStart) / fadeSpan);

        sum += amp[k];
    }

    // Normalising by the sum of the partial amplitudes bounds the waveform at
    // +/-1 whatever the spectrum does, so no shape can ever jump the level.
    const float norm = (sum > 1.0e-6f) ? (makeup / sum) : 0.0f;

    int active = 1;
    for (int k = 0; k < limit; ++k)
    {
        const float a = amp[k] * norm;
        targetRe_[k] = a * sineTable (phi[k] + 0.25f);      // a * cos(phi)
        targetIm_[k] = a * sineTable (phi[k]);              // a * sin(phi)

        if (std::fabs (a) > 2.0e-4f)
            active = k + 1;
    }

    for (int k = limit; k < kMaxPartials; ++k)
        targetRe_[k] = targetIm_[k] = 0.0f;

    activePartials_ = active;
}

//==============================================================================
void BandLimitedOscillator::snapPartialGains() noexcept
{
    for (int k = 0; k < kMaxPartials; ++k)
    {
        gainRe_[k] = targetRe_[k];
        gainIm_[k] = targetIm_[k];
    }
}

//==============================================================================
float BandLimitedOscillator::renderWave (float phase, float dt, const UnisonVoice& uv) const noexcept
{
    const float p = math::wrapPhase (phase);

    switch (wave_)
    {
        case OscWave::Triangle: return renderTriangle (p, dt, triWidth_, triInvW_, triInvComp_);
        case OscWave::Saw:      return renderSaw (p, dt);
        case OscWave::Square:
        case OscWave::Pulse:    return renderPulse (p, dt);
        case OscWave::Shark:    return renderShark (p, dt);

        case OscWave::Sine:
        case OscWave::Hollow:
        case OscWave::Glass:
        case OscWave::Water:
            return renderPartials (p, uv.numPartials < activePartials_ ? uv.numPartials : activePartials_);

        case OscWave::NumWaves:
        default:
            return 0.0f;
    }
}

//==============================================================================
float BandLimitedOscillator::renderPartials (float p, int n) const noexcept
{
    const float s1 = sineTable (p);
    const float c1 = sineTable (p + 0.25f);

    float sk = s1, ck = c1;
    float y  = gainRe_[0] * s1 + gainIm_[0] * c1;

    for (int k = 1; k < n; ++k)
    {
        // Complex rotation: (sk, ck) becomes sin/cos of (k+1) * theta.
        const float ns = sk * c1 + ck * s1;
        const float nc = ck * c1 - sk * s1;
        sk = ns;
        ck = nc;
        y += gainRe_[k] * ns + gainIm_[k] * nc;
    }

    return y;
}

//==============================================================================
float BandLimitedOscillator::renderTriangle (float p, float dt, float width,
                                             float invW, float invComp) const noexcept
{
    float y = (p < width) ? (2.0f * p * invW - 1.0f)
                          : (1.0f - 2.0f * (p - width) * invComp);

    // Both corners are slope discontinuities: polyBLAMP rounds them at exactly
    // the rate the slope changes, which is what keeps a high triangle clean.
    const float slopeStep = 2.0f * (invW + invComp) * dt;   // change in level per sample
    y += 0.5f * slopeStep * math::polyBlamp (p, dt);
    y -= 0.5f * slopeStep * math::polyBlamp (math::wrapPhase (p - width), dt);

    return y;
}

//==============================================================================
float BandLimitedOscillator::renderSaw (float p, float dt) const noexcept
{
    float y = (2.0f * p - 1.0f) - math::polyBlep (p, dt);

    if (sawTriMix_ < 1.0f)
        y = math::lerp (renderTriangle (p, dt, 0.5f, 2.0f, 2.0f), y, sawTriMix_);

    if (sawOctave_ > 0.0f)
    {
        const float q = math::wrapPhase (p + 0.5f);
        y += sawOctave_ * ((2.0f * q - 1.0f) - math::polyBlep (q, dt));
    }

    return y;
}

//==============================================================================
float BandLimitedOscillator::renderPulse (float p, float dt) const noexcept
{
    float y = (p < pulseWidth_) ? 1.0f : -1.0f;

    y += math::polyBlep (p, dt);                                        // rising edge
    y -= math::polyBlep (math::wrapPhase (p - pulseWidth_), dt);        // falling edge

    return (y - pulseDc_) * pulseScale_;
}

//==============================================================================
float BandLimitedOscillator::renderShark (float p, float dt) const noexcept
{
    const float s = 2.0f * p - 1.0f;

    float y = (s > sharkThr_) ? (2.0f * sharkThr_ - s) : s;

    y -= sharkThr_ * math::polyBlep (p, dt);                                    // jump of -2 * thr
    y -= 2.0f * dt * math::polyBlamp (math::wrapPhase (p - sharkEdge_), dt);    // fold corner

    return (y - sharkDc_) * sharkScale_;
}

} // namespace ripples
