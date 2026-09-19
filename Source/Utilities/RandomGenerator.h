#pragma once

#include <cstdint>
#include <cmath>

namespace ripples
{

/**
    Fast, realtime-safe PRNG (xoshiro128+ style). Deterministic per seed, no
    allocation, no locks. Each voice / modulator owns its own instance so there
    is never contention on the audio thread.
*/
class RandomGenerator
{
public:
    RandomGenerator() noexcept { seed (0x9E3779B9u); }
    explicit RandomGenerator (uint32_t s) noexcept { seed (s); }

    void seed (uint32_t s) noexcept
    {
        // SplitMix32 to spread a single word across the state.
        for (auto& w : state)
        {
            s += 0x9E3779B9u;
            uint32_t z = s;
            z = (z ^ (z >> 16)) * 0x85EBCA6Bu;
            z = (z ^ (z >> 13)) * 0xC2B2AE35u;
            w = z ^ (z >> 16);
        }
        if ((state[0] | state[1] | state[2] | state[3]) == 0u)
            state[0] = 0x1234567u;
    }

    /** Raw 32-bit value. */
    uint32_t nextUInt() noexcept
    {
        const uint32_t result = state[0] + state[3];
        const uint32_t t = state[1] << 9;

        state[2] ^= state[0];
        state[3] ^= state[1];
        state[1] ^= state[2];
        state[0] ^= state[3];
        state[2] ^= t;
        state[3] = (state[3] << 11) | (state[3] >> 21);

        return result;
    }

    /** Uniform in [0,1). */
    float nextFloat() noexcept
    {
        return (float) (nextUInt() >> 8) * (1.0f / 16777216.0f);
    }

    /** Uniform in [-1,1). */
    float nextBipolar() noexcept { return nextFloat() * 2.0f - 1.0f; }

    /** Uniform in [lo,hi). */
    float nextRange (float lo, float hi) noexcept { return lo + nextFloat() * (hi - lo); }

    /** Approximately Gaussian (mean 0, sd ~1) via sum of uniforms — cheap and
        good enough for organic modulation noise. */
    float nextGaussian() noexcept
    {
        const float s = nextFloat() + nextFloat() + nextFloat()
                      + nextFloat() + nextFloat() + nextFloat();
        return (s - 3.0f) * 0.7071f;
    }

    /** True with the given probability. */
    bool nextBool (float probability) noexcept { return nextFloat() < probability; }

private:
    uint32_t state[4] {};
};

} // namespace ripples
