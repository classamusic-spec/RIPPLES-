#pragma once

/*
    RIPPLES — sub oscillator.

    Sine, triangle or square one or two octaves below the played note. This is
    the weight under abyss basses and cinematic low end, so it stays clean:
    the triangle corners are polyBLAMPed and the square edges polyBLEPed, and
    every shape is DC free by construction.
*/

#include "Parameters/ParameterEnums.h"

namespace ripples
{

class SubOscillator
{
public:
    struct Params { SubWave wave = SubWave::Sine; int octave = -1; };   // octave: -1 or -2

    void prepare (double sampleRate);
    void reset() noexcept;
    void setParams (const Params& p) noexcept;
    void setFrequency (float baseHz) noexcept;   // the *note* frequency; octave applied inside
    float processSample() noexcept;

private:
    double sampleRate_    = 44100.0;
    float  invSampleRate_ = 1.0f / 44100.0f;
    float  maxFreq_       = 21168.0f;

    Params params_ {};
    float  octaveRatio_ = 0.5f;
    float  baseHz_      = 110.0f;
    float  phase_       = 0.0f;
    float  inc_         = 0.0f;
};

} // namespace ripples
