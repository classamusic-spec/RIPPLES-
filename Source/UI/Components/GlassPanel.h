#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "UI/Theme/RippleTheme.h"

namespace ripples
{

//==============================================================================
/**
    The glass container used for major panels only.

    One layer of glass: a dark translucent fill, a subtle cyan edge, a very mild
    internal top highlight and a soft outer shadow. Deliberately quiet — if
    every panel glows, the hierarchy dies.
*/
class GlassPanel : public juce::Component
{
public:
    GlassPanel();
    explicit GlassPanel (const juce::String& title);

    /** Sets the panel's title bar text. Empty removes the title bar entirely. */
    void setTitle (const juce::String& title);

    /** Tints the panel edge and title tick. */
    void setAccent (juce::Colour accent);

    /** Padding between the glass edge and getContentBounds(). */
    void setContentInset (int inset);

    /** The inner area left for content, after the title bar and the inset. */
    juce::Rectangle<int> getContentBounds() const;

    //==========================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Rectangle<int> getGlassBounds() const;

    juce::String panelTitle;
    juce::Colour accentColour { RippleTheme::get().cyan };
    int          contentInset { RippleTheme::get().panelContentInset };

    // Cached on resize so paint() never rebuilds geometry.
    juce::Rectangle<float> glassArea;
    juce::Rectangle<int>   titleArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlassPanel)
};

} // namespace ripples
