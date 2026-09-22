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
    constexpr int   kMaxPoints      = 384;
    constexpr float kPointsPerPixel = 1.0f;

    constexpr float kAmplitudeRatio = 0.84f;   // of the plot half-height
    constexpr float kSwellRise      = 0.82f;   // Swell spends this much of the cycle rising
    constexpr float kFlowScale      = 0.98f;   // keeps the Flow contour inside -1..1

    // The rear wave: the same shape at full depth, trimmed in amplitude and
    // shifted in phase, so the two crests overlap rather than coincide.
    constexpr float kGhostAmpRatio    = 0.82f;   // of the full-depth amplitude
    constexpr float kGhostPhaseOffset = 0.07f;   // cycles
    constexpr float kGhostHaloAlpha   = 0.60f;   // multiples of the traceGhost token alpha
    constexpr float kGhostCoreAlpha   = 1.00f;
    constexpr float kTintGhost        = 0.35f;

    constexpr float kPlayLineAlpha  = 0.30f;
    constexpr float kPlayDotRatio   = 1.25f;   // multiples of the core trace width
    constexpr float kPlayHaloRatio  = 2.40f;
    constexpr float kPlayHaloAlpha  = 0.30f;

    constexpr float kRepaintThresholdPx = 0.5f;  // sub-pixel moves are not worth a frame

    //==========================================================================
    // LUMINOUS TRACE
    //
    // A trace is three strokes of one piece of geometry: a very wide, very faint
    // bloom, a mid halo, then the crisp core on top. The stroked outlines are
    // built once, when the curve or the bounds change, and paint() only fills
    // them — so a frame is three fills of cached geometry, with no blur, no
    // DropShadow and no image anywhere.
    //==========================================================================

    constexpr float kTraceRefHeight = 46.0f;   // plot height the theme widths are drawn for
    constexpr float kTraceScaleMin  = 0.55f;   // a short graph must not be swallowed by its glow
    constexpr float kTraceScaleMax  = 1.75f;   // ... and a 2560-wide editor keeps its proportions
    constexpr float kMinGlowRatio   = 2.0f;    // the halo never collapses onto the core
    constexpr float kMinBloomRatio  = 4.2f;

    constexpr float kTintCore  = 0.22f;        // how far each layer leans toward the panel accent
    constexpr float kTintGlow  = 0.62f;
    constexpr float kTintBloom = 0.78f;

    struct TraceWidths { float core, glow, bloom; };

    /** Theme trace widths, scaled to the height of the graph they belong to. */
    TraceWidths traceWidthsFor (const RippleTheme& t, float plotHeight) noexcept
    {
        const float s    = juce::jlimit (kTraceScaleMin, kTraceScaleMax, plotHeight / kTraceRefHeight);
        const float core = t.traceCoreWidth * juce::jmax (1.0f, s);

        return { core,
                 juce::jmax (t.traceGlowWidth  * s, core * kMinGlowRatio),
                 juce::jmax (t.traceBloomWidth * s, core * kMinBloomRatio) };
    }

    /** Leans a trace token toward a panel accent without inventing a colour: the
        token keeps its own alpha, only its hue moves. */
    juce::Colour tinted (juce::Colour token, juce::Colour accent, float amount) noexcept
    {
        return token.interpolatedWith (accent.withAlpha (token.getFloatAlpha()), amount);
    }

    /** Strokes one ribbon into a cached outline. From a rebuild, never paint(). */
    void buildRibbon (juce::Path& dest, const juce::Path& source, float width)
    {
        dest.clear();

        if (source.isEmpty() || width <= 0.0f)
            return;

        juce::PathStrokeType (width, juce::PathStrokeType::curved,
                              juce::PathStrokeType::rounded).createStrokedPath (dest, source);
    }

    /** Builds the three stroked ribbons for a curve. */
    void buildTrace (const juce::Path& source, TraceWidths w,
                     juce::Path& core, juce::Path& glow, juce::Path& bloom)
    {
        buildRibbon (bloom, source, w.bloom);
        buildRibbon (glow,  source, w.glow);
        buildRibbon (core,  source, w.core);
    }

    /** Fills a prepared trace back to front: bloom, halo, core. */
    void paintTrace (juce::Graphics& g, const RippleTheme& t, juce::Colour accent,
                     const juce::Path& core, const juce::Path& glow, const juce::Path& bloom,
                     juce::AffineTransform transform = {})
    {
        g.setColour (tinted (t.traceBloom, accent, kTintBloom).withAlpha (t.traceBloomAlpha));
        g.fillPath (bloom, transform);

        g.setColour (tinted (t.traceGlow, accent, kTintGlow).withAlpha (t.traceGlowAlpha));
        g.fillPath (glow, transform);

        g.setColour (tinted (t.traceCore, accent, kTintCore));
        g.fillPath (core, transform);
    }

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
    const auto& theme = RippleTheme::get();

    plotBounds = getLocalBounds().toFloat()
                                 .reduced ((float) RippleTheme::xs)
                                 .reduced ((float) RippleTheme::sm);

    // The well outline, cached so the bloom can spill against the rounded
    // corners without a Path being built inside paint().
    const auto well = getLocalBounds().toFloat().reduced ((float) RippleTheme::xs);

    wellClip.clear();

    if (well.getWidth() > 0.0f && well.getHeight() > 0.0f)
        wellClip.addRoundedRectangle (well.reduced (theme.borderWidth), theme.controlRadius);

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
    coreStroke.clear();
    glowStroke.clear();
    bloomStroke.clear();
    ghostCoreStroke.clear();
    ghostGlowStroke.clear();

    if (plotBounds.getWidth() <= 0.0f || plotBounds.getHeight() <= 0.0f)
        return;

    amplitude = plotBounds.getHeight() * 0.5f * kAmplitudeRatio;

    const int numPoints = juce::jlimit (kMinPoints, kMaxPoints,
                                        (int) (plotBounds.getWidth() * kPointsPerPixel));

    const float midY      = plotBounds.getCentreY();
    const float ghostAmp  = amplitude * kGhostAmpRatio;

    curve.preallocateSpace (numPoints * 3 + 8);
    ghost.preallocateSpace (numPoints * 3 + 8);

    for (int i = 0; i < numPoints; ++i)
    {
        const float p = (float) i / (float) (numPoints - 1);
        const float x = plotBounds.getX() + p * plotBounds.getWidth();
        const float v = math::clamp (shapeValue (p), -1.0f, 1.0f);

        // The rear wave runs ahead in phase, so the two crests overlap instead
        // of tracing one another.
        const float gv = math::clamp (shapeValue (p + kGhostPhaseOffset), -1.0f, 1.0f);

        if (i == 0)
        {
            curve.startNewSubPath (x, midY - v * depth * amplitude);
            ghost.startNewSubPath (x, midY - gv * ghostAmp);
        }
        else
        {
            curve.lineTo (x, midY - v * depth * amplitude);
            ghost.lineTo (x, midY - gv * ghostAmp);
        }
    }

    // The ribbons are stroked here, once, and only filled from paint(). The rear
    // wave gets the halo and the core, never the bloom: it is depth, not a
    // second subject.
    const auto widths = traceWidthsFor (RippleTheme::get(), plotBounds.getHeight());

    buildTrace (curve, widths, coreStroke, glowStroke, bloomStroke);

    buildRibbon (ghostGlowStroke, ghost, widths.glow);
    buildRibbon (ghostCoreStroke, ghost, widths.core);
}

