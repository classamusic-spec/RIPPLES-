#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "UI/Theme/RippleTheme.h"

#include <vector>

namespace ripples
{

//==============================================================================
/**
    The ADSR graph.

    The drawn curvature is the same exponential motion the Envelope DSP
    produces — a decelerating attack and -60 dB exponential decay and release —
    so what the player sees is what they hear, not a straight-line cartoon.

    The time axis is logarithmic with a knee near a millisecond, so a 1 ms attack
    and a 10 s attack are both legible in the same window. Each stage is also
    guaranteed a minimum slice of the width, so no stage can ever collapse to
    nothing.

    Nothing here animates by itself: there is no timer, and the paths are rebuilt
    only when the ADSR values or the bounds change. Idle cost is zero.
*/
class EnvelopeView final : public juce::Component
{
public:
    EnvelopeView();
    ~EnvelopeView() override = default;

    //==========================================================================
    // Contract API.
    void setADSR (float attack, float decay, float sustain, float release);  // secs / 0..1
    void setAccent (juce::Colour accent);
    void setPlayhead (float normalisedPosition);   // <0 hides it

    //==========================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    //==========================================================================
    void rebuildPaths();

    /** Curve height (pixels) at a plot x, from the cached sample points. */
    float curveYAt (float x) const noexcept;

    //==========================================================================
    float attackSec  = 0.01f;
    float decaySec   = 0.30f;
    float sustain    = 0.70f;
    float releaseSec = 0.50f;

    juce::Colour accentColour   = RippleTheme::get().cyan;
    float        playhead        = -1.0f;   // <0 = hidden
    float        playheadPainted = -1.0f;   // last position actually drawn

    // Cached geometry.
    juce::Path                      curve, fill;
    std::vector<juce::Point<float>> points;     // x-monotonic, for the playhead lookup
    juce::Rectangle<float>          plotBounds, labelBounds;
    float                           stageX[3] {};   // x at the end of A, D and S
    float                           sustainY = 0.0f;
    bool                            showStageLabels = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnvelopeView)
};

} // namespace ripples
