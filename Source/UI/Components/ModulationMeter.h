#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "UI/Theme/RippleTheme.h"

namespace ripples
{

//==============================================================================
/**
    A small bipolar activity indicator for one modulation matrix row.

    Centre-zero: the bar grows left for negative values and right for positive
    ones. The displayed value is smoothed toward the value last set, so a
    modulator sampled once per frame glides instead of flickering.

    The smoothing timer only runs while the bar is actually moving *and* the
    component is on screen. Once it settles, or the row scrolls out of view, the
    timer stops and the meter costs nothing.
*/
class ModulationMeter final : public juce::Component,
                              private juce::Timer
{
public:
    ModulationMeter();
    ~ModulationMeter() override;

    //==========================================================================
    // Contract API.
    void setValue (float bipolarValue);   // -1..1
    void setAccent (juce::Colour accent);

    //==========================================================================
    void paint (juce::Graphics& g) override;
    void visibilityChanged() override;
    void parentHierarchyChanged() override;

private:
    //==========================================================================
    void timerCallback() override;
    void updateTimerState();

    //==========================================================================
    float targetValue    = 0.0f;
    float displayedValue = 0.0f;

    juce::Colour accentColour = RippleTheme::get().aqua;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModulationMeter)
};

} // namespace ripples
