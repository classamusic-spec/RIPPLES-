#pragma once

/*
    RIPPLES — APVTS parameter layout.

    Builds every automatable parameter declared in ParameterIDs.h, with musical
    ranges, skews, units and text conversions. The defaults together form the
    INIT DEEP SAW reference patch: two detuned saws, a quiet sine sub an octave
    down, an LP24 filter at a moderate cutoff with a moderate filter envelope,
    and every "water" extra (droplets, resonator, global effects, modulation
    matrix) parked at zero so the raw voice can be judged on its own.

    Nothing in here may invent a parameter string — every ID comes from
    ripples::pid.
*/

#include <juce_audio_processors/juce_audio_processors.h>

namespace ripples
{

//==============================================================================
/** Version hint given to every juce::ParameterID. */
inline constexpr int kParameterVersionHint = 1;

/** Level-style dB ranges run from kMinLevelDb (treated as silence) to kMaxLevelDb. */
inline constexpr float kMinLevelDb = -60.0f;
inline constexpr float kMaxLevelDb = 6.0f;

/** Master output / ceiling dB bounds. */
inline constexpr float kMinOutputDb = -24.0f;
inline constexpr float kMaxOutputDb = 12.0f;

//==============================================================================
/** Builds the complete RIPPLES parameter layout. Message thread only. */
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace ripples
