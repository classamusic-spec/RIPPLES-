#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace ripples
{

//==============================================================================
/**
    The page selector that sits between the macro strip and the tabbed content.

    Deliberately typographic: widely tracked capitals, a hairline rule across the
    full width and a single bright underline that slides to the selected tab. The
    underline is the only thing that animates, and its timer stops the instant it
    arrives, so a settled interface costs nothing.
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
    int tabAt (juce::Point<int> p) const;

    juce::StringArray   tabs;
    juce::Array<float>  widths;      // measured width of each tab cell
    int   selected = 0;
    int   hovered  = -1;

    float underlineX = 0.0f;
    float underlineW = 0.0f;
    bool  underlineValid = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TabBar)
};

} // namespace ripples
