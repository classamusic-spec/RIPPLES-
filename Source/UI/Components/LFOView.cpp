#include "UI/Components/LFOView.h"

#include "Utilities/MathUtils.h"

#include <cmath>

namespace ripples
{

namespace
{
    //==========================================================================
    // Graph-domain constants: ratios of the plot rectangle or of the theme
    // stroke width. No colour or pixel literals.
    //==========================================================================

    constexpr int   kMinPoints      = 96;
    constexpr int   kMaxPoints      = 512;
    constexpr float kPointsPerPixel = 1.5f;

    constexpr float kAmplitudeRatio = 0.80f;   // of the plot half-height
    constexpr float kSwellRise      = 0.82f;   // Swell spends this much of the cycle rising
    constexpr float kFlowScale      = 0.98f;   // keeps the Flow contour inside -1..1

    constexpr float kStrokeScale    = 1.7f;    // multiples of theme.borderWidth
    constexpr float kGlowWidthInner = 3.0f;    // multiples of the crisp stroke width
    constexpr float kGlowWidthOuter = 6.5f;
    constexpr float kGlowAlphaInner = 0.34f;   // scaled by theme.glowAmount
    constexpr float kGlowAlphaOuter = 0.16f;

    constexpr float kGhostAlpha     = 0.18f;   // full-depth contour behind the curve
    constexpr float kGhostThreshold = 0.97f;   // only drawn when depth is below this

    constexpr float kPlayLineAlpha  = 0.26f;
    constexpr float kPlayDotScale   = 2.4f;    // multiples of theme.borderWidth
    constexpr float kPlayHaloScale  = 4.2f;
    constexpr float kPlayHaloAlpha  = 0.26f;

    constexpr float kRepaintThresholdPx = 0.5f;  // sub-pixel moves are not worth a frame

