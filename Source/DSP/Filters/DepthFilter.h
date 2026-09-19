#pragma once

/*
    RIPPLES — DEPTH filter.

    A zero-delay-feedback (TPT) transistor-ladder core with Oberheim-style stage
    tap mixing, so every response in the contract comes out of one topology and
    one resonance character:

        u  = ladder input after the resonance feedback subtraction
        y1 = 1 / (1 + s)      ... y4 = 1 / (1 + s)^4

        LP12  = y2
        LP24  = y4
        BP12  = 2 (y1 - y2)
        HP12  = u - 2 y1 + y2
        Notch = u - 2 y1 + 2 y2
        Morph = LP24 -> 4(y2 - 2 y3 + y4) -> (u - 4 y1 + 6 y2 - 4 y3 + y4)

    The taps are smoothed per sample, so mode changes and MOVEMENT sweeps are
    click free and the resonant peak never jumps.

    Signal chain per channel:  PRESSURE -> DRIVE -> ladder -> tap mix -> trim.

    Everything after prepare() is allocation free, branch light and bounded by
    construction: the integrator states run through a C1 soft limiter, so even a
    self-oscillating ladder driven with full-scale noise cannot leave a fixed
    amplitude envelope.
*/

#include "Parameters/ParameterEnums.h"

namespace ripples
{

//==============================================================================
class DepthFilter
{
public:
    struct Params
    {
        FilterMode mode  = FilterMode::LP24;
        float cutoffHz   = 1000.0f;
        float resonance  = 0.2f;   // 0..1, self-oscillating near 1
        float drive      = 0.0f;   // 0..1 pre-filter drive
        float movement   = 0.5f;   // 0..1 morph position (Morph mode)
        float pressure   = 0.0f;   // 0..1 density / low-mid weight
    };

    void prepare (double sampleRate);
    void reset() noexcept;
    void setParams (const Params& p) noexcept;
    void setCutoff (float hz) noexcept;          // per-sample modulated cutoff
    void processSample (float& l, float& r) noexcept;

    //== helpers (not part of the contract, safe to ignore) ====================
    float getCutoff() const noexcept          { return cutoffHz_; }
    float getMinCutoff() const noexcept       { return minCutoff_; }
    float getMaxCutoff() const noexcept       { return maxCutoff_; }

private:
    //==========================================================================
    struct ChannelState
    {
        float s1 = 0.0f, s2 = 0.0f, s3 = 0.0f, s4 = 0.0f;   // ladder integrators
        float lowS = 0.0f;                                  // PRESSURE low band
        float env  = 0.0f;                                  // PRESSURE follower
        float dcX1 = 0.0f, dcY1 = 0.0f;                     // DC blocker
    };

    float processOne (float x, ChannelState& c) noexcept;
    void  updateDerived() noexcept;      // per-sample smoothing + derived gains

    //== sample-rate constants ================================================
    double sampleRate_   = 44100.0;
    float  wdScale_      = 0.0f;         // pi / fs
    float  minCutoff_    = 20.0f;
    float  maxCutoff_    = 20000.0f;
    float  smoothCoeff_  = 1.0f;
    float  lowBandG_     = 0.0f;         // TPT gain of the PRESSURE low band
    float  envAtt_       = 1.0f;
    float  envRel_       = 1.0f;
    float  dcR_          = 0.9995f;
    float  gLowHi_       = 1.0f;         // TPT gain at the bass-taming corners
    float  invLowRange_  = 1.0f;

    //== cutoff-derived (updated by setCutoff, cheap) ==========================
    float cutoffHz_  = 1000.0f;
    float G_         = 0.0f;
    float oneMinusG_ = 1.0f;
    float G2_        = 0.0f;
    float G3_        = 0.0f;
    float G4_        = 0.0f;
    float lowness_   = 0.0f;             // 1 at/below 60 Hz, 0 at/above 400 Hz

    //== smoothed control values ==============================================
    FilterMode mode_  = FilterMode::LP24;
    float kTarget_    = 0.0f,  k_        = 0.0f;
    float driveTarget_= 0.0f,  drive_    = 0.0f;
    float trimTarget_ = 1.0f,  trim_     = 1.0f;
    float pressTarget_= 0.0f,  pressure_ = 0.0f;
    float tapTarget_[5] { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
    float tap_[5]       { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };

    //== derived once per sample, shared by both channels ======================
    float kEff_      = 0.0f;
    float inComp_    = 1.0f;
    float driveG_    = 0.0015f;
    float driveInv_  = 1.0f;
    float satG_      = 0.0015f;
    float satInv_    = 1.0f;
    float asymAmt_   = 0.0f;
    float lowGain_   = 0.0f;
    float softenAmt_ = 0.0f;
    float makeup_    = 1.0f;

    ChannelState ch_[2];
};

} // namespace ripples
