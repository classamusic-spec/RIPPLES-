#pragma once

/*
    RIPPLES — ADSR envelope, used for both the amp and the mod/filter envelope.

    Analogue-style exponential segments: every stage is a one-pole heading for a
    target it never actually reaches, so the attack has a natural decelerating
    curve and the decay/release are real exponential tails rather than ramps.
    The coefficients are chosen so a segment still lands exactly on its
    end point after the programmed number of samples.

    The running value is never reset on note-on, so a retrigger from any stage
    starts its attack from wherever the envelope already was — no snap to zero,
    no click. isActive() stays true until the release has fallen below -100 dB,
    so a voice cannot be stolen while it is still audible.

    All coefficients are kept in double: a 20 s release at 192 kHz needs a pole
    within 2e-6 of unity, which single precision cannot resolve accurately.
*/

namespace ripples
{

//==============================================================================
class Envelope
{
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    struct Params
    {
        float attack = 0.01f, decay = 0.3f, sustain = 0.7f, release = 0.5f;
    };

    void prepare (double sampleRate);
    void setParams (const Params& p) noexcept;
    void noteOn() noexcept;
    void noteOff() noexcept;
    void reset() noexcept;                 // hard reset to Idle
    float processSample() noexcept;        // returns 0..1
    bool isActive() const noexcept;        // false once fully released
    Stage getStage() const noexcept;
    float getCurrentValue() const noexcept;

private:
    void recalculate() noexcept;

    double sampleRate_ = 44100.0;
    double value_      = 0.0;
    Stage  stage_      = Stage::Idle;

    double attackCoef_  = 0.0, attackBase_  = 0.0;
    double decayCoef_   = 0.0, decayBase_   = 0.0;
    double releaseCoef_ = 0.0, releaseBase_ = 0.0;
    double sustain_     = 0.7;
    double sustainCoef_ = 1.0;

    Params params_ {};
    bool   coeffsValid_ = false;
};

} // namespace ripples
