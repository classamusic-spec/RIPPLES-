#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace ripples
{

//==============================================================================
/**
    The page selector that sits between the macro strip and the tabbed content.

    Deliberately typographic: widely tracked capitals and a hairline rule across
    the full width. The selected page is a filled rounded pill behind its label;
    every other tab is plain dim text with no decoration at all. The pill glides
    between tabs rather than jumping, and it is the only thing that animates —
    its timer stops the instant it arrives and whenever the bar is hidden, so a
    settled interface costs nothing.
*/
class TabBar final : public juce::Component,
                     private juce::Timer
{
public:
    TabBar();
    ~TabBar() override;

    /** Replaces the tab names. Selection is clamped into the new range. */
    void setTabs (const juce::StringArray& names);

    void setSelectedTab (int index, bool sendNotification = true);
    int  getSelectedTab() const noexcept { return selected; }

    /** Height this bar would like, given the current density. */
    int getPreferredHeight() const;

    std::function<void (int)> onTabChanged;

    //==========================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void visibilityChanged() override;
    void parentHierarchyChanged() override;

private:
    //==========================================================================
    void timerCallback() override;
    void updateTimerState();
    void recomputeTabWidths();
    juce::Rectangle<float> tabBounds (int index) const;

    /** Where the pill wants to be for a given tab: horizontal extent only. */
    juce::Rectangle<float> pillTargetFor (int index) const;

    /** The pill as it should be drawn right now, inside the given text strip. */
    juce::Rectangle<float> pillArea (juce::Rectangle<float> strip) const;

    int tabAt (juce::Point<int> p) const;

    juce::StringArray   tabs;
    juce::Array<float>  widths;      // measured width of each tab cell
    int   selected = 0;
    int   hovered  = -1;

    // The animated pill, in horizontal terms; its height comes from the bar.
    float pillX = 0.0f;
    float pillW = 0.0f;
    bool  pillValid = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TabBar)
};

} // namespace ripples
