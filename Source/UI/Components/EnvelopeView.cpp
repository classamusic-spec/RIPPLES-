#include "UI/Components/EnvelopeView.h"

#include "Utilities/DSPConstants.h"
#include "Utilities/MathUtils.h"

#include <algorithm>
#include <cmath>

namespace ripples
{

namespace
{
    //==========================================================================
    // Graph-domain constants: ratios of the plot rectangle, of the theme stroke
    // width, or times in seconds. No colour or pixel literals — every screen
    // value comes from a RippleTheme token.
    //==========================================================================

    constexpr float kTimeKnee          = 0.02f;   // seconds; where the time axis stops being linear
    constexpr float kMinStageFraction  = 0.08f;   // guaranteed slice of the timed width per stage
    constexpr float kSustainFraction   = 0.18f;   // share of the plot width held at sustain
    constexpr float kMaxTimeSec        = 20.0f;

    // Exponential curvature, matching the Envelope DSP.
    constexpr float kAttackCurve  = 3.0f;   // decelerating rise
    constexpr float kDecayCurve   = 4.6f;   // ~ -40 dB across the segment
    constexpr float kReleaseCurve = 4.6f;

    constexpr int   kStagePoints    = 44;
    constexpr float kAmplitudeRatio = 0.92f;  // of the plot height

    constexpr float kFillAlphaTop   = 1.00f;  // multiples of the traceFill token alpha
    constexpr float kFillAlphaMid   = 0.42f;
    constexpr float kFillMidStop    = 0.55f;

    constexpr float kGuideAlpha       = 0.55f;   // of panelBorderSoft
    constexpr float kPlayheadLineAlpha = 0.34f;
    constexpr float kPlayheadDotRatio  = 1.25f;  // multiples of the core trace width
    constexpr float kPlayheadHaloRatio = 2.30f;
    constexpr float kPlayheadHaloAlpha = 0.30f;
    constexpr float kRepaintThresholdPx = 0.5f;  // sub-pixel playhead moves are not worth a frame

    //==========================================================================
    // LUMINOUS TRACE
    //
    // A trace is three strokes of one piece of geometry: a very wide, very faint
    // bloom, a mid halo, then the crisp core on top. The stroked outlines are
    // built once, when the envelope or the bounds change, and paint() only fills
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
    constexpr float kTintFill  = 0.70f;

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

    /** log time compression — turns a duration into an axis length. */
    inline float timeToAxis (float seconds) noexcept
    {
        return std::log (1.0f + math::clamp (seconds, 0.0f, kMaxTimeSec) / kTimeKnee);
    }

