#pragma once

/*
    RIPPLES — factory presets.

    These are compiled-in C++ data. There is no file IO and no JSON/XML parsing
    at startup: the table is built once, lazily, into a function-local static
    the first time it is asked for, and handed out by const reference after
    that. Loading a factory preset is a walk over a std::map on the message
    thread — never on the audio thread.

    Banks:  THE SURFACE, SHALLOW WATER, DEEP BLUE, THE ABYSS,
            DROPLETS, CURRENTS, BIOLUMINESCENCE, STORMS
*/

#include "Presets/PresetManager.h"

#include <vector>

namespace ripples::factory
{

/** The whole factory bank, in browsing order. */
const std::vector<Preset>& getPresets();

/** Index of SUBMERGED DREAMS — the preset the plug-in opens with. */
int getDefaultPresetIndex();

/** Index of INIT DEEP SAW. */
int getInitPresetIndex();

/** Case-insensitive lookup; -1 when absent. */
int indexOfPreset (const juce::String& name);

/** Bank names in display order. */
juce::StringArray getBankNames();

/** Category names in display order. */
juce::StringArray getCategoryNames();

/** The full tag vocabulary. */
juce::StringArray getTagVocabulary();

//==============================================================================
/** Converts a zero-based CHOICE INDEX into the normalised 0..1 value that a
    juce::AudioParameterChoice expects.

    juce::AudioParameterChoice is built on NormalisableRange<float> (0,
    numChoices - 1, 1), so its convertTo0to1 is simply

        normalised = index / (numChoices - 1)

    and convertFrom0to1 rounds back to the nearest integer index. Getting this
    wrong is silent — you do not get an error, you get the wrong waveform — so
    every enum in a factory preset goes through this one function, with the
    enum's own NumXxx sentinel as numChoices.

    Worked example: OscWave has 9 entries (NumWaves == 9), so
    OscWave::Water (index 8) is 8 / 8 = 1.0, and OscWave::Saw (index 2) is
    2 / 8 = 0.25. A single-choice parameter maps to 0. */
inline float choiceToNormalised (int index, int numChoices) noexcept
{
    if (numChoices <= 1)
        return 0.0f;

    return juce::jlimit (0.0f, 1.0f, (float) index / (float) (numChoices - 1));
}

/** Inverse of choiceToNormalised — the nearest choice index. */
inline int normalisedToChoice (float normalised, int numChoices) noexcept
{
    if (numChoices <= 1)
        return 0;

    return juce::jlimit (0, numChoices - 1,
                         (int) std::lround (juce::jlimit (0.0f, 1.0f, normalised)
                                            * (float) (numChoices - 1)));
}

} // namespace ripples::factory
