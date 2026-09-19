#include "DSP/Modulation/CurrentModulator.h"

#include "Utilities/MathUtils.h"
#include "Utilities/DSPConstants.h"

#include <cmath>

namespace ripples
{

namespace
{
    // Stationary standard deviation of the main layer. Chosen so the modulator
    // genuinely uses its range: crests past +/-1 happen occasionally and are
    // rounded off by the soft bound rather than sliced.
    constexpr float kTargetSd = 0.52f;

    // The slow bias layer: one wander every half minute or so.
    constexpr float kBiasRateHz = 0.035f;
    constexpr float kBiasZeta   = 1.0f;
    constexpr float kBiasSd     = 0.45f;

    // Damping ratio sweep. 1.6 is overdamped and restless, 0.6 is a gliding
    // undulation that still never repeats.
    constexpr float kZetaMax = 1.6f;
    constexpr float kZetaMin = 0.6f;

    // A uniform draw scaled to unit variance. After two integrations the
    // distribution is Gaussian anyway, and this is a third of the cost.
    constexpr float kUnitUniform = 1.7320508f;

    // Explicit-integration safety: never let omega*dt exceed this in one step.
    constexpr float kMaxOmegaDt = 0.15f;
    constexpr int   kMaxSubSteps = 64;

    /** Smooth bound to (-1,1). Unity slope below the knee, asymptotic above it,
        C1 at the join — the crests round over instead of flattening. */
    inline float softBound (float x) noexcept
    {
        constexpr float knee = 0.55f;
        const float a = std::fabs (x);
        if (a <= knee)
            return x;

        const float y = knee + (1.0f - knee) * std::tanh ((a - knee) / (1.0f - knee));
        return x < 0.0f ? -y : y;
    }
}

//==============================================================================
void CurrentModulator::prepare (double newSampleRate, uint32_t seed)
{
    sampleRate    = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    invSampleRate = 1.0 / sampleRate;

    rng.seed (seed);
    recompute();
    reset();
}

void CurrentModulator::reset() noexcept
{
    // The RNG is deliberately *not* re-seeded: every note gets its own stretch
    // of water rather than the same wander replayed.
    mainL.clear(); mainR.clear();
    biasL.clear(); biasR.clear();

    value = valueR = 0.0f;
}

void CurrentModulator::setParams (const Params& p) noexcept
{
    params = p;
    recompute();
}

void CurrentModulator::recompute() noexcept
{
    const float rate   = math::clamp (params.rate, kMinRateHz, kMaxRateHz);
    const float smooth = math::clamp (params.smoothness, 0.0f, 1.0f);

    omega = math::twoPi * rate;

    const float zeta = math::lerp (kZetaMax, kZetaMin, smooth);

    twoZetaOmega = 2.0f * zeta * omega;
    omegaSq      = omega * omega;

    // g = sd * sqrt(4 * zeta * omega^3): keeps Var(x) = sd^2 for any rate/damping.
    forceGain = kTargetSd * 2.0f * std::sqrt (zeta) * omega * std::sqrt (omega);

    // A gentle third pole, tracking the rate so it never becomes a lag.
    smoothOmega = omega * math::lerp (6.0f, 3.0f, smooth);

    const float biasOmega = math::twoPi * kBiasRateHz;
    biasTwoZetaOmega = 2.0f * kBiasZeta * biasOmega;
    biasOmegaSq      = biasOmega * biasOmega;
    biasForceGain    = kBiasSd * 2.0f * std::sqrt (kBiasZeta) * biasOmega * std::sqrt (biasOmega);

    const float s = math::clamp (params.stereo, 0.0f, 1.0f);
    indepGain = s;
    corrGain  = std::sqrt (std::max (0.0f, 1.0f - s * s));

    const float bs = s * 0.7f;
    biasIndepGain = bs;
    biasCorrGain  = std::sqrt (std::max (0.0f, 1.0f - bs * bs));
}

//==============================================================================
void CurrentModulator::integrate (Particle& p, float noise, float dt, float sqrtDt,
                                  float twoZetaW, float wSq, float gain) noexcept
{
    // Semi-implicit (symplectic) Euler: the position uses the *new* velocity,
    // which is markedly better behaved than the plain explicit form.
    p.velocity += (-twoZetaW * p.velocity - wSq * p.position) * dt + gain * noise * sqrtDt;
    p.position += p.velocity * dt;
}

void CurrentModulator::tidy (Particle& p) noexcept
{
    p.position = math::clamp (math::sanitise (p.position), -8.0f, 8.0f);
    p.velocity = math::clamp (math::sanitise (p.velocity), -1.0e4f, 1.0e4f);
    p.smoothed = math::clamp (math::sanitise (p.smoothed), -8.0f, 8.0f);
}

float CurrentModulator::advance (int numSamples) noexcept
{
    if (numSamples <= 0)
        return value;

    const float dt = (float) ((double) numSamples * invSampleRate);

    //--------------------------------------------------------------- main layer
    int steps = (int) std::ceil ((omega * dt) / kMaxOmegaDt);
    steps = math::clamp (steps, 1, kMaxSubSteps);

    const float dtSub    = dt / (float) steps;
    const float sqrtDtSub = std::sqrt (dtSub);

    for (int i = 0; i < steps; ++i)
    {
        const float shared = rng.nextBipolar() * kUnitUniform;
        const float sepL   = rng.nextBipolar() * kUnitUniform;
        const float sepR   = rng.nextBipolar() * kUnitUniform;

        integrate (mainL, corrGain * shared + indepGain * sepL,
                   dtSub, sqrtDtSub, twoZetaOmega, omegaSq, forceGain);
        integrate (mainR, corrGain * shared + indepGain * sepR,
                   dtSub, sqrtDtSub, twoZetaOmega, omegaSq, forceGain);
    }

    // Third pole, exact for any block size so it can never go unstable.
    const float smoothA = 1.0f - std::exp (-smoothOmega * dt);
    mainL.smoothed += smoothA * (mainL.position - mainL.smoothed);
    mainR.smoothed += smoothA * (mainR.position - mainR.smoothed);

    tidy (mainL);
    tidy (mainR);

    //--------------------------------------------------------------- bias layer
    // Fixed at ~0.035 Hz, so a single full-block step is always far inside the
    // stability limit.
    {
        const float shared = rng.nextBipolar() * kUnitUniform;
        const float sepL   = rng.nextBipolar() * kUnitUniform;
        const float sepR   = rng.nextBipolar() * kUnitUniform;
        const float sqrtDt = std::sqrt (dt);

        integrate (biasL, biasCorrGain * shared + biasIndepGain * sepL,
                   dt, sqrtDt, biasTwoZetaOmega, biasOmegaSq, biasForceGain);
        integrate (biasR, biasCorrGain * shared + biasIndepGain * sepR,
                   dt, sqrtDt, biasTwoZetaOmega, biasOmegaSq, biasForceGain);

        tidy (biasL);
        tidy (biasR);
    }

    //------------------------------------------------------------------- output
    const float bias = math::clamp (params.drift, 0.0f, 1.0f);

    value  = math::clamp (math::sanitise (softBound (mainL.smoothed + bias * biasL.position)), -1.0f, 1.0f);
    valueR = math::clamp (math::sanitise (softBound (mainR.smoothed + bias * biasR.position)), -1.0f, 1.0f);

    return value;
}

} // namespace ripples