    /** The sunken well every visualisation sits in. */
    void drawWell (juce::Graphics& g, juce::Rectangle<float> r, const RippleTheme& t)
    {
        g.setColour (t.panelSunken);
        g.fillRoundedRectangle (r, t.controlRadius);

        juce::ColourGradient depth (t.backgroundDeep.withMultipliedAlpha (0.65f),
                                    r.getCentreX(), r.getY(),
                                    t.backgroundDeep.withAlpha (0.0f),
                                    r.getCentreX(), r.getBottom(), false);
        g.setGradientFill (depth);
        g.fillRoundedRectangle (r, t.controlRadius);

        g.setColour (t.panelBorderSoft);
        g.drawRoundedRectangle (r.reduced (t.borderWidth * 0.5f), t.controlRadius, t.borderWidth);
    }
}

//==============================================================================
EnvelopeView::EnvelopeView()
{
    setOpaque (false);
    setInterceptsMouseClicks (false, false);
}

//==============================================================================
void EnvelopeView::setADSR (float attack, float decay, float sus, float release)
{
    const float a = math::clamp (attack,  0.0f, kMaxTimeSec);
    const float d = math::clamp (decay,   0.0f, kMaxTimeSec);
    const float s = math::clamp (sus,     0.0f, 1.0f);
    const float r = math::clamp (release, 0.0f, kMaxTimeSec);

    if (juce::approximatelyEqual (a, attackSec)  && juce::approximatelyEqual (d, decaySec)
        && juce::approximatelyEqual (s, sustain) && juce::approximatelyEqual (r, releaseSec))
        return;

    attackSec  = a;
    decaySec   = d;
    sustain    = s;
    releaseSec = r;

    rebuildPaths();
    repaint();
}

void EnvelopeView::setAccent (juce::Colour accent)
{
    if (accent == accentColour)
        return;

    accentColour = accent;
    repaint();
}

void EnvelopeView::setPlayhead (float normalisedPosition)
{
    const float v = normalisedPosition < 0.0f ? -1.0f
                                              : math::clamp (normalisedPosition, 0.0f, 1.0f);

    if (juce::approximatelyEqual (v, playhead))
        return;

    const bool wasHidden = playhead < 0.0f;
    const bool nowHidden = v < 0.0f;
    playhead = v;

    if (wasHidden != nowHidden || plotBounds.getWidth() <= 0.0f)
    {
        repaint();
        return;
    }

    // Only spend a frame once the dot has actually moved a visible amount.
    if (std::abs (v - playheadPainted) * plotBounds.getWidth() >= kRepaintThresholdPx)
        repaint();
}

//==============================================================================
void EnvelopeView::resized()
{
    auto inner = getLocalBounds().toFloat()
                                 .reduced ((float) RippleTheme::xs)
                                 .reduced ((float) RippleTheme::sm);

    showStageLabels = getHeight() >= RippleTheme::grid (14);

    if (showStageLabels)
        labelBounds = inner.removeFromBottom ((float) RippleTheme::md);
    else
        labelBounds = {};

    plotBounds = inner;

    // The well outline, cached so the bloom can spill against the rounded
    // corners without a Path being built inside paint().
    const auto& theme = RippleTheme::get();
    const auto  well  = getLocalBounds().toFloat().reduced ((float) RippleTheme::xs);

    wellClip.clear();

    if (well.getWidth() > 0.0f && well.getHeight() > 0.0f)
        wellClip.addRoundedRectangle (well.reduced (theme.borderWidth), theme.controlRadius);

    rebuildPaths();
}

//==============================================================================
void EnvelopeView::rebuildPaths()
{
    curve.clear();
    fill.clear();
    points.clear();
    coreStroke.clear();
    glowStroke.clear();
    bloomStroke.clear();

    if (plotBounds.getWidth() <= 0.0f || plotBounds.getHeight() <= 0.0f)
        return;

    //--------------------------------------------------------------------------
    // Non-linear time axis. Attack, decay and release share the width in
    // proportion to their compressed durations; each keeps a legible minimum.
    const float rawA = timeToAxis (attackSec);
    const float rawD = timeToAxis (decaySec);
    const float rawR = timeToAxis (releaseSec);
    const float sum  = rawA + rawD + rawR;

    float fracA = 1.0f / 3.0f, fracD = fracA, fracR = fracA;

    if (sum > 0.0f)
    {
        fracA = rawA / sum;
        fracD = rawD / sum;
        fracR = rawR / sum;
    }

    const float span = 1.0f - 3.0f * kMinStageFraction;
    fracA = kMinStageFraction + span * fracA;
    fracD = kMinStageFraction + span * fracD;
    fracR = kMinStageFraction + span * fracR;

    const float timedWidth = plotBounds.getWidth() * (1.0f - kSustainFraction);
    const float widthA = timedWidth * fracA;
    const float widthD = timedWidth * fracD;
    const float widthS = plotBounds.getWidth() * kSustainFraction;
    const float widthR = timedWidth * fracR;

    const float left   = plotBounds.getX();
    const float bottom = plotBounds.getBottom();
    const float height = plotBounds.getHeight() * kAmplitudeRatio;

    stageX[0] = left + widthA;
    stageX[1] = stageX[0] + widthD;
    stageX[2] = stageX[1] + widthS;
    sustainY  = bottom - sustain * height;

    const auto addPoint = [this, bottom, height] (float x, float level)
    {
        points.push_back ({ x, bottom - math::clamp (level, 0.0f, 1.0f) * height });
    };

    points.reserve ((size_t) (kStagePoints * 3 + 4));

    //--------------------------------------------------------------------------
    // Attack: 1 - exp(-k u), normalised so it lands exactly on 1.
    {
        const float endValue = 1.0f - std::exp (-kAttackCurve);

        for (int i = 0; i < kStagePoints; ++i)
        {
            const float u = (float) i / (float) (kStagePoints - 1);
            addPoint (left + widthA * u, (1.0f - std::exp (-kAttackCurve * u)) / endValue);
        }
    }

    // Decay: exponential fall from 1 to the sustain level.
    {
        const float tail = std::exp (-kDecayCurve);

        for (int i = 1; i < kStagePoints; ++i)
        {
            const float u = (float) i / (float) (kStagePoints - 1);
            const float e = (std::exp (-kDecayCurve * u) - tail) / (1.0f - tail);
            addPoint (stageX[0] + widthD * u, sustain + (1.0f - sustain) * e);
        }
    }

    // Sustain: held.
    addPoint (stageX[2], sustain);

    // Release: exponential fall from the sustain level to silence.
    {
        const float tail = std::exp (-kReleaseCurve);

        for (int i = 1; i < kStagePoints; ++i)
        {
            const float u = (float) i / (float) (kStagePoints - 1);
            const float e = (std::exp (-kReleaseCurve * u) - tail) / (1.0f - tail);
            addPoint (stageX[2] + widthR * u, sustain * e);
        }
    }

    //--------------------------------------------------------------------------
    curve.preallocateSpace ((int) points.size() * 3 + 8);

    for (size_t i = 0; i < points.size(); ++i)
    {
        if (i == 0) curve.startNewSubPath (points[i]);
        else        curve.lineTo (points[i]);
    }

    fill = curve;
    fill.lineTo (points.back().x, bottom);
    fill.lineTo (points.front().x, bottom);
    fill.closeSubPath();

    // The ribbons are stroked here, once, and only filled from paint().
    buildTrace (curve, traceWidthsFor (RippleTheme::get(), plotBounds.getHeight()),
                coreStroke, glowStroke, bloomStroke);
}

//==============================================================================
float EnvelopeView::curveYAt (float x) const noexcept
{
    if (points.empty())
        return plotBounds.getBottom();

    if (x <= points.front().x) return points.front().y;
    if (x >= points.back().x)  return points.back().y;

    const auto it = std::lower_bound (points.begin(), points.end(), x,
                                      [] (const juce::Point<float>& p, float v) { return p.x < v; });

    if (it == points.begin())
        return it->y;

    const auto& b = *it;
    const auto& a = *(it - 1);
    const float dx = b.x - a.x;

    return dx > 0.0f ? a.y + (b.y - a.y) * ((x - a.x) / dx) : b.y;
}

//==============================================================================
void EnvelopeView::paint (juce::Graphics& g)
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
    // Quiet guides: the sustain level and the stage boundaries.
    g.setColour (theme.panelBorderSoft.withMultipliedAlpha (kGuideAlpha));
    g.drawLine (plotBounds.getX(), sustainY, plotBounds.getRight(), sustainY, theme.borderWidth);