    /** The sunken well every visualisation sits in. */
    void drawWell (juce::Graphics& g, juce::Rectangle<float> r, const RippleTheme& t)
    {
        g.setColour (t.panelSunken);
        g.fillRoundedRectangle (r, t.controlRadius);

        juce::ColourGradient depthFade (t.backgroundDeep.withMultipliedAlpha (0.65f),
                                        r.getCentreX(), r.getY(),
                                        t.backgroundDeep.withAlpha (0.0f),
                                        r.getCentreX(), r.getBottom(), false);
        g.setGradientFill (depthFade);
        g.fillRoundedRectangle (r, t.controlRadius);

        g.setColour (t.panelBorderSoft);
        g.drawRoundedRectangle (r.reduced (t.borderWidth * 0.5f), t.controlRadius, t.borderWidth);
    }
}

//==============================================================================
LFOView::LFOView()
{
    setOpaque (false);
    setInterceptsMouseClicks (false, false);
}

//==============================================================================
void LFOView::setShape (TideShape shape, float newDepth)
{
    const float d = math::clamp (newDepth, 0.0f, 1.0f);

    if (shape == currentShape && juce::approximatelyEqual (d, depth))
        return;

    currentShape = shape;
    depth        = d;

    rebuildPaths();
    repaint();
}

void LFOView::setPhase (float phase01)
{
    const float p = math::wrapPhase (phase01);

    if (juce::approximatelyEqual (p, phase))
        return;

    phase = p;

    if (plotBounds.getWidth() <= 0.0f)
        return;

    // Shortest distance around the cycle, so a wrap does not count as a jump.
    float delta = std::abs (phase - phasePainted);
    delta = juce::jmin (delta, 1.0f - delta);

    if (delta * plotBounds.getWidth() >= kRepaintThresholdPx)
        repaint();
}

void LFOView::setAccent (juce::Colour accent)
{
    if (accent == accentColour)
        return;

    accentColour = accent;
    repaint();
}

//==============================================================================
void LFOView::resized()
{
    plotBounds = getLocalBounds().toFloat()
                                 .reduced ((float) RippleTheme::xs)
                                 .reduced ((float) RippleTheme::sm);
    rebuildPaths();
}

//==============================================================================
float LFOView::shapeValue (float phase01) const noexcept
{
    using namespace math;

    const float p = wrapPhase (phase01);

    switch (currentShape)
    {
        case TideShape::Sine:
            return std::sin (twoPi * p);

        case TideShape::Triangle:
            return 1.0f - 4.0f * std::abs (wrapPhase (p + 0.25f) - 0.5f);

        case TideShape::Swell:
        {
            // Slow rise, fast fall — a wave gathering and then breaking.
            if (p < kSwellRise)
                return smootherstep (p / kSwellRise) * 2.0f - 1.0f;

            return 1.0f - 2.0f * smootherstep ((p - kSwellRise) / (1.0f - kSwellRise));
        }

        case TideShape::DoubleWave:
        {
            // Two crests per cycle, the second deliberately lower than the
            // first so the two halves are never mistaken for one another.
            const float crests = -std::cos (twoPi * 2.0f * p);
            const float weight = 0.8f + 0.2f * std::sin (twoPi * p);
            return crests * weight;
        }

        case TideShape::Flow:
        {
            // A smoothed, asymmetric contour: one long shallow lift, one deep
            // trough. Built from three phase-offset partials.
            const float v = 0.62f * std::sin (twoPi * p)
                          + 0.28f * std::sin (twoPi * 2.0f * p + 0.9f)
                          + 0.14f * std::sin (twoPi * 3.0f * p + 2.1f);
            return clamp (v * kFlowScale, -1.0f, 1.0f);
        }

        case TideShape::NumShapes:
        default:
            break;
    }

    return std::sin (twoPi * p);
}

float LFOView::yForPhase (float phase01) const noexcept
{
    return plotBounds.getCentreY()
           - math::clamp (shapeValue (phase01) * depth, -1.0f, 1.0f) * amplitude;
}

//==============================================================================
void LFOView::rebuildPaths()
{
    curve.clear();
    ghost.clear();

    if (plotBounds.getWidth() <= 0.0f || plotBounds.getHeight() <= 0.0f)
        return;

    amplitude = plotBounds.getHeight() * 0.5f * kAmplitudeRatio;

    const int numPoints = juce::jlimit (kMinPoints, kMaxPoints,
                                        (int) (plotBounds.getWidth() * kPointsPerPixel));

    const float midY = plotBounds.getCentreY();

    curve.preallocateSpace (numPoints * 3 + 8);
    ghost.preallocateSpace (numPoints * 3 + 8);

    for (int i = 0; i < numPoints; ++i)
    {
        const float p = (float) i / (float) (numPoints - 1);
        const float x = plotBounds.getX() + p * plotBounds.getWidth();
        const float v = math::clamp (shapeValue (p), -1.0f, 1.0f);

        if (i == 0)
        {
            curve.startNewSubPath (x, midY - v * depth * amplitude);
            ghost.startNewSubPath (x, midY - v * amplitude);
        }
        else
        {
            curve.lineTo (x, midY - v * depth * amplitude);
            ghost.lineTo (x, midY - v * amplitude);
        }
    }
}

//==============================================================================
void LFOView::paint (juce::Graphics& g)
{
    const auto& theme = RippleTheme::get();
    const auto  well  = getLocalBounds().toFloat().reduced ((float) RippleTheme::xs);

    if (well.getWidth() <= 0.0f || well.getHeight() <= 0.0f)
        return;

    drawWell (g, well, theme);

    if (curve.isEmpty())
        return;

    juce::Path clipShape;
    clipShape.addRoundedRectangle (well.reduced (theme.borderWidth), theme.controlRadius);

    const juce::Graphics::ScopedSaveState saved (g);
    g.reduceClipRegion (clipShape);

    //--------------------------------------------------------------------------
    // Zero axis.
    g.setColour (theme.panelBorderSoft);
    g.drawLine (plotBounds.getX(), plotBounds.getCentreY(),
                plotBounds.getRight(), plotBounds.getCentreY(), theme.borderWidth);

    const float stroke = theme.borderWidth * kStrokeScale;
    const auto  joint  = juce::PathStrokeType::curved;
    const auto  cap    = juce::PathStrokeType::rounded;

    //--------------------------------------------------------------------------
    // The contour the shape would have at full depth, left as a quiet ghost.
    if (depth < kGhostThreshold && ! ghost.isEmpty())
    {
        g.setColour (theme.cyanDim.withAlpha (kGhostAlpha));
        g.strokePath (ghost, juce::PathStrokeType (theme.borderWidth, joint, cap));
    }

    //--------------------------------------------------------------------------
    // Restrained bloom, then the crisp line.
    g.setColour (accentColour.withAlpha (kGlowAlphaOuter * theme.glowAmount));
    g.strokePath (curve, juce::PathStrokeType (stroke * kGlowWidthOuter, joint, cap));

    g.setColour (accentColour.withAlpha (kGlowAlphaInner * theme.glowAmount));
    g.strokePath (curve, juce::PathStrokeType (stroke * kGlowWidthInner, joint, cap));

    g.setColour (accentColour);
    g.strokePath (curve, juce::PathStrokeType (stroke, joint, cap));

    //--------------------------------------------------------------------------
    // The playhead riding the curve.
    const float x = plotBounds.getX() + phase * plotBounds.getWidth();
    const float y = yForPhase (phase);

    g.setColour (accentColour.withAlpha (kPlayLineAlpha));
    g.drawLine (x, plotBounds.getCentreY(), x, y, theme.borderWidth);

    const float halo = theme.borderWidth * kPlayHaloScale;
    g.setColour (accentColour.withAlpha (kPlayHaloAlpha));
    g.fillEllipse (x - halo, y - halo, halo * 2.0f, halo * 2.0f);

    const float dot = theme.borderWidth * kPlayDotScale;
    g.setColour (theme.primaryText);
    g.fillEllipse (x - dot, y - dot, dot * 2.0f, dot * 2.0f);

    phasePainted = phase;
}

} // namespace ripples
