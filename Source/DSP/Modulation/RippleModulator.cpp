#include "DSP/Modulation/RippleModulator.h"

#include "Utilities/MathUtils.h"
#include "Utilities/DSPConstants.h"

#include <cmath>

namespace ripples
{

namespace
{
    // -80 dB. Below this the ripple is doing nothing audible, so it is retired.
    constexpr float kSilence       = 1.0e-4f;
    constexpr float kSilenceNepers = 9.2103404f;   // -ln(kSilence)

    // decay 0..1 -> nepers per cycle. At 0 the wave rings for ~37 cycles, at 1
    // it is gone inside one. Expressing it per cycle keeps the control meaning
    // the same whether the ripple is at 0.5 Hz or 40 Hz.
    constexpr float kNepersPerCycleMin = 0.25f;
    constexpr float kNepersPerCycleMax = 9.25f;
    constexpr float kDecayCurve        = 1.6f;

    // Ceiling on the peak normalisation, so an absurdly short ring turns into a
    // quiet blip rather than a near-vertical edge.
    constexpr float kMaxPeakGain = 6.0f;

    // The retrigger / parameter-change blend.
    constexpr float kResidualTau = 0.012f;

    constexpr float kMaxSpreadCycles = 0.5f;

    inline double wrap01 (double p) noexcept
    {
        p -= std::floor (p);
        return p < 0.0 ? p + 1.0 : p;
    }

    /** The blend. `residual` is a signed 0..1 position between the wave and the
        boundary it is heading for, so the result is always inside -1..1 *and*
        can still reproduce any earlier output exactly — including the opposite
        sign, which a plain (1 - |wave|) scaling could not reach. */
    inline float blend (float wave, float residual) noexcept
    {
        const float room = residual >= 0.0f ? (1.0f - wave) : (1.0f + wave);
        return wave + residual * room;
    }

    /** Inverse of blend(): the residual that reproduces `target` from `wave`. */
    inline float blendResidual (float target, float wave) noexcept
    {
        const float d = target - wave;
        const float room = d >= 0.0f ? (1.0f - wave) : (1.0f + wave);
        return d / std::max (room, 1.0e-4f);
    }