    for (const float x : stageX)
        g.drawLine (x, plotBounds.getY(), x, plotBounds.getBottom(), theme.borderWidth);

    //--------------------------------------------------------------------------
    // Soft wash under the envelope, fading to nothing at the floor.
    const auto wash = tinted (theme.traceFill, accentColour, kTintFill);

    juce::ColourGradient body (wash.withMultipliedAlpha (kFillAlphaTop),
                               plotBounds.getCentreX(), plotBounds.getY(),
                               wash.withAlpha (0.0f),
                               plotBounds.getCentreX(), plotBounds.getBottom(), false);
    body.addColour (kFillMidStop, wash.withMultipliedAlpha (kFillAlphaMid));
    g.setGradientFill (body);
    g.fillPath (fill);

    //--------------------------------------------------------------------------
    // The envelope itself: bloom, halo, crisp core.
    paintTrace (g, theme, accentColour, coreStroke, glowStroke, bloomStroke);

    //--------------------------------------------------------------------------
    if (showStageLabels && ! labelBounds.isEmpty())
    {
        g.setColour (theme.tertiaryText);
        g.setFont (theme.smallFont());

        const float edges[5] { plotBounds.getX(), stageX[0], stageX[1], stageX[2],
                               plotBounds.getRight() };
        const char* names[4] { "A", "D", "S", "R" };

        for (int i = 0; i < 4; ++i)
            g.drawText (names[i],
                        juce::Rectangle<float> (edges[i], labelBounds.getY(),
                                                edges[i + 1] - edges[i], labelBounds.getHeight()),
                        juce::Justification::centred, false);
    }

    //--------------------------------------------------------------------------
    if (playhead >= 0.0f)
    {
        const auto  widths = traceWidthsFor (theme, plotBounds.getHeight());
        const float x = plotBounds.getX() + playhead * plotBounds.getWidth();
        const float y = curveYAt (x);

        g.setColour (tinted (theme.traceGlow, accentColour, kTintGlow)
                         .withAlpha (kPlayheadLineAlpha));
        g.drawLine (x, plotBounds.getY(), x, plotBounds.getBottom(), theme.borderWidth);

        const float halo = widths.core * kPlayheadHaloRatio;
        g.setColour (tinted (theme.traceGlow, accentColour, kTintGlow)
                         .withAlpha (kPlayheadHaloAlpha));
        g.fillEllipse (x - halo, y - halo, halo * 2.0f, halo * 2.0f);

        const float dot = widths.core * kPlayheadDotRatio;
        g.setColour (tinted (theme.traceCore, accentColour, kTintCore));
        g.fillEllipse (x - dot, y - dot, dot * 2.0f, dot * 2.0f);
    }

    playheadPainted = playhead;
}

} // namespace ripples
