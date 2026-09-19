#include "UI/Components/FilterResponseView.h"

#include "Utilities/DSPConstants.h"
#include "Utilities/MathUtils.h"

#include <cmath>

namespace ripples
{

namespace
{
    //==========================================================================
    // Graph-domain constants: decibels, hertz, and ratios of the plot rectangle
    // or of the theme stroke width. No colour or pixel literals.
    //==========================================================================

    constexpr float kMinDb    = -48.0f;
    constexpr float kMaxDb    =  24.0f;
    constexpr float kDbGrid   =  12.0f;

    constexpr float kMinHz    = dsp::kMinCutoffHz;
    constexpr float kMaxHz    = dsp::kMaxCutoffHz;

    constexpr int   kMinPoints = 96;
    constexpr int   kMaxPoints = 512;
    constexpr float kPointsPerPixel = 1.5f;

    // Resonance mapping. Both prototypes are tuned to peak at about +21 dB when
    // resonance is fully up, which keeps the curve inside the dB window instead
    // of flattening against the top of it.
    constexpr float kQMin       = 0.7071f;   // Butterworth, no resonance
    constexpr float kQRange     = 17.0f;     // Q = kQMin * kQRange^resonance
    constexpr float kLadderMax  = 3.45f;     // ladder feedback; self-oscillates at 4

    constexpr float kStrokeScale    = 1.7f;   // multiples of theme.borderWidth
    constexpr float kGlowWidthInner = 3.0f;   // multiples of the crisp stroke width
    constexpr float kGlowWidthOuter = 6.5f;
    constexpr float kGlowAlphaInner = 0.34f;  // scaled by theme.glowAmount
    constexpr float kGlowAlphaOuter = 0.16f;

    constexpr float kFillAlphaTop = 0.26f;
    constexpr float kFillAlphaMid = 0.09f;
    constexpr float kFillMidStop  = 0.55f;

    constexpr float kGridMinorAlpha = 0.55f;   // of panelBorderSoft
    constexpr float kGridMajorAlpha = 1.0f;
    constexpr float kZeroLineAlpha  = 0.22f;   // of cyanDim
    constexpr float kCutoffLineAlpha = 0.26f;
    constexpr float kCutoffDotScale = 2.2f;    // multiples of theme.borderWidth
    constexpr float kCutoffHaloScale = 3.6f;
    constexpr float kCutoffHaloAlpha = 0.26f;

    struct GridLine { float hz; bool major; const char* label; };

