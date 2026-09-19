#pragma once

/*
    RIPPLES — WATER noise source.

    Six colours of noise that feed the main filter as part of the synthesis
    chain, not as an effect: white, pink (-3 dB/oct), deep rumble, a breathing
    surf wash, sparse resonant bubbles and fine air hiss.

    The two channels run completely separate generators — separate PRNG
    streams, separate filter states, separate bubble pools — so the noise is
    decorrelated and sits wide instead of collapsing to a mono point.
*/

#include "Parameters/ParameterEnums.h"
#include "Utilities/RandomGenerator.h"

#include <cstdint>

namespace ripples
{

class WaterNoise
{
public:
    struct Params { NoiseType type = NoiseType::White; float tone = 0.5f; };   // tone 0..1

    void prepare (double sampleRate, uint32_t seed);
    void reset() noexcept;
    void setParams (const Params& p) noexcept;
    void processSample (float& outL, float& outR) noexcept;

private:
    //==========================================================================
    /** One short resonant blip — a bubble. Runs as a decaying complex
        rotation, so there is no biquad to go unstable. */
    struct Blip
    {
        float re = 0.0f, im = 0.0f;
        float cosW = 1.0f, sinW = 0.0f;
        float decay = 0.999f;
        float freq = 800.0f;
        float glide = 1.0f;
        bool  active = false;
    };

    static constexpr int kMaxBlips = 8;

    /** Everything one channel owns. Two of these, never shared. */
    struct Channel
    {
        RandomGenerator rng;

        float white[2] {};      // fixed 19 kHz band limit on the raw source
        float pink[7] {};
        float deepLp[3] {};
        float deepCoef = 0.02f;
        float deepPhase = 0.0f;
        float airLp[2] {};
        float airCoef = 0.5f;

        float svfIc1 = 0.0f, svfIc2 = 0.0f;
        float svfA1 = 0.0f, svfA2 = 0.0f, svfA3 = 0.0f, svfK = 1.0f;

        float surfPhase = 0.0f, surfWander = 0.0f;
        float surfAmp = 1.0f, surfAmpTarget = 1.0f;

        float tiltLp = 0.0f;
        float dcX = 0.0f, dcY = 0.0f;

        Blip blips[kMaxBlips] {};
    };

    void  updateControl (Channel& ch, NoiseType type, float tone) noexcept;
    float generate (Channel& ch, NoiseType type, float tone, bool doControl) noexcept;
    float pinkStep (Channel& ch, float white) const noexcept;
    float gainForTone (NoiseType type, float tone) const noexcept;

    //==========================================================================
    double sampleRate_    = 44100.0;
    float  invSampleRate_ = 1.0f / 44100.0f;
    float  controlSeconds_ = 32.0f / 44100.0f;

    float pinkPole_[6] {};
    float pinkGain_[6] {};
    float bandLimitCoef_ = 1.0f;
    float whiteGain_ = 1.0f;
    float tiltCoef_ = 0.1f;
    float dcCoef_   = 0.9995f;
    float blipProb_ = 0.0002f;
    float toneCoef_ = 0.02f;

    Params params_ {};
    float  toneTarget_   = 0.5f;
    float  toneSmoothed_ = 0.5f;

    Channel left_, right_;
    int controlCounter_ = 0;
};

} // namespace ripples
