#include "DSP/Filters/DepthFilter.h"

#include "Utilities/MathUtils.h"
#include "Utilities/DSPConstants.h"

#include <algorithm>
#include <cmath>

namespace ripples
{

namespace
{
    //== tuning constants =====================================================
    constexpr float kMaxFeedback   = 4.55f;   // ladder self-oscillates at k = 4
    constexpr float kMinCutoffRel  = 0.45f;   // highest cutoff as a fraction of fs

    // Integrator-state ceiling. Transparent below the knee, asymptotic above
    // it, and applied as ONE common gain across all four states — see the note
    // at the call site for why that matters to the tuning.
    constexpr float kStateKnee     = 3.0f;
    constexpr float kStateRange    = 3.0f;    // asymptote = knee + range
    constexpr float kInputKnee     = 6.0f;
    constexpr float kInputRange    = 4.0f;
    constexpr float kOutputKnee    = 2.5f;
    constexpr float kOutputRange   = 2.5f;

    // Resonance bass compensation: the ladder's DC gain is 1/(1+k), so some of
    // it is fed back in. More of it at low cutoffs, where the loss is the
    // difference between a bass patch and silence.
    constexpr float kBassCompBase  = 0.36f;
    constexpr float kBassCompLow   = 0.40f;
    constexpr float kLowTame       = 0.07f;   // resonance reduction at 20 Hz
    constexpr float kTrimPerK      = 0.22f;

    // PRESSURE
    constexpr float kPressLowGain  = 1.35f;   // low-mid weight
    constexpr float kPressSoften   = 1.80f;   // transient softening depth
    constexpr float kPressSatG     = 2.20f;   // density saturation hardness
    constexpr float kPressAsym     = 0.16f;   // even-harmonic tilt
    constexpr float kPressMakeup   = 2.40f;
    constexpr float kMinSatG       = 0.0015f; // "off" hardness -> linear

    // DRIVE
    constexpr float kMaxDriveG     = 9.0f;
    constexpr float kDriveMakeup   = 1.80f;

    //=========================================================================
    /** C1-continuous soft limiter: identity for |x| <= knee, asymptotic to
        knee + range. One divide, no branch on the hot path worth worrying
        about, and guaranteed bounded output for any finite input. */
    inline float softLimit (float x, float knee, float range) noexcept
    {
        const float a = std::fabs (x);
        if (a <= knee)
            return x;

        const float over = a - knee;
        const float y    = knee + range * over / (over + range);
        return x < 0.0f ? -y : y;
    }

    /** Returns the TPT one-pole gain G = g / (1 + g) with g = tan(wd).

        tan is evaluated with its [5/4] Pade approximant, which is accurate to
        better than 1e-6 over the whole usable range (wd <= 1.42, i.e. cutoff up
        to 0.45 fs) and costs one divide — the same divide that G needs anyway,
        because g/(1+g) = N/(D+N). Accuracy matters here: a sloppy prewarp puts
        the resonant peak out of tune, which is audible on a self-oscillating
        filter played as a sine source. */
    inline float tptGain (float wd) noexcept
    {
        const float x2 = wd * wd;
        const float x4 = x2 * x2;
        const float n  = wd * (945.0f - 105.0f * x2 + x4);
        const float d  = 945.0f - 420.0f * x2 + 15.0f * x4;
        return n / (d + n);
    }

    //=========================================================================
    // Oberheim-style ladder taps over (u, y1, y2, y3, y4).
    constexpr float kTapLP12 [5] { 0.0f,  0.0f,  1.0f,  0.0f, 0.0f };
    constexpr float kTapLP24 [5] { 0.0f,  0.0f,  0.0f,  0.0f, 1.0f };
    constexpr float kTapBP12 [5] { 0.0f,  2.0f, -2.0f,  0.0f, 0.0f };
    constexpr float kTapHP12 [5] { 1.0f, -2.0f,  1.0f,  0.0f, 0.0f };
    constexpr float kTapNotch[5] { 1.0f, -2.0f,  2.0f,  0.0f, 0.0f };
    constexpr float kTapBP24 [5] { 0.0f,  0.0f,  4.0f, -8.0f, 4.0f };
    constexpr float kTapHP24 [5] { 1.0f, -4.0f,  6.0f, -4.0f, 1.0f };

