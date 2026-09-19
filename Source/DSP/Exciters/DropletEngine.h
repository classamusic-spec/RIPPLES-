#pragma once

#include "Parameters/ParameterEnums.h"
#include "Utilities/DSPConstants.h"
#include "Utilities/MathUtils.h"
#include "Utilities/RandomGenerator.h"

#include <array>
#include <cstdint>
#include <vector>

namespace ripples
{

/**
    DROPLETS — the water-drop exciter.

    One droplet is: a short excitation (impact tick: band-passed noise burst)
    into a small resonant cavity (two decaying sinusoidal partials) whose pitch
    RISES as the bubble cavity collapses, wrapped in a fast attack / exponential
    decay envelope and panned.  The rising chirp is the cue that makes the sound
    read as "drop" rather than "click"; SPLASH scales it.

    GRAVITY and BOUNCE re-strike the same droplet voice in a geometric series —
    successive impacts arrive closer together and quieter, exactly like a real
    drip landing in a basin.

    ATMOSPHERIC mode spawns droplets stochastically at a rate set by DENSITY.
    NOTE mode tunes each droplet to the note passed to trigger(), so drops are
    melodically playable.

    Realtime safety: the whole voice pool is allocated in prepare(); trigger()
    and processSample() never allocate, lock or log.

    @note processSample() ADDS this sample's droplet output into outL/outR — the
          engine is a layer, not an insert.  Pass zeros to obtain the droplets
          on their own.
*/
class DropletEngine
{
public:
    struct Params
    {
        float amount = 0.0f, density = 0.3f, size = 0.5f, tone = 0.5f,
              splash = 0.3f, gravity = 0.5f, bounce = 0.3f,
              random = 0.5f, spread = 0.5f;     // all 0..1
        DropletMode mode = DropletMode::Atmospheric;
    };

    void prepare (double sampleRate, uint32_t seed);
    void reset() noexcept;
    void setParams (const Params& p) noexcept;
    void trigger (float noteHz, float velocity) noexcept;   // NOTE mode / note-on
    void processSample (float& outL, float& outR) noexcept;

    /** Pops one pending droplet event for the UI. Returns false when none left.
        Drain this from the same thread that calls processSample(). */
    bool consumeDropletEvent (float& intensity, float& pan) noexcept;

    //== Extra helpers (not part of the contract; safe to ignore) ==============
    int getActiveDropletCount() const noexcept;
    int getMaxDroplets() const noexcept { return (int) pool.size(); }

private:
    //==========================================================================
    struct Droplet
    {
        bool  active = false;

        // Body: two magic-circle (lossless 2nd-order) sinusoids.
        float s1x = 0.0f, s1y = 0.0f, s2x = 0.0f, s2y = 0.0f;
        float partial2Ratio = 2.3f, partial2Level = 0.2f;

        // Pitch trajectory — the collapsing-cavity chirp.
        float freqHz = 400.0f, targetHz = 400.0f;
        float chirpDepth = 0.4f, riseSeconds = 0.03f, riseCoeff = 0.01f;

        // Envelopes.
        float attackEnv = 0.0f, attackCoeff = 0.3f;
        float decayEnv  = 0.0f, decayMul  = 0.999f;
        float decayEnv2 = 0.0f, decayMul2 = 0.999f;
        float noiseEnv  = 0.0f, noiseMul  = 0.99f;
        float noiseLevel = 0.1f, noiseDecaySeconds = 0.008f;

        // Impact tick band-pass (TPT state-variable filter).
        float bpG = 0.1f, bpR2 = 0.6f, bpDen = 1.0f, bpS1 = 0.0f, bpS2 = 0.0f;

        // Output shaping.
        float amplitude = 0.0f, pan = 0.0f, gainL = 0.707f, gainR = 0.707f;
        float lpZ = 0.0f, lpCoeff = 0.5f;

        // Bounce scheduling.
        int   bouncesLeft = 0, bounceCountdown = 0;
        float bounceIntervalSamples = 0.0f, bounceIntervalRatio = 0.7f;
        float bounceGainRatio = 0.55f, bouncePitchRatio = 1.03f;
        float decaySeconds = 0.2f;

        RandomGenerator rng;
    };

    struct Event { float intensity = 0.0f; float pan = 0.0f; };
    static constexpr int kEventCapacity = 32;
    static_assert ((kEventCapacity & (kEventCapacity - 1)) == 0,
                   "kEventCapacity must be a power of two");

    Droplet& allocateDroplet() noexcept;
    void  configureAndStrike (Droplet& d, float targetHz, float amp, bool melodic) noexcept;
    void  strike (Droplet& d, float targetHz, float amp, float decaySeconds) noexcept;
    void  spawnAtmospheric() noexcept;
    void  scheduleNextSpawn() noexcept;
    void  pushEvent (float intensity, float pan) noexcept;
    float atmosphericFrequency (RandomGenerator& g) const noexcept;

    //==========================================================================
    double sampleRate = 44100.0;
    float  maxBodyHz  = 8820.0f;     // 0.2 * sr — keeps the oscillators far from Nyquist
    float  piOverSr   = 0.0f;

    Params params {};
    float  amountTarget = 0.0f, amountSmoothed = 0.0f, amountCoeff = 0.002f;

    std::vector<Droplet> pool;       // allocated in prepare()
    RandomGenerator rng;

    float spawnCountdown = 0.0f;

    std::array<Event, kEventCapacity> eventRing {};
    uint32_t eventWrite = 0, eventRead = 0;
};

} // namespace ripples
