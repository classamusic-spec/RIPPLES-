#include "UI/Components/ModulationMeter.h"

#include "Utilities/MathUtils.h"

#include <cmath>

namespace ripples
{

namespace
{
    //==========================================================================
    // Graph-domain constants: ratios of the meter rectangle or of the theme
    // stroke width. No colour or pixel literals.
    //==========================================================================

    constexpr float kSettleEpsilon = 0.001f;   // below this the bar is treated as arrived
    constexpr float kTrackHeightRatio = 0.62f; // of the well height
    constexpr float kCentreTickRatio  = 0.74f; // of the well height
    constexpr float kMinBarWidthScale = 1.0f;  // multiples of theme.borderWidth

    constexpr float kFillAlphaInner = 0.35f;   // at the centre of the bar
    constexpr float kCapAlpha        = 0.85f;
    constexpr float kCentreTickAlpha = 0.55f;
    constexpr float kTrackLineAlpha  = 0.80f;  // of panelBorderSoft

    //==========================================================================
    // LUMINOUS TRACE, TURNED DOWN
    //
    // The same bloom / halo / core stack the graphs use, so this belongs to the
    // family — but twelve of these sit in the modulation matrix at once, so the
    // layers are a fraction of the theme widths and the alphas are scaled back.
    // Three rounded rectangles: no blur, no image, nothing cached to go stale.
    //==========================================================================

    constexpr float kMeterTraceScale = 0.20f;  // of the theme trace widths
    constexpr float kQuietAlpha      = 0.55f;  // the family look, turned down

    constexpr float kTintCore  = 0.22f;        // how far each layer leans toward the row accent
    constexpr float kTintGlow  = 0.62f;
    constexpr float kTintBloom = 0.78f;

