#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "UI/Components/GlassPanel.h"
#include "UI/Components/RippleKnob.h"
#include "UI/Components/SectionHeader.h"
#include "Utilities/VisualizationState.h"

#include <memory>

namespace ripples
{

//==============================================================================
/**
    The MODULATION page: the twelve matrix slots, plus the depth of the two
    FLUID FIELD axes.

    Every row is source, destination, amount, polarity and a live meter, so the
    matrix reads as a patchbay rather than a table of numbers. The rows live in a
    viewport: with enough width they fall into two columns and all twelve are on
    screen at once, and when the window is small they simply scroll — no row is
    ever dropped.

    The meters share one timer, which stops whenever the page is hidden.
*/
class ModulationPage final : public juce::Component,
                             private juce::Timer
{
public:
    ModulationPage (juce::AudioProcessorValueTreeState& apvts, VisualizationState& vis);
    ~ModulationPage() override;

    void resized() override;
    void visibilityChanged() override;
    void parentHierarchyChanged() override;

private:
    //==========================================================================
    class MatrixContent;

    void timerCallback() override;
    void updateTimerState();

    GlassPanel     matrixPanel;
    juce::Viewport viewport;
    std::unique_ptr<MatrixContent> matrix;

    GlassPanel    fluidPanel;
    SectionHeader fluidHeader { "FLUID FIELD", "AXIS DEPTH" };
    std::unique_ptr<RippleKnob> fluidXAmount, fluidYAmount;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModulationPage)
};

} // namespace ripples
