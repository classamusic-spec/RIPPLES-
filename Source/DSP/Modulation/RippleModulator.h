#pragma once

namespace ripples
{

/**
    RIPPLE — a triggered damped wave: sin(2*pi*f*t) * exp(-decay*t).

    A stone dropped in: the surface swings hard, then settles. It is the one
    modulator here with a beginning and an end, so three things matter as much
    as the formula.

    It has to reach the depth it was asked for. A steep decay eats most of the
    first quarter cycle, so the raw formula peaks at 0.23 when `decay` is at
    maximum — the control would read as a depth control rather than a time
    control. The wave is therefore normalised by its own analytic peak
    (max of exp(-n*c)*sin(2*pi*c), which sits at c = atan(2*pi/n)/(2*pi)), so
    `intensity` means the peak excursion whatever the decay is.

    It has to retrigger without clicking. A new trigger restarts the wave at
    phase zero, where the wave is silent, and carries the difference between the
    old output and the new one forward as a residual that fades over about
    twelve milliseconds. The residual is scaled by (1 - |wave|), so the sum can
    never leave -1..1 and the blend disappears exactly as the new ripple takes
    over. The same mechanism covers a parameter change mid-ring, including a
    polarity flip.

    And it has to stop. `decay` is expressed in nepers per cycle so it means the
    same thing at any rate, and `cycles` clamps it from below so the ring can
    never outlast its allotted number of cycles. isActive() goes false once both
    channels are under -80 dB, which is what lets a voice free itself instead of
    ringing inaudibly forever.

    `spread` delays the right-hand ripple rather than merely offsetting its
    phase — the wave reaches one side first, envelope and all, which is both
    what water does and what keeps the right channel peak at 1 as well.

      rateHz  the frequency of the wave
      decay   0..1, higher = faster: ~37 cycles of ring down to about one
      cycles  the hard bound on the ring length
      spread  0..1, up to half a cycle of delay on the right value
      invert  polarity
*/
class RippleModulator
{
public:
    struct Params
    {
        float rateHz = 4.0f;
        float decay  = 0.5f;   // 0..1, higher = faster decay
        float cycles = 4.0f;   // how many cycles before it is considered finished
        float spread = 0.0f;   // 0..1 stereo phase offset
        bool  invert = false;  // polarity
    };

    void prepare (double sampleRate);
    void reset() noexcept;
    void setParams (const Params& p) noexcept;

    /** Restarts the ripple, scaled by intensity (0..1). Click-free while ringing. */
    void trigger (float intensity) noexcept;

    /** Steps the ripple forward by numSamples and returns the new left value. */
    float advance (int numSamples) noexcept;

    float getValue()  const noexcept { return value; }
    float getValueR() const noexcept { return valueR; }

    /** False once the ripple is inaudible, so voices and CPU are not wasted. */
    bool isActive() const noexcept;

    static constexpr float kMinRateHz = 0.01f;
    static constexpr float kMaxRateHz = 100.0f;
    static constexpr float kMinCycles = 0.25f;
    static constexpr float kMaxCycles = 64.0f;

private:
    void recompute() noexcept;
    void updateWave() noexcept;
    void refreshOutputs() noexcept;

    double sampleRate    = 44100.0;
    double invSampleRate = 1.0 / 44100.0;

    Params params {};

    // Derived from the parameters.
    float decayPerSecond = 1.0f;
    float durationLimit  = 1.0f;   // seconds of ring, before the stereo delay
    float delaySeconds   = 0.0f;   // right-channel delay
    float peakGain       = 1.0f;   // normalises the damped sine to a peak of 1
    float polarity       = 1.0f;

    // State.
    double elapsed   = 0.0;        // seconds since the trigger
    float  amplitude = 0.0f;
    bool   ringing   = false;

    float envL = 0.0f, envR = 0.0f;
    float waveL = 0.0f, waveR = 0.0f;

    float residualL = 0.0f, residualR = 0.0f;

    float value = 0.0f, valueR = 0.0f;
};

} // namespace ripples