    /** Leans a trace token toward the row accent without inventing a colour: the
        token keeps its own alpha, only its hue moves. */
    juce::Colour tinted (juce::Colour token, juce::Colour accent, float amount) noexcept
    {
        return token.interpolatedWith (accent.withAlpha (token.getFloatAlpha()), amount);
    }
}

//==============================================================================
ModulationMeter::ModulationMeter()
{
    setOpaque (false);
    setInterceptsMouseClicks (false, false);
}

ModulationMeter::~ModulationMeter()
{
    stopTimer();
}

//==============================================================================
void ModulationMeter::setValue (float bipolarValue)
{
    const float v = math::clamp (bipolarValue, -1.0f, 1.0f);

    if (juce::approximatelyEqual (v, targetValue))
        return;

    targetValue = v;
    updateTimerState();

    // If nothing is animating — settled, or off screen — the value has already
    // been snapped, so make sure the new position is drawn.
    if (! isTimerRunning())
        repaint();
}

void ModulationMeter::setAccent (juce::Colour accent)
{
    if (accent == accentColour)
        return;

    accentColour = accent;
    repaint();
}

//==============================================================================
void ModulationMeter::visibilityChanged()      { updateTimerState(); }
void ModulationMeter::parentHierarchyChanged() { updateTimerState(); }

void ModulationMeter::updateTimerState()
{
    if (! isShowing())
    {
        // Nothing to glide toward while off screen: arrive instantly so the
        // meter is correct the moment it is shown again.
        stopTimer();
        displayedValue = targetValue;
        return;
    }

    if (std::abs (targetValue - displayedValue) > kSettleEpsilon)
    {
        if (! isTimerRunning())
            startTimerHz (juce::jlimit (1, 120, RippleTheme::get().targetFrameRate));
    }
    else
    {
        displayedValue = targetValue;
        stopTimer();
    }
}

void ModulationMeter::timerCallback()
{
    // Safety net: a parent may have been hidden without this component being
    // told. Arrive instantly and stop rather than animating out of sight.
    if (! isShowing())
    {
        displayedValue = targetValue;
        stopTimer();
        repaint();
        return;
    }

    const auto& theme = RippleTheme::get();
    const float fps   = (float) juce::jmax (1, theme.targetFrameRate);
    const float coeff = math::clamp (1.0f - std::exp (-1.0f / juce::jmax (1.0e-3f,
                                                                         theme.valueSmoothSeconds * fps)),
                                     0.0f, 1.0f);

    displayedValue += (targetValue - displayedValue) * coeff;

    if (std::abs (targetValue - displayedValue) <= kSettleEpsilon)
    {
        displayedValue = targetValue;
        stopTimer();
    }

    repaint();
}

//==============================================================================
void ModulationMeter::paint (juce::Graphics& g)
{
    const auto& theme = RippleTheme::get();
    const auto  well  = getLocalBounds().toFloat().reduced (theme.borderWidth);

    if (well.getWidth() <= 0.0f || well.getHeight() <= 0.0f)
        return;

    // Being painted means we are on screen: restart the glide if it was
    // stopped while hidden and the value has since moved.
    if (! isTimerRunning() && std::abs (targetValue - displayedValue) > kSettleEpsilon)
        updateTimerState();

    //--------------------------------------------------------------------------
    // The sunken track.
    const float trackHeight = well.getHeight() * kTrackHeightRatio;
    const auto  track = well.withSizeKeepingCentre (well.getWidth(), trackHeight);
    const float radius = juce::jmin (theme.smallRadius, trackHeight * 0.5f);

    g.setColour (theme.panelSunken);
    g.fillRoundedRectangle (track, radius);

    g.setColour (theme.panelBorderSoft.withMultipliedAlpha (kTrackLineAlpha));
    g.drawRoundedRectangle (track.reduced (theme.borderWidth * 0.5f), radius, theme.borderWidth);

    //--------------------------------------------------------------------------
    // Centre-zero tick. Drawn after the glow below when there is a bar, so the
    // zero mark never disappears under it.
    const float centreX  = well.getCentreX();
    const float tickHalf = well.getHeight() * kCentreTickRatio * 0.5f;

    const auto drawCentreTick = [&]
    {
        g.setColour (theme.cyanDim.withAlpha (kCentreTickAlpha));
        g.drawLine (centreX, well.getCentreY() - tickHalf,
                    centreX, well.getCentreY() + tickHalf, theme.borderWidth);
    };

    //--------------------------------------------------------------------------
    // The bar, growing out of the centre.
    const float value = math::clamp (displayedValue, -1.0f, 1.0f);

    if (std::abs (value) <= kSettleEpsilon)
    {
        drawCentreTick();
        return;
    }

    const float halfSpan = track.getWidth() * 0.5f - theme.borderWidth;
    const float tipX = centreX + value * halfSpan;
    const float barWidth = juce::jmax (theme.borderWidth * kMinBarWidthScale,
                                       std::abs (tipX - centreX));
    const float leftX = juce::jmin (centreX, tipX);

    const auto bar = juce::Rectangle<float> (leftX, track.getY() + theme.borderWidth,
                                             barWidth, track.getHeight() - theme.borderWidth * 2.0f);

    //--------------------------------------------------------------------------
    // Bloom, then halo: two extra rounded rectangles at a fraction of the trace
    // widths. Quiet enough to live in twelve rows at once.
    const float bloomSpread = theme.traceBloomWidth * kMeterTraceScale;
    const float glowSpread  = theme.traceGlowWidth  * kMeterTraceScale;

    // The corner radius is capped so a short bar keeps its ends square-ish
    // instead of swelling into a pill.
    const auto haloRect = [&bar] (float spread)
    {
        return bar.expanded (spread, spread);
    };

    const auto haloRadius = [&bar, radius] (float spread)
    {
        return juce::jmin (radius + spread, (bar.getHeight() + spread * 2.0f) * 0.4f);
    };

    g.setColour (tinted (theme.traceBloom, accentColour, kTintBloom)
                     .withAlpha (theme.traceBloomAlpha * kQuietAlpha));
    g.fillRoundedRectangle (haloRect (bloomSpread), haloRadius (bloomSpread));

    g.setColour (tinted (theme.traceGlow, accentColour, kTintGlow)
                     .withAlpha (theme.traceGlowAlpha * kQuietAlpha));
    g.fillRoundedRectangle (haloRect (glowSpread), haloRadius (glowSpread));

    drawCentreTick();

    //--------------------------------------------------------------------------
    // The bar itself, gathering strength out of the centre.
    const auto barColour = tinted (theme.traceGlow, accentColour, kTintGlow);

    juce::ColourGradient body (barColour.withMultipliedAlpha (kFillAlphaInner),
                               centreX, bar.getCentreY(),
                               barColour, tipX, bar.getCentreY(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (bar, juce::jmin (radius, barWidth * 0.5f));

    // A crisp core cap at the tip so the reading is precise at a glance.
    const float capWidth = juce::jmin (theme.traceCoreWidth, barWidth);
    const float capX = value >= 0.0f ? bar.getRight() - capWidth : bar.getX();

    g.setColour (tinted (theme.traceCore, accentColour, kTintCore).withAlpha (kCapAlpha));
    g.fillRoundedRectangle (juce::Rectangle<float> (capX, bar.getY(), capWidth, bar.getHeight()),
                            juce::jmin (radius, capWidth * 0.5f));
}

} // namespace ripples
