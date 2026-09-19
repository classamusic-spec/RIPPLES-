#pragma once

namespace ripples
{

/**
    RIPPLE — a triggered damped wave: sin(2*pi*f*t) * exp(-decay*t).

    A stone dropped in: the surface swings hard, then settles. It is the one
    modulator here with a beginning and an end, so two things matter more than
    the formula.

    First, retriggering must not click. A new trigger restarts the wave at
    phase zero, where the wave itself is silent, and carries the *difference*
    between the old output and the new one forward as a residual that fades over
    about twelve milliseconds. The output is therefore continuous through a
    retrigger however hard it was still ringing. The same trick covers a
    parameter change mid-ring, including a polarity flip.

    Second, it has to stop. `decay` is expressed in nepers per cycle so it means
    the same thing at any rate, and `cycles` clamps the decay from below so the
    ring can never outlast its allotted number of cycles. isActive() goes false
    once the whole output is under -80 dB, which is what lets a voice free
    itself instead of ringing inaudibly forever.

      rateHz  the frequency of the wave
      decay   0..1, higher = faster: ~37 cycles of ring down to about one
      cycles  the hard bound on the ring length
      spread  0..1, up to half a cycle of phase offset on the right value
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
    float core (bool right) const noexcept;
    void refreshOutputs() noexcept;

    double sampleRate    = 44100.0;
    double invSampleRate = 1.0 / 44100.0;

    Params params {};

    // Derived.
    float decayPerSecond = 1.0f;
    float durationLimit  = 1.0f;   // seconds
    float spreadPhase    = 0.0f;   // cycles
    float polarity       = 1.0f;

    // State.
    double phase    = 0.0;   // 0..1
    double elapsed  = 0.0;   // seconds since the trigger
    float  envelope = 0.0f;
    float  amplitude = 0.0f;
    bool   ringing  = false;

    float residualL = 0.0f, residualR = 0.0f;

    float value = 0.0f, valueR = 0.0f;
};

} // namespace ripples
