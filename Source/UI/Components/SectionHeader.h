#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "UI/Theme/RippleTheme.h"

namespace ripples
{

//==============================================================================
/**
    A small uppercase, widely tracked title with an optional subtitle, in the
    style "TIDE / OSC A". An accent tick sits at the left and a quiet rule runs
    out to the right edge, so a row of sections reads as one system.
*/
class SectionHeader : public juce::Component
{
public:
    explicit SectionHeader (const juce::String& title, const juce::String& subtitle = {});

    void setAccent (juce::Colour accent);
    void setTitle (const juce::String& title);
    void setSubtitle (const juce::String& subtitle);

    void paint (juce::Graphics&) override;

private:
    juce::String titleText;
    juce::String subtitleText;
    juce::Colour accentColour { RippleTheme::get().cyan };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SectionHeader)
};

} // namespace ripples
