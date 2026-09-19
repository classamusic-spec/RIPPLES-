#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "UI/Theme/RippleTheme.h"

namespace ripples
{

//==============================================================================
/**
    Flat by default — a dark well with a thin border — or a filled accent
    button for the one primary action in a group. Hover and press are subtle
    shifts in fill, never a bounce.
*/
class RippleButton : public juce::TextButton
{
public:
    explicit RippleButton (const juce::String& text = {});

    void setAccent (juce::Colour accent);
    void setIsPrimary (bool shouldBePrimary);

    bool isPrimary() const noexcept { return primary; }

    void paintButton (juce::Graphics&, bool shouldDrawButtonAsHighlighted,
                      bool shouldDrawButtonAsDown) override;

private:
    juce::Colour accentColour { RippleTheme::get().cyan };
    bool primary = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RippleButton)
};

} // namespace ripples