    inline void copyTaps (const float* src, float* dst) noexcept
    {
        for (int i = 0; i < 5; ++i)
            dst[i] = src[i];
    }

    /** MOVEMENT sweeps LP24 -> BP24 -> HP24 with an equal-power crossfade, so
        the perceived level stays put while the character travels. The taps are
        additionally smoothed per sample by the caller. */
    void computeTaps (FilterMode mode, float movement, float* dst) noexcept
    {
        switch (mode)
        {
            case FilterMode::LP12:  copyTaps (kTapLP12,  dst); return;
            case FilterMode::LP24:  copyTaps (kTapLP24,  dst); return;
            case FilterMode::BP12:  copyTaps (kTapBP12,  dst); return;
            case FilterMode::HP12:  copyTaps (kTapHP12,  dst); return;
            case FilterMode::Notch: copyTaps (kTapNotch, dst); return;
            case FilterMode::Morph:
            case FilterMode::NumModes:
            default:
                break;
        }

        const float m = math::clamp (movement, 0.0f, 1.0f);

        const float* a = nullptr;
        const float* b = nullptr;
        float t = 0.0f;

        if (m < 0.5f) { a = kTapLP24; b = kTapBP24; t = m * 2.0f; }
        else          { a = kTapBP24; b = kTapHP24; t = (m - 0.5f) * 2.0f; }

        const float angle = t * math::halfPi;
        const float wa    = std::cos (angle);
        const float wb    = std::sin (angle);

        for (int i = 0; i < 5; ++i)
            dst[i] = wa * a[i] + wb * b[i];
    }
} // namespace

//==============================================================================
void DepthFilter::prepare (double sampleRate)
{
    sampleRate_ = (sampleRate > 0.0 && std::isfinite (sampleRate)) ? sampleRate : 44100.0;

    const float fs = (float) sampleRate_;
    wdScale_ = math::pi / fs;

    minCutoff_ = dsp::kMinCutoffHz;
    maxCutoff_ = kMinCutoffRel * fs;
    if (maxCutoff_ > dsp::kMaxCutoffHz) maxCutoff_ = dsp::kMaxCutoffHz;
    if (maxCutoff_ < minCutoff_ * 2.0f) maxCutoff_ = minCutoff_ * 2.0f;

    smoothCoeff_ = math::onePoleCoeff (dsp::kSmoothingSeconds, sampleRate_);

    // Bass-taming window expressed in G so setCutoff stays divide-light.
    gLowHi_ = tptGain (math::clamp (400.0f, minCutoff_, maxCutoff_) * wdScale_);
    const float gLowLo = tptGain (math::clamp (60.0f, minCutoff_, maxCutoff_) * wdScale_);
    invLowRange_ = 1.0f / std::max (gLowHi_ - gLowLo, 1.0e-9f);

    lowBandG_ = tptGain (math::clamp (280.0f, minCutoff_, maxCutoff_) * wdScale_);
    envAtt_   = math::onePoleCoeff (0.0004f, sampleRate_);
    envRel_   = math::onePoleCoeff (0.0800f, sampleRate_);
    dcR_      = 1.0f - math::twoPi * 5.0f / fs;
    if (dcR_ < 0.0f)  dcR_ = 0.0f;
    if (dcR_ > 0.9999f) dcR_ = 0.9999f;

    reset();
    setCutoff (cutoffHz_);
}

//==============================================================================
void DepthFilter::reset() noexcept
{
    for (auto& c : ch_)
        c = ChannelState{};

    // Snap every smoother so a reset never produces a ramp into the first note.
    k_        = kTarget_;
    drive_    = driveTarget_;
    trim_     = trimTarget_;
    pressure_ = pressTarget_;
    copyTaps (tapTarget_, tap_);

    updateDerived();
}

//==============================================================================
void DepthFilter::setParams (const Params& p) noexcept
{
    mode_ = (p.mode >= FilterMode::LP12 && p.mode < FilterMode::NumModes)
                ? p.mode : FilterMode::LP24;

    float res = p.resonance;
    if (! (res > 0.0f)) res = 0.0f;              // also catches NaN
    if (res > 1.0f)     res = 1.0f;
    kTarget_ = kMaxFeedback * res;

    float dr = p.drive;
    if (! (dr > 0.0f)) dr = 0.0f;
    if (dr > 1.0f)     dr = 1.0f;
    driveTarget_ = dr;

    float pr = p.pressure;
    if (! (pr > 0.0f)) pr = 0.0f;
    if (pr > 1.0f)     pr = 1.0f;
    pressTarget_ = pr;

    // Output trim keeps high resonance from simply being louder. Computed here
    // (control rate) so the audio path never divides for it.
    trimTarget_ = 1.0f / (1.0f + kTrimPerK * kTarget_);

    float mv = p.movement;
    if (! (mv > 0.0f)) mv = 0.0f;
    if (mv > 1.0f)     mv = 1.0f;
    computeTaps (mode_, mv, tapTarget_);

    setCutoff (p.cutoffHz);
}

//==============================================================================
void DepthFilter::setCutoff (float hz) noexcept
{
    if (! (hz > minCutoff_)) hz = minCutoff_;    // NaN-safe
    if (hz > maxCutoff_)     hz = maxCutoff_;
    cutoffHz_ = hz;

    const float G = tptGain (hz * wdScale_);
    G_         = G;
    oneMinusG_ = 1.0f - G;
    G2_        = G * G;
    G3_        = G2_ * G;
    G4_        = G2_ * G2_;

    // How deep into the bass we are, 0..1, used to trade resonance for weight.
    const float ln = (gLowHi_ - G) * invLowRange_;
    lowness_ = ln <= 0.0f ? 0.0f : (ln >= 1.0f ? 1.0f : ln);
}

//==============================================================================
void DepthFilter::updateDerived() noexcept
{
    // Resonance is eased off in the sub-bass so a resonant bass patch keeps its
    // fundamental, and the input gain compensates the ladder's 1/(1+k) DC loss
    // — harder the lower the cutoff sits.
    kEff_   = k_ * (1.0f - kLowTame * lowness_);
    inComp_ = 1.0f + (kBassCompBase + kBassCompLow * lowness_) * kEff_;

    // DRIVE: y = tanh(g x) / g. g -> 0 is exactly linear, so drive = 0 is a
    // bypass; raising g adds saturation at constant small-signal gain, which is
    // the level compensation the brief asks for.
    driveG_   = kMinSatG + drive_ * drive_ * kMaxDriveG;
    driveInv_ = (1.0f + kDriveMakeup * drive_ * drive_ * drive_) / driveG_;

    // PRESSURE: low-mid weight, transient softening, asymmetric density.
    const float p = pressure_;
    lowGain_   = kPressLowGain * p;
    softenAmt_ = kPressSoften * p;
    satG_      = kMinSatG + kPressSatG * p * p;
    satInv_    = 1.0f / satG_;
    asymAmt_   = kPressAsym * p * satG_;   // scaled so it vanishes with satG_
    makeup_    = 1.0f + kPressMakeup * p;
}

//==============================================================================
float DepthFilter::processOne (float x, ChannelState& c) noexcept
{
    //---------------------------------------------------------------- PRESSURE
    {
        // Low band (TPT one-pole at 280 Hz) -> low-mid weight.
        const float v  = (x - c.lowS) * lowBandG_;
        const float lo = v + c.lowS;
        c.lowS = math::antiDenormal (lo + v);

        float w = x + lowGain_ * lo;

        // Fast-attack / slow-release follower -> transient softening.
        const float mag = std::fabs (w);
        c.env += (mag > c.env ? envAtt_ : envRel_) * (mag - c.env);
        c.env  = math::antiDenormal (c.env);
        w *= 1.0f / (1.0f + softenAmt_ * c.env);

        // Asymmetric shaping applied after the bounded tanh, so it can never
        // fold back: d/dw is (1 - t^2)(1 + 2 a t) with |a| < 0.5.
        float t = math::fastTanh (w * satG_);
        t += asymAmt_ * (t * t - 0.33333333f);
        w  = t * satInv_ * makeup_;

        // The asymmetry leaves a DC term; a 5 Hz blocker takes it away.
        const float y = w - c.dcX1 + dcR_ * c.dcY1;
        c.dcX1 = w;
        c.dcY1 = math::antiDenormal (y);
        x = y;
    }

    //------------------------------------------------------------------- DRIVE
    x = math::fastTanh (x * driveG_) * driveInv_;

    // Resonance bass compensation + a hard bound on what can enter the ladder.
    x = softLimit (x * inComp_, kInputKnee, kInputRange);

    //------------------------------------------------- zero-delay-feedback core
    // Each stage is y = G x + s (1 - G); solving the loop for y4 in closed form
    // is what keeps the resonance in tune instead of one sample late.
    const float S1 = c.s1 * oneMinusG_;
    const float S2 = c.s2 * oneMinusG_;
    const float S3 = c.s3 * oneMinusG_;
    const float S4 = c.s4 * oneMinusG_;

    const float sFb = G3_ * S1 + G2_ * S2 + G_ * S3 + S4;
    const float y4  = (G4_ * x + sFb) / (1.0f + kEff_ * G4_);
    const float u   = x - kEff_ * y4;

    float v  = (u  - c.s1) * G_;
    const float y1 = v + c.s1;
    c.s1 = y1 + v;

    v = (y1 - c.s2) * G_;
    const float y2 = v + c.s2;
    c.s2 = y2 + v;

    v = (y2 - c.s3) * G_;
    const float y3 = v + c.s3;
    c.s3 = y3 + v;

    v = (y3 - c.s4) * G_;
    const float y4b = v + c.s4;
    c.s4 = y4b + v;

    // Bounded, denormal-free state. This is what gives self-oscillation a stable
    // amplitude instead of an exponential blow-up once k passes 4.
    //
    // The four integrators are scaled by ONE common gain rather than limited
    // individually. Scaling the whole state vector only scales the linear
    // system's trajectory, so the limit cycle keeps the shape and the frequency
    // the linear ladder would have had: a self-oscillating DEPTH stays in tune.
    // Limiting each state on its own instead acts like per-stage leakage and
    // pulls the oscillation ~3 % sharp, which is half a semitone of detune when
    // the filter is being played as a sine source.
    c.s1 = math::antiDenormal (c.s1);
    c.s2 = math::antiDenormal (c.s2);
    c.s3 = math::antiDenormal (c.s3);
    c.s4 = math::antiDenormal (c.s4);

    const float sMax = std::max (std::max (std::fabs (c.s1), std::fabs (c.s2)),
                                 std::max (std::fabs (c.s3), std::fabs (c.s4)));
    if (sMax > kStateKnee)
    {
        const float scale = softLimit (sMax, kStateKnee, kStateRange) / sMax;
        c.s1 *= scale;
        c.s2 *= scale;
        c.s3 *= scale;
        c.s4 *= scale;
    }

    //---------------------------------------------------------------- tap mix
    const float out = tap_[0] * u
                    + tap_[1] * y1
                    + tap_[2] * y2
                    + tap_[3] * y3
                    + tap_[4] * y4b;

    return softLimit (out * trim_, kOutputKnee, kOutputRange);
}

//==============================================================================
void DepthFilter::processSample (float& l, float& r) noexcept
{
    const float a = smoothCoeff_;

    k_        += (kTarget_     - k_)        * a;
    drive_    += (driveTarget_ - drive_)    * a;
    trim_     += (trimTarget_  - trim_)     * a;
    pressure_ += (pressTarget_ - pressure_) * a;

    for (int i = 0; i < 5; ++i)
        tap_[i] += (tapTarget_[i] - tap_[i]) * a;

    updateDerived();

    const float outL = processOne (math::sanitise (l), ch_[0]);
    const float outR = processOne (math::sanitise (r), ch_[1]);

    l = math::sanitise (outL);
    r = math::sanitise (outR);
}

} // namespace ripples