    constexpr GridLine kFrequencyGrid[]
    {
        {    30.0f, false, nullptr },
        {    50.0f, false, nullptr },
        {   100.0f, true,  "100"   },
        {   200.0f, false, nullptr },
        {   500.0f, false, nullptr },
        {  1000.0f, true,  "1k"    },
        {  2000.0f, false, nullptr },
        {  5000.0f, false, nullptr },
        { 10000.0f, true,  "10k"   }
    };

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
FilterResponseView::FilterResponseView()
{
    setOpaque (false);
    setInterceptsMouseClicks (false, false);
}

//==============================================================================
void FilterResponseView::setFilter (FilterMode newMode, float newCutoffHz, float newResonance)
{
    const float hz  = math::clamp (newCutoffHz, kMinHz, kMaxHz);
    const float res = math::clamp (newResonance, 0.0f, 1.0f);

    if (newMode == mode && juce::approximatelyEqual (hz, cutoffHz)
        && juce::approximatelyEqual (res, resonance))
        return;

    mode      = newMode;
    cutoffHz  = hz;
    resonance = res;

    rebuildPaths();
    repaint();
}

void FilterResponseView::setAccent (juce::Colour accent)
{
    if (accent == accentColour)
        return;

    accentColour = accent;
    repaint();
}

void FilterResponseView::setMorphPosition (float position01)
{
    const float p = math::clamp (position01, 0.0f, 1.0f);

    if (juce::approximatelyEqual (p, morphPos))
        return;

    morphPos = p;

    if (mode == FilterMode::Morph)
    {
        rebuildPaths();
        repaint();
    }
}

//==============================================================================
void FilterResponseView::resized()
{
    auto inner = getLocalBounds().toFloat()
                                 .reduced ((float) RippleTheme::xs)
                                 .reduced ((float) RippleTheme::sm);

    showAxisLabels = getHeight() >= RippleTheme::grid (16)
                  && getWidth()  >= RippleTheme::grid (30);

    if (showAxisLabels)
        labelBounds = inner.removeFromBottom ((float) RippleTheme::md);
    else
        labelBounds = {};

    plotBounds = inner;
    rebuildPaths();
}

//==============================================================================
float FilterResponseView::xForFrequency (float hz) const noexcept
{
    return plotBounds.getX() + math::freqToNorm (hz, kMinHz, kMaxHz) * plotBounds.getWidth();
}

float FilterResponseView::yForDecibels (float dB) const noexcept
{
    const float t = (math::clamp (dB, kMinDb, kMaxDb) - kMinDb) / (kMaxDb - kMinDb);
    return plotBounds.getBottom() - t * plotBounds.getHeight();
}

//==============================================================================
std::complex<float> FilterResponseView::responseAt (float freqHz) const noexcept
{
    // Everything is evaluated on the analogue prototype with the cutoff
    // normalised to 1, so the shape is exact and free of any digital warping.
    const std::complex<float> s { 0.0f, freqHz / cutoffHz };
    const std::complex<float> one { 1.0f, 0.0f };

    if (mode == FilterMode::LP24)
    {
        // Four-pole ladder: H(s) = (1 + k) / ((1 + s)^4 + k).
        // The numerator normalises DC gain to unity; k is the feedback that
        // produces the resonant peak and would self-oscillate at 4.
        const float k  = kLadderMax * resonance;
        const auto  a  = one + s;
        const auto  a2 = a * a;
        const auto  a4 = a2 * a2;
        return (1.0f + k) / (a4 + k);
    }

    // Two-pole state-variable prototype: D(s) = s^2 + s/Q + 1.
    const float q = kQMin * std::pow (kQRange, resonance);
    const auto  d = s * s + s / q + one;

    const auto lp = one / d;
    const auto bp = s / d;               // TPT band-pass: peaks at Q, like the DSP
    const auto hp = (s * s) / d;

    switch (mode)
    {
        case FilterMode::LP12:  return lp;
        case FilterMode::BP12:  return bp;
        case FilterMode::HP12:  return hp;
        case FilterMode::Notch: return (s * s + one) / d;

        case FilterMode::Morph:
        {
            // Continuous LP -> BP -> HP, crossfaded as complex responses so the
            // blend keeps its phase relationship, exactly as an SVF morph does.
            if (morphPos < 0.5f)
            {
                const float t = morphPos * 2.0f;
                return lp * (1.0f - t) + bp * t;
            }

            const float t = (morphPos - 0.5f) * 2.0f;
            return bp * (1.0f - t) + hp * t;
        }

        case FilterMode::LP24:
        case FilterMode::NumModes:
        default:
            break;
    }

    return lp;
}

//==============================================================================
void FilterResponseView::rebuildPaths()
{
    curve.clear();
    fill.clear();

    if (plotBounds.getWidth() <= 0.0f || plotBounds.getHeight() <= 0.0f)
        return;

    const int numPoints = juce::jlimit (kMinPoints, kMaxPoints,
                                        (int) (plotBounds.getWidth() * kPointsPerPixel));

    curve.preallocateSpace (numPoints * 3 + 8);

    for (int i = 0; i < numPoints; ++i)
    {
        const float norm = (float) i / (float) (numPoints - 1);
        const float hz   = math::normToFreq (norm, kMinHz, kMaxHz);
        const float mag  = std::abs (responseAt (hz));
        const float dB   = mag > 1.0e-6f ? 20.0f * std::log10 (mag) : kMinDb;

        const float x = plotBounds.getX() + norm * plotBounds.getWidth();
        const float y = yForDecibels (dB);

        if (i == 0) curve.startNewSubPath (x, y);
        else        curve.lineTo (x, y);
    }

    fill = curve;
    fill.lineTo (plotBounds.getRight(), plotBounds.getBottom());
    fill.lineTo (plotBounds.getX(), plotBounds.getBottom());
    fill.closeSubPath();

    const float peakMag = std::abs (responseAt (cutoffHz));
    cutoffX = xForFrequency (cutoffHz);
    cutoffY = yForDecibels (peakMag > 1.0e-6f ? 20.0f * std::log10 (peakMag) : kMinDb);
}

//==============================================================================
void FilterResponseView::paint (juce::Graphics& g)
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
    // Faint grid: decades and octaves across, 12 dB steps up.
    for (const auto& line : kFrequencyGrid)
    {
        const float x = xForFrequency (line.hz);
        g.setColour (theme.panelBorderSoft.withMultipliedAlpha (line.major ? kGridMajorAlpha
                                                                          : kGridMinorAlpha));
        g.drawLine (x, plotBounds.getY(), x, plotBounds.getBottom(), theme.borderWidth);
    }

