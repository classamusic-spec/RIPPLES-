#include "DSP/Modulation/Envelope.h"

#include "Utilities/MathUtils.h"
#include "Utilities/DSPConstants.h"

#include <cmath>

namespace ripples
{

namespace
{
    /*  Overshoot ratios. The segment aims past its end point and is stopped
        when it arrives, which is what produces the curve:

          attack  — a fairly gentle 0.3 gives the natural "leans in at the top"
                    shape of an analogue attack without sounding like a ramp;
          decay /
          release — 2e-4 puts the target far below zero, so what you hear is a
                    proper exponential tail over the whole segment.            */
    constexpr double kAttackRatio = 0.30;
    constexpr double kDecayRatio  = 0.0002;

    /** Level at which the release is considered finished: -100 dB. */
    constexpr double kIdleLevel   = 1.0e-5;

    /** Pole placing the one-pole exactly on its end point after rateSamples. */
    inline double calcCoef (double rateSamples, double targetRatio) noexcept
    {
        if (! (rateSamples > 1.0))
            rateSamples = 1.0;

        return std::exp (-std::log ((1.0 + targetRatio) / targetRatio) / rateSamples);
    }

    /** NaN-safe clamp into [lo, hi]. */
    inline float clampTime (float v, float lo, float hi) noexcept
    {
        if (! (v > lo)) return lo;      // also catches NaN
        return v > hi ? hi : v;
    }
} // namespace

//==============================================================================
void Envelope::prepare (double sampleRate)
{
    sampleRate_ = (sampleRate > 0.0 && std::isfinite (sampleRate)) ? sampleRate : 44100.0;

    // 20 ms glide so that automating SUSTAIN while a note is held cannot click.
    sustainCoef_ = 1.0 - std::exp (-1.0 / (dsp::kSmoothingSeconds * sampleRate_));

    coeffsValid_ = false;
    recalculate();
    reset();
}

//==============================================================================
void Envelope::reset() noexcept
{
    value_ = 0.0;
    stage_ = Stage::Idle;
}

//==============================================================================
void Envelope::setParams (const Params& p) noexcept
{
    const Params clamped
    {
        clampTime (p.attack,  dsp::kMinEnvTimeSec, dsp::kMaxAttackSec),
        clampTime (p.decay,   dsp::kMinEnvTimeSec, dsp::kMaxDecaySec),
        math::clamp (std::isfinite (p.sustain) ? p.sustain : 0.0f, 0.0f, 1.0f),
        clampTime (p.release, dsp::kMinEnvTimeSec, dsp::kMaxReleaseSec)
    };

    if (coeffsValid_
        && clamped.attack  == params_.attack
        && clamped.decay   == params_.decay
        && clamped.sustain == params_.sustain
        && clamped.release == params_.release)
        return;

    params_ = clamped;
    recalculate();
}

//==============================================================================
void Envelope::recalculate() noexcept
{
    sustain_ = (double) params_.sustain;

    const double sr = sampleRate_;

    attackCoef_  = calcCoef ((double) params_.attack * sr, kAttackRatio);
    attackBase_  = (1.0 + kAttackRatio) * (1.0 - attackCoef_);

    decayCoef_   = calcCoef ((double) params_.decay * sr, kDecayRatio);
    decayBase_   = (sustain_ - kDecayRatio) * (1.0 - decayCoef_);

    releaseCoef_ = calcCoef ((double) params_.release * sr, kDecayRatio);
    releaseBase_ = -kDecayRatio * (1.0 - releaseCoef_);

    coeffsValid_ = true;
}

//==============================================================================
void Envelope::noteOn() noexcept
{
    // Deliberately does NOT touch value_: the new attack picks up from the
    // level the envelope is already at, so retriggering mid-decay, mid-release
    // or mid-attack is continuous.
    stage_ = Stage::Attack;
}

void Envelope::noteOff() noexcept
{
    if (stage_ != Stage::Idle)
        stage_ = Stage::Release;
}

//==============================================================================
float Envelope::processSample() noexcept
{
    switch (stage_)
    {
        case Stage::Idle:
            break;

        case Stage::Attack:
            value_ = attackBase_ + value_ * attackCoef_;
            if (value_ >= 1.0)
            {
                value_ = 1.0;
                stage_ = Stage::Decay;
            }
            break;

        case Stage::Decay:
            value_ = decayBase_ + value_ * decayCoef_;
            if (value_ <= sustain_)
            {
                value_ = sustain_;
                // A zero sustain means the note has finished sounding; going
                // Idle lets the voice be recycled instead of held silent.
                stage_ = (sustain_ <= kIdleLevel) ? Stage::Idle : Stage::Sustain;
            }
            break;

        case Stage::Sustain:
            value_ += (sustain_ - value_) * sustainCoef_;
            break;

        case Stage::Release:
            value_ = releaseBase_ + value_ * releaseCoef_;
            if (value_ <= kIdleLevel)
            {
                value_ = 0.0;
                stage_ = Stage::Idle;
            }
            break;
    }

    if (! (value_ > 0.0))      value_ = 0.0;   // NaN-safe floor
    else if (value_ > 1.0)     value_ = 1.0;

    return (float) value_;
}

//==============================================================================
bool Envelope::isActive() const noexcept          { return stage_ != Stage::Idle; }
Envelope::Stage Envelope::getStage() const noexcept { return stage_; }
float Envelope::getCurrentValue() const noexcept  { return (float) value_; }

} // namespace ripples
