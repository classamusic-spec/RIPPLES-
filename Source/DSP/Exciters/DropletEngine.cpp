#include "DSP/Exciters/DropletEngine.h"

#include <cmath>

namespace ripples
{

namespace
{
    // Atmospheric cavity range: SIZE = 0 is a small, high drop; SIZE = 1 a big, low one.
    constexpr float kAtmoSmallHz   = 4200.0f;
    constexpr float kAtmoLargeHz   =  165.0f;

    // Spawn rate in droplets / second across the DENSITY range.
    constexpr float kMinSpawnRate  = 0.12f;
    constexpr float kMaxSpawnRate  = 26.0f;

    constexpr int   kMaxBounces    = 9;
    constexpr float kAttackSeconds = 0.0007f;   // click-free but still percussive
    constexpr float kSilence       = 3.0e-5f;   // -90 dB — retire the voice
    constexpr float kOutputLimit   = 2.0f;      // absolute bound on the engine output

    /** sin(w) for small w (|w| < 0.8) — 5th-order Taylor, error < 2e-4.
        Used to turn a frequency into a magic-circle coefficient every sample
        without paying for std::sin. */
    inline float sinSmall (float w) noexcept
    {
        const float w2 = w * w;
        return w * (1.0f - w2 * (1.0f / 6.0f) * (1.0f - w2 * 0.05f));
    }

    /** 1 +/- amount, uniformly distributed. */
    inline float jitter (RandomGenerator& g, float amount) noexcept
    {
        return 1.0f + g.nextBipolar() * amount;
    }

    /** Bounded soft limiter — exactly +/-limit, near-unity slope well below it. */
    inline float softLimit (float x, float limit) noexcept
    {
        return math::fastTanh (x / limit) * limit;
    }
}

//==============================================================================
void DropletEngine::prepare (double newSampleRate, uint32_t seed)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    maxBodyHz  = (float) (sampleRate * 0.20);
    piOverSr   = math::pi / (float) sampleRate;

    pool.assign ((size_t) dsp::kMaxDroplets, Droplet {});

    rng.seed (seed);
    for (size_t i = 0; i < pool.size(); ++i)
        pool[i].rng.seed (seed + 0x9E3779B9u * (uint32_t) (i + 1));

    amountCoeff = math::onePoleCoeff (dsp::kSmoothingSeconds, sampleRate);

    reset();
}

void DropletEngine::reset() noexcept
{
    for (auto& d : pool)
    {
        d.active = false;
        d.s1x = d.s1y = d.s2x = d.s2y = 0.0f;
        d.attackEnv = d.decayEnv = d.decayEnv2 = d.noiseEnv = 0.0f;
        d.bpS1 = d.bpS2 = 0.0f;
        d.lpZ = 0.0f;
        d.amplitude = 0.0f;
        d.bouncesLeft = 0;
        d.bounceCountdown = 0;
    }

    amountSmoothed = amountTarget;
    eventWrite = eventRead = 0;
    spawnCountdown = 0.0f;
    scheduleNextSpawn();
}

void DropletEngine::setParams (const Params& p) noexcept
{
    params = p;
    params.amount  = math::clamp (p.amount,  0.0f, 1.0f);
    params.density = math::clamp (p.density, 0.0f, 1.0f);
    params.size    = math::clamp (p.size,    0.0f, 1.0f);
    params.tone    = math::clamp (p.tone,    0.0f, 1.0f);
    params.splash  = math::clamp (p.splash,  0.0f, 1.0f);
    params.gravity = math::clamp (p.gravity, 0.0f, 1.0f);
    params.bounce  = math::clamp (p.bounce,  0.0f, 1.0f);
    params.random  = math::clamp (p.random,  0.0f, 1.0f);
    params.spread  = math::clamp (p.spread,  0.0f, 1.0f);

    amountTarget = params.amount;
}

int DropletEngine::getActiveDropletCount() const noexcept
{
    int n = 0;
    for (const auto& d : pool)
        if (d.active) ++n;
    return n;
}

//==============================================================================
DropletEngine::Droplet& DropletEngine::allocateDroplet() noexcept
{
    // Free voice first.
    for (auto& d : pool)
        if (! d.active)
            return d;

    // Otherwise steal the quietest. Pending bounces count for a little so an
    // ongoing drip is not dismantled by a newcomer.
    size_t best = 0;
    float  bestLevel = 1.0e30f;

    for (size_t i = 0; i < pool.size(); ++i)
    {
        const auto& d = pool[i];
        const float level = d.amplitude * d.decayEnv * d.attackEnv
                          + 0.01f * (float) d.bouncesLeft;
        if (level < bestLevel)
        {
            bestLevel = level;
            best = i;
        }
    }

    return pool[best];
}

