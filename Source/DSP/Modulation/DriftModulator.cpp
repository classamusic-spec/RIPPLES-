#include "DSP/Modulation/DriftModulator.h"

#include "Utilities/MathUtils.h"
#include "Utilities/DSPConstants.h"

#include <cmath>

namespace ripples
{

namespace
{
    constexpr float kLayerSd   = 0.55f;
    constexpr float kSlowZeta  = 0.9f;    // just under critical: it glides
    constexpr float kFineZeta  = 1.1f;

    // The upper layer sits a little over two octaves above the lower one, and
    // never drops below 0.015 Hz so there is always audible motion.
    constexpr float kFineRatio     = 4.0f;
    constexpr float kFineMinRateHz = 0.015f;
    constexpr float kFineMaxRateHz = 2.0f;

    constexpr float kSlowWeight = 0.94f;
    constexpr float kFineWeight = 0.30f;

    constexpr float kUnitUniform = 1.7320508f;
    constexpr float kMaxOmegaDt  = 0.15f;
    constexpr int   kMaxSubSteps = 32;

    inline float softBound (float x) noexcept
    {
        constexpr float knee = 0.55f;
        const float a = std::fabs (x);
        if (a <= knee)
            return x;

        const float y = knee + (1.0f - knee) * std::tanh ((a - knee) / (1.0f - knee));
        return x < 0.0f ? -y : y;
    }

    inline void configureLayer (float& twoZetaOmega, float& omegaSq, float& forceGain,
                                float& omegaOut, float rateHz, float zeta) noexcept
    {
        const float w = math::twoPi * rateHz;

        omegaOut     = w;
        twoZetaOmega = 2.0f * zeta * w;
        omegaSq      = w * w;
        forceGain    = kLayerSd * 2.0f * std::sqrt (zeta) * w * std::sqrt (w);
    }
}

//==============================================================================
void DriftModulator::prepare (double newSampleRate, uint32_t seed)
{
    sampleRate    = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    invSampleRate = 1.0 / sampleRate;

    rng.seed (seed);
    recompute();
    reset();
}

void DriftModulator::reset() noexcept
{
    // As with CURRENT the RNG keeps running, so a reset starts a new journey
    // rather than replaying the last one.
    slow.clear();
    fine.clear();

    value = valueR = 0.0f;
}

void DriftModulator::setParams (const Params& p) noexcept
{
    params = p;
    recompute();
}

void DriftModulator::recompute() noexcept
{
    const float rate = math::clamp (params.rate, kMinRateHz, kMaxRateHz);

    configureLayer (slow.twoZetaOmega, slow.omegaSq, slow.forceGain, slow.omega,
                    rate, kSlowZeta);

    const float fineRate = math::clamp (rate * kFineRatio, kFineMinRateHz, kFineMaxRateHz);
    configureLayer (fine.twoZetaOmega, fine.omegaSq, fine.forceGain, fine.omega,
                    fineRate, kFineZeta);

    const float s = math::clamp (params.stereo, 0.0f, 1.0f);
    indepGain = s;
    corrGain  = std::sqrt (std::max (0.0f, 1.0f - s * s));
}

//==============================================================================
void DriftModulator::tidy (Particle& p) noexcept
{
    p.position = math::clamp (math::sanitise (p.position), -8.0f, 8.0f);
    p.velocity = math::clamp (math::sanitise (p.velocity), -1.0e3f, 1.0e3f);
}

void DriftModulator::stepLayer (Layer& layer, float dt) noexcept
{
    int steps = (int) std::ceil ((layer.omega * dt) / kMaxOmegaDt);
    steps = math::clamp (steps, 1, kMaxSubSteps);

    const float dtSub     = dt / (float) steps;
    const float sqrtDtSub = std::sqrt (dtSub);

    for (int i = 0; i < steps; ++i)
    {
        const float shared = rng.nextBipolar() * kUnitUniform;
        const float sepL   = rng.nextBipolar() * kUnitUniform;
        const float sepR   = rng.nextBipolar() * kUnitUniform;

        const float nL = corrGain * shared + indepGain * sepL;
        const float nR = corrGain * shared + indepGain * sepR;

        layer.left.velocity  += (-layer.twoZetaOmega * layer.left.velocity
                                 - layer.omegaSq * layer.left.position) * dtSub
                              + layer.forceGain * nL * sqrtDtSub;
        layer.left.position  += layer.left.velocity * dtSub;

        layer.right.velocity += (-layer.twoZetaOmega * layer.right.velocity
                                 - layer.omegaSq * layer.right.position) * dtSub
                              + layer.forceGain * nR * sqrtDtSub;
        layer.right.position += layer.right.velocity * dtSub;
    }

    tidy (layer.left);
    tidy (layer.right);
}

float DriftModulator::advance (int numSamples) noexcept
{
    if (numSamples <= 0)
        return value;

    const float dt = (float) ((double) numSamples * invSampleRate);

    stepLayer (slow, dt);
    stepLayer (fine, dt);

    const float l = kSlowWeight * slow.left.position  + kFineWeight * fine.left.position;
    const float r = kSlowWeight * slow.right.position + kFineWeight * fine.right.position;

    value  = math::clamp (math::sanitise (softBound (l)), -1.0f, 1.0f);
    valueR = math::clamp (math::sanitise (softBound (r)), -1.0f, 1.0f);

    return value;
}

} // namespace ripples
