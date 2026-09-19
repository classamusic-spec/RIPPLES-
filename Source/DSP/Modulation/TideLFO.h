#pragma once

#include "Parameters/ParameterEnums.h"

namespace ripples
{

/**
    TIDE — the smooth, deterministic modulator.

    A phase-accumulating LFO whose shapes are all *value continuous across the
    wrap point*, so the output never steps and never clicks when it is driving
    pitch or cutoff. Swell and DoubleWave are additionally slope continuous
    (C1), which is what stops them reading as a sawtooth with rounded corners.

    Flow is deliberately not a single periodic function: three phase
    accumulators run at mutually irrational ratios, so the composite contour
    keeps the same *character* while never repeating its felt shape. It stays
    continuous in time for the same reason a sine does — nothing in it wraps.

    Changing `shape` crossfades over ~12 ms rather than snapping, so editing the
    shape while a pad is held does not produce a step on the modulated
    destination.

    Output is -1..1. `stereoPhase` offsets the right-hand value only.
*/
class TideLFO
{
public:
    struct Params
    {
        TideShape shape   = TideShape::Sine;
        float rateHz      = 1.0f;
        float phaseOffset = 0.0f;   // 0..1
        float stereoPhase = 0.0f;   // 0..1 offset for the right channel value
    };

    void prepare (double sampleRate);
    void reset (float phase01 = 0.0f) noexcept;
    void setParams (const Params& p) noexcept;

    /** Steps the LFO forward by numSamples and returns the new left value. */
    float advance (int numSamples) noexcept;

    float getValue()  const noexcept { return value; }
    float getValueR() const noexcept { return valueR; }

    /** Main phase, 0..1 — for the LFOView playhead. */
    float getPhase() const noexcept { return (float) phase; }

    static constexpr float kMinRateHz = 0.001f;
    static constexpr float kMaxRateHz = 200.0f;

private:
    void updateValues() noexcept;
    float shapeValue (TideShape s, double p1, double p2, double p3) const noexcept;

    double sampleRate    = 44100.0;
    double invSampleRate = 1.0 / 44100.0;
    double increment     = 1.0 / 44100.0;

    // Three accumulators: the second and third are only used by Flow.
    double phase = 0.0, phaseB = 0.0, phaseC = 0.0;

    Params params {};

    // Shape-change crossfade.
    TideShape fadeFromShape = TideShape::Sine;
    int fadeLengthSamples   = 1;
    int fadeRemaining       = 0;

    float value  = 0.0f;
    float valueR = 0.0f;
};

} // namespace ripples