void DropletEngine::pushEvent (float intensity, float pan) noexcept
{
    if (eventWrite - eventRead >= (uint32_t) kEventCapacity)
        ++eventRead;                                   // drop the oldest

    eventRing[(size_t) (eventWrite & (uint32_t) (kEventCapacity - 1))]
        = { math::clamp (intensity, 0.0f, 1.0f), math::clamp (pan, -1.0f, 1.0f) };
    ++eventWrite;
}

bool DropletEngine::consumeDropletEvent (float& intensity, float& pan) noexcept
{
    if (eventRead == eventWrite)
        return false;

    const auto& e = eventRing[(size_t) (eventRead & (uint32_t) (kEventCapacity - 1))];
    intensity = e.intensity;
    pan       = e.pan;
    ++eventRead;
    return true;
}

//==============================================================================
float DropletEngine::atmosphericFrequency (RandomGenerator& g) const noexcept
{
    const float centre = kAtmoSmallHz * std::pow (kAtmoLargeHz / kAtmoSmallHz, params.size);
    const float octaves = (0.18f + 1.05f * params.random) * 0.45f * g.nextGaussian();
    return math::clamp (centre * std::pow (2.0f, octaves), 35.0f, maxBodyHz);
}

void DropletEngine::scheduleNextSpawn() noexcept
{
    const float rate = kMinSpawnRate * std::pow (kMaxSpawnRate / kMinSpawnRate, params.density);
    const float mean = 1.0f / rate;

    // Poisson arrivals when RANDOM is up, metronomic dripping when it is down.
    const float u    = math::clamp (rng.nextFloat(), 1.0e-5f, 1.0f);
    const float expo = -std::log (u) * mean;
    float interval   = math::lerp (mean, expo, params.random);

    interval = math::clamp (interval, 0.003f, mean * 8.0f);
    spawnCountdown = interval * (float) sampleRate;
}

void DropletEngine::spawnAtmospheric() noexcept
{
    if (pool.empty())
        return;

    auto& d = allocateDroplet();
    const float hz  = atmosphericFrequency (d.rng);
    const float amp = 0.85f * (1.0f - 0.55f * params.random * d.rng.nextFloat());
    configureAndStrike (d, hz, amp, false);
}

void DropletEngine::trigger (float noteHz, float velocity) noexcept
{
    if (pool.empty())
        return;                       // prepare() has not run yet

    auto& d = allocateDroplet();

    float hz;
    if (params.mode == DropletMode::Note)
    {
        // SIZE offsets the cavity by up to +/- 1.5 octaves around the played note;
        // RANDOM stays small here so the engine remains melodically playable.
        const float sizeRatio = std::pow (2.0f, (0.5f - params.size) * 3.0f);
        hz = math::clamp (noteHz, 20.0f, 12000.0f) * sizeRatio
                * std::pow (2.0f, d.rng.nextBipolar() * params.random * 0.06f);
    }
    else
    {
        hz = atmosphericFrequency (d.rng);
    }

    const float vel = math::clamp (velocity, 0.0f, 1.0f);
    const float amp = (0.18f + 0.82f * vel) * (1.0f - 0.3f * params.random * d.rng.nextFloat());

    configureAndStrike (d, math::clamp (hz, 25.0f, maxBodyHz), amp,
                        params.mode == DropletMode::Note);
}