    for (float dB = kMinDb; dB <= kMaxDb + 0.5f; dB += kDbGrid)
    {
        const float y = yForDecibels (dB);

        if (std::abs (dB) < 0.5f)
            g.setColour (theme.cyanDim.withAlpha (kZeroLineAlpha));
        else
            g.setColour (theme.panelBorderSoft.withMultipliedAlpha (kGridMinorAlpha));

        g.drawLine (plotBounds.getX(), y, plotBounds.getRight(), y, theme.borderWidth);
    }

    //--------------------------------------------------------------------------
    // Subtle cyan wash beneath the response.
    juce::ColourGradient body (accentColour.withAlpha (kFillAlphaTop),
                               plotBounds.getCentreX(), plotBounds.getY(),
                               accentColour.withAlpha (0.0f),
                               plotBounds.getCentreX(), plotBounds.getBottom(), false);
    body.addColour (kFillMidStop, accentColour.withAlpha (kFillAlphaMid));
    g.setGradientFill (body);
    g.fillPath (fill);

    //--------------------------------------------------------------------------
    // Restrained bloom, then the crisp line.
    const float stroke = theme.borderWidth * kStrokeScale;
    const auto  joint  = juce::PathStrokeType::curved;
    const auto  cap    = juce::PathStrokeType::rounded;

    g.setColour (accentColour.withAlpha (kGlowAlphaOuter * theme.glowAmount));
    g.strokePath (curve, juce::PathStrokeType (stroke * kGlowWidthOuter, joint, cap));

    g.setColour (accentColour.withAlpha (kGlowAlphaInner * theme.glowAmount));
    g.strokePath (curve, juce::PathStrokeType (stroke * kGlowWidthInner, joint, cap));

    g.setColour (accentColour);
    g.strokePath (curve, juce::PathStrokeType (stroke, joint, cap));

    //--------------------------------------------------------------------------
    // Where the cutoff sits.
    g.setColour (accentColour.withAlpha (kCutoffLineAlpha));
    g.drawLine (cutoffX, plotBounds.getY(), cutoffX, plotBounds.getBottom(), theme.borderWidth);

    const float halo = theme.borderWidth * kCutoffHaloScale;
    g.setColour (accentColour.withAlpha (kCutoffHaloAlpha));
    g.fillEllipse (cutoffX - halo, cutoffY - halo, halo * 2.0f, halo * 2.0f);

    const float dot = theme.borderWidth * kCutoffDotScale;
    g.setColour (theme.primaryText);
    g.fillEllipse (cutoffX - dot, cutoffY - dot, dot * 2.0f, dot * 2.0f);

    //--------------------------------------------------------------------------
    if (showAxisLabels && ! labelBounds.isEmpty())
    {
        g.setColour (theme.tertiaryText);
        g.setFont (theme.smallFont());

        for (const auto& line : kFrequencyGrid)
        {
            if (line.label == nullptr)
                continue;

            const float x = xForFrequency (line.hz);
            const auto  area = juce::Rectangle<float> (x - (float) RippleTheme::xl * 0.5f,
                                                       labelBounds.getY(),
                                                       (float) RippleTheme::xl,
                                                       labelBounds.getHeight());
            g.drawText (line.label, area, juce::Justification::centred, false);
        }
    }
}

} // namespace ripples
