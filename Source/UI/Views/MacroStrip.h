#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "UI/Components/GlassPanel.h"
#include "UI/Components/RippleKnob.h"

#include <array>
#include <memory>

namespace ripples
{

//==============================================================================
/**
    The eight primary macros, in one uninterrupted strip.

    DEPTH, WET, RIPPLE, CURRENT, DROPS, PRESSURE, SPACE and GLOW are the controls
    a player reaches for first, so they get the largest knobs in the instrument
    and the most generous internal padding. The "FLOW TRANSFORMS SOUND" caption
    closes the strip on the right, and is the first thing dropped when the window
    gets narrow — it is decoration, the knobs are not.
*/
class MacroStrip final : public GlassPanel
{
public:
    explicit MacroStrip (juce::AudioProcessorValueTreeState& apvts);
    ~MacroStrip() override;

    void resized() override;
    void paintOverChildren (juce::Graphics&) override;

private:
    static constexpr int kNumMacros = 8;

    std::array<std::unique_ptr<RippleKnob>, kNumMacros> knobs;

    juce::Rectangle<int> captionArea;   // empty when the caption is suppressed

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MacroStrip)
};

} // namespace ripples