//==============================================================================
void DropletEngine::configureAndStrike (Droplet& d, float targetHz, float amp, bool melodic) noexcept
{
    auto& g = d.rng;
    const float r = params.random;

    // Ring time — a big cavity rings longer than a small one.
    float decaySec = math::lerp (0.055f, 0.42f, params.size);
    decaySec *= std::pow (2.0f, g.nextBipolar() * r * 0.85f);
    d.decaySeconds = math::clamp (decaySec, 0.015f, 1.6f);

    // The collapsing-cavity chirp. This is the "drop" cue; SPLASH scales it.
    float chirp = math::lerp (0.10f, 1.65f, params.splash) * jitter (g, r * 0.4f);
    d.chirpDepth = math::clamp (chirp, 0.02f, 2.6f);

    // The rise happens over the first few tens of milliseconds, as it does in a
    // real collapsing cavity. Keeping it short also means the long part of the
    // tail sits on the settled pitch, which is what makes NOTE mode playable.
    d.riseSeconds = math::clamp (d.decaySeconds * 0.16f * jitter (g, r * 0.45f),
                                 0.005f, 0.09f);

    // TONE: brightness of the cavity — the inharmonic partial and the impact tick.
    d.partial2Ratio = math::lerp (1.87f, 2.72f, params.tone) * jitter (g, r * 0.12f);
    d.partial2Level = math::clamp (math::lerp (0.04f, 0.45f, params.tone) * jitter (g, r * 0.4f),
                                   0.0f, 0.8f);
    d.noiseLevel    = math::lerp (0.03f, 0.26f, params.tone)
                        * math::lerp (0.6f, 1.6f, params.splash) * jitter (g, r * 0.5f);
    d.noiseDecaySeconds = math::clamp (math::lerp (0.0035f, 0.014f, params.size) * jitter (g, r * 0.5f),
                                       0.001f, 0.05f);

    // Stereo placement.
    d.pan = math::clamp (g.nextBipolar() * params.spread, -1.0f, 1.0f);
    math::equalPowerPan (d.pan, d.gainL, d.gainR);

    // Bouncing drip: a geometric series of impacts, each sooner and quieter.
    int bounces = (int) std::floor (params.bounce * (float) kMaxBounces
                                    * (1.0f - 0.45f * r * g.nextFloat()) + 0.5f);
    d.bouncesLeft = (int) math::clamp (bounces, 0, kMaxBounces);

    const float firstInterval = 0.40f * std::pow (0.075f / 0.40f, params.gravity)
                                      * jitter (g, r * 0.3f);
    d.bounceIntervalSamples = math::clamp (firstInterval, 0.02f, 1.2f) * (float) sampleRate;
    d.bounceIntervalRatio   = math::clamp (math::lerp (0.84f, 0.46f, params.gravity)
                                             * jitter (g, r * 0.07f), 0.35f, 0.94f);
    d.bounceGainRatio       = math::clamp (math::lerp (0.40f, 0.72f, params.bounce)
                                             * jitter (g, r * 0.12f), 0.2f, 0.85f);
    // Each bounce is a smaller impact, so it sits a touch higher. Kept tiny when
    // the engine has to stay in tune.
    d.bouncePitchRatio = 1.0f + (melodic ? 0.012f : 0.035f + 0.03f * params.splash);
    d.bounceCountdown  = (int) d.bounceIntervalSamples;

    d.amplitude = math::clamp (amp, 0.0f, 1.0f);

    strike (d, targetHz, d.amplitude, d.decaySeconds);
    pushEvent (d.amplitude, d.pan);
}

void DropletEngine::strike (Droplet& d, float targetHz, float amp, float decaySeconds) noexcept
{
    d.targetHz = math::clamp (targetHz, 25.0f, maxBodyHz);

    // Start below the target and rise to it: in NOTE mode the settled pitch is
    // the played note, so the drop is still melodically readable.
    d.freqHz = math::clamp (d.targetHz / (1.0f + d.chirpDepth), 20.0f, maxBodyHz);

    d.riseCoeff   = math::clamp (math::onePoleCoeff (d.riseSeconds, sampleRate), 0.0f, 1.0f);
    d.attackCoeff = math::clamp (math::onePoleCoeff (kAttackSeconds, sampleRate), 0.0f, 1.0f);

    d.attackEnv = 0.0f;
    d.decayEnv  = 1.0f;
    d.decayMul  = math::decayCoeff (decaySeconds, sampleRate);
    d.decayEnv2 = 1.0f;
    d.decayMul2 = math::decayCoeff (decaySeconds * math::lerp (0.22f, 0.8f, params.tone), sampleRate);

    d.noiseEnv  = 1.0f;
    d.noiseMul  = math::decayCoeff (d.noiseDecaySeconds, sampleRate);

    // Impact tick: a narrow band of noise above the cavity pitch.
    {
        const float fc = math::clamp (d.targetHz * 2.6f, 60.0f, (float) (sampleRate * 0.42));
        const float gg = std::tan (math::pi * fc / (float) sampleRate);
        d.bpG   = gg;
        d.bpR2  = 0.35f;                                   // Q ~ 2.9
        d.bpDen = 1.0f / (1.0f + d.bpR2 * gg + gg * gg);
        d.bpS1 = d.bpS2 = 0.0f;
    }

    // Brightness: a one-pole damper placed relative to the cavity pitch, so a
    // small bright drop stays bright and a big dull one stays dull.
    {
        const float fc = math::clamp (d.targetHz * math::lerp (1.2f, 9.0f, params.tone),
                                      120.0f, (float) (sampleRate * 0.45));
        d.lpCoeff = math::clamp (math::onePoleCoeff (1.0f / (math::twoPi * fc), sampleRate),
                                 0.0f, 1.0f);
        d.lpZ = 0.0f;
    }

    // Magic-circle oscillators start at the zero crossing — no onset click.
    d.s1x = 0.0f; d.s1y = 1.0f;
    d.s2x = 0.0f; d.s2y = 1.0f;

    d.amplitude = math::clamp (amp, 0.0f, 1.0f);
    d.active    = true;
}

