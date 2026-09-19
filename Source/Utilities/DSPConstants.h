#pragma once

namespace ripples::dsp
{

inline constexpr int    kMaxVoices          = 16;
inline constexpr int    kMaxUnison          = 7;
inline constexpr int    kMaxBlockSize       = 2048;
inline constexpr int    kModBlockSize       = 32;    // control-rate subdivision

inline constexpr float  kMinCutoffHz        = 20.0f;
inline constexpr float  kMaxCutoffHz        = 20000.0f;

inline constexpr float  kMinEnvTimeSec      = 0.001f;
inline constexpr float  kMaxAttackSec       = 10.0f;
inline constexpr float  kMaxDecaySec        = 20.0f;
inline constexpr float  kMaxReleaseSec      = 20.0f;

inline constexpr int    kNumResonatorBanks  = 8;
inline constexpr int    kMaxDroplets        = 24;

inline constexpr float  kMaxDelaySeconds    = 4.0f;
inline constexpr float  kMaxPredelaySeconds = 0.25f;

inline constexpr float  kSmoothingSeconds   = 0.02f;  // generic param smoothing
inline constexpr float  kSlowSmoothingSec   = 0.12f;  // macros / depth-style params

inline constexpr float  kDenormalGuard      = 1.0e-20f;

} // namespace ripples::dsp
