#pragma once

#include <cmath>
#include <algorithm>
#include <cstdint>

/*
    Realtime-safe scalar maths helpers. Everything here is header-only,
    allocation-free and branch-light. No JUCE dependency so the DSP layer can be
    unit tested standalone.
*/

namespace ripples::math
{

inline constexpr float pi      = 3.14159265358979323846f;
inline constexpr float twoPi   = 6.28318530717958647692f;
inline constexpr float halfPi  = 1.57079632679489661923f;

//==============================================================================
template <typename T>
inline constexpr T clamp (T v, T lo, T hi) noexcept { return v < lo ? lo : (v > hi ? hi : v); }

/** Linear interpolation. */
template <typename T>
inline constexpr T lerp (T a, T b, T t) noexcept { return a + (b - a) * t; }

/** Maps v from [inLo,inHi] to [outLo,outHi] without clamping. */
inline constexpr float mapRange (float v, float inLo, float inHi, float outLo, float outHi) noexcept
{
    return outLo + (v - inLo) * (outHi - outLo) / (inHi - inLo);
}

//==============================================================================
/** Flushes denormals and non-finite values to zero. Cheap enough for per-sample use. */
inline float sanitise (float x) noexcept
{
    if (! std::isfinite (x))     return 0.0f;
    if (std::fabs (x) < 1.0e-20f) return 0.0f;
    return x;
}

/** Adds a tiny DC offset to keep feedback paths out of denormal territory. */
inline float antiDenormal (float x) noexcept { return x + 1.0e-18f; }

//==============================================================================
/** Smooth, bounded saturator. Unity slope at zero, asymptotic to +/-1. */
inline float softClip (float x) noexcept
{
    return std::tanh (x);
}

/** Cheaper tanh approximation — accurate to ~1e-3 over [-3,3]. */
inline float fastTanh (float x) noexcept
{
    const float x2 = x * x;
    if (x2 > 9.0f) return x > 0.0f ? 1.0f : -1.0f;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

/** Asymmetric tube-ish shaper — adds even harmonics, used by PRESSURE. */
inline float asymSaturate (float x, float asym) noexcept
{
    const float b = x + asym * x * x;
    return fastTanh (b);
}

//==============================================================================
/** Equal-power pan. pan in [-1,1]; writes gains for L and R. */
inline void equalPowerPan (float pan, float& lGain, float& rGain) noexcept
{
    const float p = clamp (pan, -1.0f, 1.0f) * 0.5f + 0.5f;   // 0..1
    const float a = p * halfPi;
    lGain = std::cos (a);
    rGain = std::sin (a);
}

/** Equal-power dry/wet crossfade coefficients. */
inline void equalPowerMix (float mix, float& dryGain, float& wetGain) noexcept
{
    const float m = clamp (mix, 0.0f, 1.0f) * halfPi;
    dryGain = std::cos (m);
    wetGain = std::sin (m);
}

//==============================================================================
inline float decibelsToGain (float dB) noexcept
{
    return dB <= -100.0f ? 0.0f : std::pow (10.0f, dB * 0.05f);
}

inline float gainToDecibels (float gain) noexcept
{
    return gain <= 1.0e-5f ? -100.0f : 20.0f * std::log10 (gain);
}

//==============================================================================
/** MIDI note (with fractional cents) to Hz. */
inline float noteToHz (float midiNote) noexcept
{
    return 440.0f * std::pow (2.0f, (midiNote - 69.0f) / 12.0f);
}

inline float hzToNote (float hz) noexcept
{
    return 69.0f + 12.0f * std::log2 (std::max (hz, 1.0e-6f) / 440.0f);
}

/** Semitone offset as a pitch ratio. */
inline float semitonesToRatio (float semis) noexcept
{
    return std::pow (2.0f, semis / 12.0f);
}

//==============================================================================
/** One-pole coefficient for a given time constant. */
inline float onePoleCoeff (float timeSeconds, double sampleRate) noexcept
{
    if (timeSeconds <= 0.0f) return 1.0f;
    return 1.0f - std::exp (-1.0f / (timeSeconds * (float) sampleRate));
}

/** Exponential decay coefficient reaching -60dB after timeSeconds. */
inline float decayCoeff (float timeSeconds, double sampleRate) noexcept
{
    if (timeSeconds <= 0.0f) return 0.0f;
    return std::exp (-6.907755f / (timeSeconds * (float) sampleRate));
}

//==============================================================================
/** Perceptually even frequency skew: 0..1 -> 20Hz..20kHz. */
inline float normToFreq (float norm, float lo = 20.0f, float hi = 20000.0f) noexcept
{
    return lo * std::pow (hi / lo, clamp (norm, 0.0f, 1.0f));
}

inline float freqToNorm (float hz, float lo = 20.0f, float hi = 20000.0f) noexcept
{
    return std::log (clamp (hz, lo, hi) / lo) / std::log (hi / lo);
}

//==============================================================================
/** Smoothstep / smootherstep easing, for macro curves and UI motion. */
inline constexpr float smoothstep (float t) noexcept
{
    t = clamp (t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline constexpr float smootherstep (float t) noexcept
{
    t = clamp (t, 0.0f, 1.0f);
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

//==============================================================================
/** PolyBLEP residual for band-limiting discontinuous oscillators.
    @param t    normalised phase in [0,1)
    @param dt   phase increment per sample
*/
inline float polyBlep (float t, float dt) noexcept
{
    if (dt <= 0.0f) return 0.0f;

    if (t < dt)                 // just after a step
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt)          // just before a step
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

/** Integrated polyBLEP (polyBLAMP) for band-limiting slope discontinuities
    such as the corners of a triangle wave. */
inline float polyBlamp (float t, float dt) noexcept
{
    if (dt <= 0.0f) return 0.0f;

    if (t < dt)
    {
        const float x = t / dt - 1.0f;
        return -1.0f / 3.0f * x * x * x;
    }
    if (t > 1.0f - dt)
    {
        const float x = (t - 1.0f) / dt + 1.0f;
        return 1.0f / 3.0f * x * x * x;
    }
    return 0.0f;
}

//==============================================================================
/** Wraps phase into [0,1). */
inline float wrapPhase (float p) noexcept
{
    p -= std::floor (p);
    return p < 0.0f ? p + 1.0f : p;
}

} // namespace ripples::math