    inline bool sameParams (const RippleModulator::Params& a,
                            const RippleModulator::Params& b) noexcept
    {
        return a.rateHz == b.rateHz && a.decay == b.decay && a.cycles == b.cycles
            && a.spread == b.spread && a.invert == b.invert;
    }
}

//==============================================================================
void RippleModulator::prepare (double newSampleRate)
{
    sampleRate    = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    invSampleRate = 1.0 / sampleRate;

    recompute();
    reset();
}

void RippleModulator::reset() noexcept
{
    elapsed   = 0.0;
    amplitude = 0.0f;
    ringing   = false;

    envL = envR = 0.0f;
    waveL = waveR = 0.0f;

    residualL = residualR = 0.0f;
    value     = valueR    = 0.0f;
}

void RippleModulator::setParams (const Params& p) noexcept
{
    // Identical parameters must be a no-op: the plug-in core re-sends them every
    // block, and re-anchoring the blend each time would drag the wave flat.
    if (sameParams (p, params))
        return;

    // Otherwise hold the output where it is, whatever the new parameters do.
    const float oldL = value;
    const float oldR = valueR;

    params = p;
    recompute();
    updateWave();

    residualL = math::clamp (math::sanitise (blendResidual (oldL, waveL)), -1.0f, 1.0f);
    residualR = math::clamp (math::sanitise (blendResidual (oldR, waveR)), -1.0f, 1.0f);

    refreshOutputs();
}

void RippleModulator::recompute() noexcept
{
    // sanitise() first: a NaN parameter would survive a bare clamp.
    const float rate   = math::clamp (math::sanitise (params.rateHz), kMinRateHz, kMaxRateHz);
    const float cycles = math::clamp (math::sanitise (params.cycles), kMinCycles, kMaxCycles);
    const float decay  = math::clamp (math::sanitise (params.decay), 0.0f, 1.0f);
    const float spread = math::clamp (math::sanitise (params.spread), 0.0f, 1.0f);

    params.rateHz = rate;
    params.cycles = cycles;
    params.decay  = decay;
    params.spread = spread;

    // Nepers per cycle from the decay control...
    float nepersPerCycle = math::lerp (kNepersPerCycleMin, kNepersPerCycleMax,
                                       std::pow (decay, kDecayCurve));

    // ...then clamped from below so the ring always fits inside `cycles`.
    nepersPerCycle = std::max (nepersPerCycle, kSilenceNepers / cycles);

    decayPerSecond = nepersPerCycle * rate;
    durationLimit  = cycles / rate;
    delaySeconds   = kMaxSpreadCycles * spread / rate;
    polarity       = params.invert ? -1.0f : 1.0f;

    // Analytic peak of exp(-n*c) * sin(2*pi*c): the crest sits where
    // tan(theta) = 2*pi/n. Dividing it out makes `intensity` mean the peak.
    const float a     = nepersPerCycle / math::twoPi;
    const float theta = std::atan (1.0f / a);
    const float peak  = std::exp (-a * theta) * std::sin (theta);

    peakGain = math::clamp (1.0f / std::max (peak, 1.0e-3f), 1.0f, kMaxPeakGain);
}

//==============================================================================
void RippleModulator::trigger (float intensity) noexcept
{
    const float oldL = value;
    const float oldR = valueR;

    amplitude = math::clamp (math::sanitise (intensity), 0.0f, 1.0f);

    elapsed = 0.0;
    ringing = amplitude > kSilence;

    if (! ringing)
        amplitude = 0.0f;

    updateWave();   // both channels are exactly zero at t = 0

    // Whatever was already on the output carries forward, so a retrigger on a
    // still-ringing modulator blends instead of snapping.
    residualL = math::clamp (math::sanitise (blendResidual (oldL, waveL)), -1.0f, 1.0f);
    residualR = math::clamp (math::sanitise (blendResidual (oldR, waveR)), -1.0f, 1.0f);

    refreshOutputs();
}

void RippleModulator::updateWave() noexcept
{
    if (! ringing)
    {
        envL = envR = 0.0f;
        waveL = waveR = 0.0f;
        return;
    }

    const float scale = polarity * amplitude * peakGain;

    const double tL = elapsed;
    envL  = math::sanitise (std::exp (-decayPerSecond * (float) tL));
    waveL = scale * envL * std::sin (math::twoPi * (float) wrap01 ((double) params.rateHz * tL));

    // The right-hand ripple is the same wave arriving later, envelope included.
    const double tR = elapsed - (double) delaySeconds;

    if (tR >= 0.0)
    {
        envR  = math::sanitise (std::exp (-decayPerSecond * (float) tR));
        waveR = scale * envR * std::sin (math::twoPi * (float) wrap01 ((double) params.rateHz * tR));
    }
    else
    {
        envR  = 1.0f;     // not started yet, but still very much alive
        waveR = 0.0f;
    }

    waveL = math::clamp (math::sanitise (waveL), -1.0f, 1.0f);
    waveR = math::clamp (math::sanitise (waveR), -1.0f, 1.0f);
}

void RippleModulator::refreshOutputs() noexcept
{
    value  = math::clamp (math::sanitise (blend (waveL, residualL)), -1.0f, 1.0f);
    valueR = math::clamp (math::sanitise (blend (waveR, residualR)), -1.0f, 1.0f);
}

float RippleModulator::advance (int numSamples) noexcept
{
    if (numSamples <= 0)
        return value;

    const double dt = (double) numSamples * invSampleRate;

    if (ringing)
    {
        elapsed += dt;
        updateWave();

        const float loudest = std::max (envL, envR) * amplitude * peakGain;

        if (loudest <= kSilence || elapsed >= (double) (durationLimit + delaySeconds))
        {
            // Already below -80 dB here, so dropping the last sliver is silent.
            ringing   = false;
            amplitude = 0.0f;
            updateWave();
        }
    }

    const float residualDecay = std::exp (-(float) dt / kResidualTau);
    residualL = math::sanitise (residualL * residualDecay);
    residualR = math::sanitise (residualR * residualDecay);

    if (std::fabs (residualL) <= kSilence) residualL = 0.0f;
    if (std::fabs (residualR) <= kSilence) residualR = 0.0f;

    refreshOutputs();
    return value;
}

bool RippleModulator::isActive() const noexcept
{
    return ringing || residualL != 0.0f || residualR != 0.0f;
}

} // namespace ripples
