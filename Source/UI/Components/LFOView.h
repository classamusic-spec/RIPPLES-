#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Parameters/ParameterEnums.h"
#include "UI/Theme/RippleTheme.h"

namespace ripples
{

//==============================================================================
/**
    Draws the selected modulator's shape with a moving phase indicator.

    Every TideShape has its own generator — a sine, a triangle, the slow-rise
    fast-fall Swell, the two unequal crests of DoubleWave and the smoothed
    asymmetric Flow contour — so the display tells the player what the SHAPE
    control actually does. `depth` scales the drawn amplitude, and a dimmer rear
    wave — the same shape at full depth, trimmed and shifted a little in phase —
    sits behind it. That second crest is what reads as motion, and it doubles as
    the reference against which a reduced depth is legible.

    The component owns no timer: setPhase() is driven from whatever timer the
    owning view already runs, and a move smaller than a pixel does not even ask
    for a repaint. The curve itself is cached and rebuilt only when the shape,
    the depth or the bounds change.
*/
class LFOView final : public juce::Component
{
public:
    LFOView();
    ~LFOView() override = default;

    //==========================================================================
    // Contract API.
    void setShape (TideShape shape, float depth);
    void setPhase (float phase01);
    void setAccent (juce::Colour accent);

    //==========================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    //==========================================================================
    void rebuildPaths();

    /** The shape generator: normalised phase in [0,1) -> -1..1. */
    float shapeValue (float phase01) const noexcept;

    float yForPhase (float phase01) const noexcept;

    //==========================================================================
    TideShape currentShape = TideShape::Sine;
    float     depth        = 1.0f;
    float     phase        = 0.0f;
    float     phasePainted = 0.0f;

    juce::Colour accentColour = RippleTheme::get().aqua;

    // Cached geometry. The stroked ribbons for both waves are built alongside
    // the curves, so a frame is a handful of fills of cached geometry — nothing
    // is stroked, allocated or blurred in paint().
    juce::Path             curve, ghost;
    juce::Path             coreStroke, glowStroke, bloomStroke;
    juce::Path             ghostCoreStroke, ghostGlowStroke;
    juce::Path             wellClip;
    juce::Rectangle<float> plotBounds;
    float                  amplitude = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LFOView)
};

} // namespace ripples
