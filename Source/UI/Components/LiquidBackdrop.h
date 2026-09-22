#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Utilities/VisualizationState.h"

namespace ripples
{

//==============================================================================
/**
    The flowing water behind everything.

    A full-window layer of slowly drifting caustics, rendered procedurally at
    low resolution and blitted up, that sits behind every panel. The frosted
    glass panels are translucent, so this is what is seen THROUGH them — the
    whole interface reads as sitting in a moving body of water rather than on a
    flat dark sheet.

    It runs on its own timer at a reduced rate: the motion is deliberately
    calm, so a full 60 fps buys nothing, and a full-window repaint is not cheap
    once the translucent panels above have to be recomposited over it. The
    surface breathes very gently with the sound through VisualizationState.
*/
class LiquidBackdrop final : public juce::Component,
                             private juce::Timer
{
public:
    explicit LiquidBackdrop (VisualizationState& vis);
    ~LiquidBackdrop() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;
    void parentHierarchyChanged() override;

private:
    void timerCallback() override;
    void updateTimerState();
    void renderInto (juce::Image& image) const;

    VisualizationState& visual;

    juce::Image field;              // low-res caustic field, blitted up
    float phase    = 0.0f;          // wrapped drift phase
    float warp     = 0.0f;          // second, slower drift for the domain warp
    float glowSmoothed = 0.35f;
    float rmsSmoothed  = 0.0f;
    double lastTickMs  = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiquidBackdrop)
};

} // namespace ripples