//==============================================================================
void LFOView::paint (juce::Graphics& g)
{
    const auto& theme = RippleTheme::get();
    const auto  well  = getLocalBounds().toFloat().reduced ((float) RippleTheme::xs);

    if (well.getWidth() <= 0.0f || well.getHeight() <= 0.0f)
        return;

    drawWell (g, well, theme);

    if (coreStroke.isEmpty() || wellClip.isEmpty())
        return;

    const juce::Graphics::ScopedSaveState saved (g);
    g.reduceClipRegion (wellClip);

    //--------------------------------------------------------------------------
    // Zero axis.
    g.setColour (theme.panelBorderSoft);
    g.drawLine (plotBounds.getX(), plotBounds.getCentreY(),
                plotBounds.getRight(), plotBounds.getCentreY(), theme.borderWidth);

    //--------------------------------------------------------------------------
    // The rear wave: full depth, trimmed and shifted, two dim layers only.
    if (! ghostCoreStroke.isEmpty())
    {
        const auto ghostColour = tinted (theme.traceGhost, accentColour, kTintGhost);

        g.setColour (ghostColour.withMultipliedAlpha (kGhostHaloAlpha));
        g.fillPath (ghostGlowStroke);

        g.setColour (ghostColour.withMultipliedAlpha (kGhostCoreAlpha));
        g.fillPath (ghostCoreStroke);
    }

    //--------------------------------------------------------------------------
    // The modulator itself: bloom, halo, crisp core.
    paintTrace (g, theme, accentColour, coreStroke, glowStroke, bloomStroke);

    //--------------------------------------------------------------------------
    // The playhead riding the curve, lit from the same palette as the trace.
    const auto  widths = traceWidthsFor (theme, plotBounds.getHeight());
    const float x = plotBounds.getX() + phase * plotBounds.getWidth();
    const float y = yForPhase (phase);

    g.setColour (tinted (theme.traceGlow, accentColour, kTintGlow).withAlpha (kPlayLineAlpha));
    g.drawLine (x, plotBounds.getCentreY(), x, y, theme.borderWidth);

    const float halo = widths.core * kPlayHaloRatio;
    g.setColour (tinted (theme.traceGlow, accentColour, kTintGlow).withAlpha (kPlayHaloAlpha));
    g.fillEllipse (x - halo, y - halo, halo * 2.0f, halo * 2.0f);

    const float dot = widths.core * kPlayDotRatio;
    g.setColour (tinted (theme.traceCore, accentColour, kTintCore));
    g.fillEllipse (x - dot, y - dot, dot * 2.0f, dot * 2.0f);

    phasePainted = phase;
}

} // namespace ripples
