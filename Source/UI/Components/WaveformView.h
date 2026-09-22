#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Parameters/ParameterEnums.h"
#include "UI/Theme/RippleTheme.h"

#include <vector>

namespace ripples
{

//==============================================================================
/**
    Draws the actual shape of the selected oscillator wave.

    Every one of the nine OscWave members is rendered from its own generator, and
    the `shape` parameter genuinely changes the drawn contour — this display is
    how the player learns what the SHAPE control does, so it must never fall back
    to a generic sine.

    The curve is built once into a cached juce::Path spanning a little more than
    the visible window, so the slow ambient drift is applied as an affine
    translation at paint time instead of rebuilding geometry every frame. Only
    the Water wave has genuine internal motion, and even that rebuilds at a
    fraction of the frame rate.

    When setAnimated (false) is in force, or the component is off screen, the
    internal timer is stopped outright: idle CPU is zero.
*/
class WaveformView final : public juce::Component,
                           private juce::Timer
{
public:
    WaveformView();
    ~WaveformView() override;

    //==========================================================================
    // Contract API.
    void setWave (OscWave wave, float shape);
    void setAccent (juce::Colour accent);
    void setAnimated (bool shouldAnimate);

    //==========================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;
    void parentHierarchyChanged() override;

private:
    //==========================================================================
    void timerCallback() override;
    void updateTimerState();
    void rebuildPath();

    /** The wave generator: normalised phase in [0,1) -> roughly -1..1. */
    float waveSample (float phase01) const noexcept;

    /** True for waves whose contour genuinely changes over time (Water). */
    bool waveMovesInternally() const noexcept;

    //==========================================================================
    OscWave      currentWave  = OscWave::Sine;
    float        currentShape = 0.5f;
    juce::Colour accentColour = RippleTheme::get().cyan;
    bool         animated     = false;

    // Slow ambient motion. Phases are kept wrapped into [0,1) so they stay
    // exact no matter how long the editor is left open.
    float driftPhase    = 0.0f;
    float morphPhase[3] { 0.0f, 0.33f, 0.66f };
    int   framesSinceRebuild = 0;

    // Cached geometry. The three stroked ribbons that make up the luminous
    // trace are built alongside the curve, so a frame is three fills of cached
    // geometry — nothing is stroked, allocated or blurred in paint().
    juce::Path             curve;
    juce::Path             coreStroke, glowStroke, bloomStroke;
    juce::Path             wellClip;
    juce::Rectangle<float> plotBounds;
    float                  pixelsPerCycle = 0.0f;
    std::vector<float>     scratch;          // reused sample buffer, never sized in paint()

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformView)
};

} // namespace ripples