//==============================================================================
void DropletEngine::processSample (float& outL, float& outR) noexcept
{
    if (pool.empty())
        return;                       // prepare() has not run yet

    amountSmoothed += (amountTarget - amountSmoothed) * amountCoeff;

    if (params.mode == DropletMode::Atmospheric && amountTarget > 1.0e-4f)
    {
        spawnCountdown -= 1.0f;
        if (spawnCountdown <= 0.0f)
        {
            spawnAtmospheric();
            scheduleNextSpawn();
        }
    }

    float sumL = 0.0f, sumR = 0.0f;

    for (auto& d : pool)
    {
        if (! d.active)
            continue;

        // --- bouncing: re-strike the same voice, sooner and quieter each time
        if (d.bouncesLeft > 0 && --d.bounceCountdown <= 0)
        {
            --d.bouncesLeft;
            d.bounceIntervalSamples *= d.bounceIntervalRatio;
            d.bounceCountdown = (int) std::max (d.bounceIntervalSamples, 16.0f);

            d.decaySeconds = std::max (d.decaySeconds * 0.78f, 0.012f);
            const float amp = d.amplitude * d.bounceGainRatio;
            strike (d, d.targetHz * d.bouncePitchRatio, amp, d.decaySeconds);
            pushEvent (amp, d.pan);
        }

        if (d.decayEnv < kSilence)
        {
            if (d.bouncesLeft <= 0)
                d.active = false;
            continue;                                   // silent, waiting to bounce
        }

        // --- pitch trajectory: rising toward the settled cavity frequency
        d.freqHz += (d.targetHz - d.freqHz) * d.riseCoeff;

        const float w1 = math::clamp (piOverSr * d.freqHz, 0.0f, 0.75f);
        const float k1 = 2.0f * sinSmall (w1);
        d.s1x += k1 * d.s1y;
        d.s1y -= k1 * d.s1x;

        const float w2 = math::clamp (piOverSr * d.freqHz * d.partial2Ratio, 0.0f, 0.70f);
        const float k2 = 2.0f * sinSmall (w2);
        d.s2x += k2 * d.s2y;
        d.s2y -= k2 * d.s2x;

        // NaN / runaway trap for the lossless oscillator pair.
        if (! (std::fabs (d.s1x) < 8.0f)) { d.s1x = 0.0f; d.s1y = 1.0f; }
        if (! (std::fabs (d.s2x) < 8.0f)) { d.s2x = 0.0f; d.s2y = 1.0f; }

        // --- envelopes
        d.attackEnv += (1.0f - d.attackEnv) * d.attackCoeff;
        d.decayEnv  *= d.decayMul;
        d.decayEnv2 *= d.decayMul2;

        float body = d.s1x * d.decayEnv + d.s2x * d.partial2Level * d.decayEnv2;
        body *= d.attackEnv;

        // --- impact tick
        float tick = 0.0f;
        if (d.noiseEnv > 1.0e-4f)
        {
            d.noiseEnv *= d.noiseMul;
            const float n  = d.rng.nextBipolar() * d.noiseEnv;
            const float hp = (n - (d.bpR2 + d.bpG) * d.bpS1 - d.bpS2) * d.bpDen;
            const float bp = d.bpG * hp + d.bpS1;
            d.bpS1 = d.bpG * hp + bp;
            const float lp = d.bpG * bp + d.bpS2;
            d.bpS2 = d.bpG * bp + lp;
            tick = bp * d.noiseLevel * 2.0f;
        }

        const float s = (body + tick) * d.amplitude;
        d.lpZ = math::sanitise (d.lpZ + (s - d.lpZ) * d.lpCoeff);

        sumL += d.lpZ * d.gainL;
        sumR += d.lpZ * d.gainR;
    }

    sumL = math::sanitise (sumL * amountSmoothed);
    sumR = math::sanitise (sumR * amountSmoothed);

    outL += softLimit (sumL, kOutputLimit);
    outR += softLimit (sumR, kOutputLimit);
}

} // namespace ripples
