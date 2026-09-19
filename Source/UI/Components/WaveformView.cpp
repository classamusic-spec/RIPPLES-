#include "UI/Components/WaveformView.h"

#include "Utilities/MathUtils.h"

#include <cmath>

namespace ripples
{

namespace
{
    //==========================================================================
    // Graph-domain constants. These are ratios — of the plot rectangle, of the
    // theme stroke width, or of the theme glow amount — never raw pixel or
    // colour literals. Anything that maps to the screen goes through a
    // RippleTheme token.
    //==========================================================================

    constexpr float kVisibleCycles   = 2.0f;   // cycles shown across the plot width
    constexpr float kCycleMargin     = 1.0f;   // spare cycle each side, so drift never runs out
    constexpr float kPointsPerPixel  = 1.6f;   // curve resolution
    constexpr int   kMinPoints       = 128;
    constexpr int   kMaxPoints       = 1024;

    constexpr float kAmplitudeRatio  = 0.80f;  // of the plot half-height
    constexpr float kStrokeScale     = 1.7f;   // multiples of theme.borderWidth
    constexpr float kGlowWidthInner  = 3.0f;   // multiples of the crisp stroke width
    constexpr float kGlowWidthOuter  = 6.5f;
    constexpr float kGlowAlphaInner  = 0.34f;  // scaled by theme.glowAmount
    constexpr float kGlowAlphaOuter  = 0.16f;

    constexpr float kAutoGainMin     = 0.5f;   // bounds on the per-rebuild normalisation
    constexpr float kAutoGainMax     = 2.0f;

    // Water only: rebuild the cached path every N frames (20 Hz at 60 fps).
    // Its internal motion has a period of many seconds, so this is invisibly
    // smooth while costing a fraction of a per-frame rebuild.
    constexpr int   kMorphRebuildInterval = 3;

    // Internal-motion rates for the Water wave, in Hz.
    constexpr float kWaterRates[3] { 0.17f, 0.11f, 0.07f };

    //==========================================================================
    /** Reflects x back into [-1,1] — a wave folder. Identity on [-1,1]. */
    inline float foldTriangle (float x) noexcept
    {
        x += 1.0f;
        x -= 4.0f * std::floor (x * 0.25f);
        return x < 2.0f ? x - 1.0f : 3.0f - x;
    }

    /** The sunken well every visualisation sits in. */
    void drawWell (juce::Graphics& g, juce::Rectangle<float> r, const RippleTheme& t)
    {
        g.setColour (t.panelSunken);
        g.fillRoundedRectangle (r, t.controlRadius);

        // Light falls from above the surface: the top of the well is deeper.
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
WaveformView::WaveformView()
{
    setOpaque (false);
    setInterceptsMouseClicks (false, false);
}

WaveformView::~WaveformView()
{
    stopTimer();
}

//==============================================================================
void WaveformView::setWave (OscWave wave, float shape)
{
    const auto clamped = math::clamp (shape, 0.0f, 1.0f);

    if (wave == currentWave && juce::approximatelyEqual (clamped, currentShape))
        return;

    currentWave  = wave;
    currentShape = clamped;
    rebuildPath();
    repaint();
}

void WaveformView::setAccent (juce::Colour accent)
{
    if (accent == accentColour)
        return;

    accentColour = accent;
    repaint();
}

void WaveformView::setAnimated (bool shouldAnimate)
{
    if (shouldAnimate == animated)
        return;

    animated = shouldAnimate;
    updateTimerState();
    repaint();
}

//==============================================================================
void WaveformView::resized()
{
    plotBounds = getLocalBounds().toFloat().reduced ((float) RippleTheme::sm);
    rebuildPath();
}

void WaveformView::visibilityChanged()      { updateTimerState(); }
void WaveformView::parentHierarchyChanged() { updateTimerState(); }

void WaveformView::updateTimerState()
{
    const bool shouldRun = animated && isShowing();

    if (shouldRun == isTimerRunning())
        return;

    if (shouldRun)
        startTimerHz (juce::jlimit (1, 120, RippleTheme::get().targetFrameRate));
    else
        stopTimer();
}

void WaveformView::timerCallback()
{
    // Safety net: a parent further up may have been hidden without this
    // component hearing about it. One frame later the timer is gone again.
    if (! isShowing())
    {
        stopTimer();
        return;
    }

    const auto& theme = RippleTheme::get();
    const float dt = 1.0f / (float) juce::jmax (1, theme.targetFrameRate);

    driftPhase = math::wrapPhase (driftPhase + theme.ambientMotionRate * dt);

    if (waveMovesInternally())
    {
        for (int i = 0; i < 3; ++i)
            morphPhase[i] = math::wrapPhase (morphPhase[i] + kWaterRates[i] * dt);

        if (++framesSinceRebuild >= kMorphRebuildInterval)
        {
            framesSinceRebuild = 0;
            rebuildPath();
        }
    }

    repaint();
}

bool WaveformView::waveMovesInternally() const noexcept
{
    return currentWave == OscWave::Water;
}

//==============================================================================
float WaveformView::waveSample (float phase01) const noexcept
{
    using namespace math;

    const float p = wrapPhase (phase01);
    const float s = currentShape;

    switch (currentWave)
    {
        case OscWave::Sine:
        {
            // Phase distortion: the cycle leans left or right around its centre.
            const float bend   = (s - 0.5f) * 2.0f;                        // -1..1
            const float warped = p + bend * 0.22f * std::sin (twoPi * p);
            return std::sin (twoPi * warped);
        }

        case OscWave::Triangle:
        {
            // Shape slides the apex: symmetric at 0.5, a ramp at either end.
            const float apex = clamp (0.06f + s * 0.88f, 0.06f, 0.94f);
            return p < apex ? (p / apex) * 2.0f - 1.0f
                            : 1.0f - ((p - apex) / (1.0f - apex)) * 2.0f;
        }

        case OscWave::Saw:
        {
            // Shape curves the ramp — convex below 0.5, concave above.
            const float x = p * 2.0f - 1.0f;
            const float k = std::pow (2.6f, (s - 0.5f) * 2.0f);            // 0.38 .. 2.6
            return (x < 0.0f ? -1.0f : 1.0f) * std::pow (std::abs (x), k);
        }

        case OscWave::Square:
        {
            // Shape hardens the edges: rounded and almost sinusoidal at 0,
            // a hard square at 1.
            const float hardness = 1.1f + s * s * 46.0f;
            const float norm     = fastTanh (hardness);
            return norm > 0.0f ? fastTanh (hardness * std::sin (twoPi * p)) / norm : 0.0f;
        }

        case OscWave::Pulse:
        {
            const float duty = clamp (0.5f + (s - 0.5f) * 0.88f, 0.06f, 0.94f);
            return p < duty ? 1.0f : -1.0f;
        }

        case OscWave::Shark:
        {
            // Asymmetric folded saw — grows teeth as shape rises.
            const float drive = 1.0f + s * 2.6f;
            return foldTriangle ((p * 2.0f - 1.0f) * drive + s * 0.45f);
        }

        case OscWave::Hollow:
        {
            // Odd harmonics only: a clarinet heard underwater.
            const int partials = 2 + (int) std::lround (s * 7.0f);         // 2..9
            float acc = 0.0f, norm = 0.0f;

            for (int i = 0; i < partials; ++i)
            {
                const float n = (float) (2 * i + 1);
                const float a = std::pow (n, -1.35f);
                acc  += a * std::sin (twoPi * n * p);
                norm += a;
            }

            return norm > 0.0f ? acc / norm : 0.0f;
        }

        case OscWave::Glass:
        {
            // Slightly stretched, inharmonic-leaning partials — sea glass.
            constexpr float ratios[]  { 1.0f, 2.01f, 3.04f, 4.09f, 5.98f, 8.21f };
            constexpr float weights[] { 1.0f, 0.52f, 0.34f, 0.22f, 0.15f, 0.10f };
            constexpr float offsets[] { 0.0f, 0.6f,  1.2f,  1.8f,  2.4f,  3.0f  };

            const float brightness = 0.25f + s * 0.95f;
            float acc = 0.0f, tilt = 1.0f;

            for (int i = 0; i < 6; ++i)
            {
                acc  += weights[i] * tilt * std::sin (twoPi * ratios[i] * p + offsets[i]);
                tilt *= brightness;
            }

            return acc;
        }

        case OscWave::Water:
        {
            // Soft morphing wave with slow internal motion: a phase-modulated
            // fundamental plus two drifting partials.
            const float pmDepth = 0.35f + s * 0.75f;
            const float inner   = std::sin (twoPi * (2.0f * p + morphPhase[0]));

            float v = std::sin (twoPi * (p + pmDepth * 0.12f * inner));
            v += 0.34f * std::sin (twoPi * (2.0f * p + morphPhase[1])) * (0.35f + 0.65f * s);
            v += 0.18f * std::sin (twoPi * (3.0f * p - morphPhase[2])) * s;
            return v;
        }

        case OscWave::NumWaves:
        default:
            break;
    }

    return std::sin (math::twoPi * p);
}

//==============================================================================
void WaveformView::rebuildPath()
{
    curve.clear();

    if (plotBounds.getWidth() <= 0.0f || plotBounds.getHeight() <= 0.0f)
        return;

    const float spanCycles = kVisibleCycles + 2.0f * kCycleMargin;
    pixelsPerCycle = plotBounds.getWidth() / kVisibleCycles;

    const int numPoints = juce::jlimit (kMinPoints, kMaxPoints,
                                        (int) (plotBounds.getWidth() * kPointsPerPixel
                                               * spanCycles / kVisibleCycles));

    if ((int) scratch.size() != numPoints)
        scratch.resize ((size_t) numPoints);

    // First pass: sample and find the peak so every wave fills the well evenly.
    float peak = 0.0f;

    for (int i = 0; i < numPoints; ++i)
    {
        const float phase = -kCycleMargin + spanCycles * (float) i / (float) (numPoints - 1);
        const float v = waveSample (phase);
        scratch[(size_t) i] = v;
        peak = juce::jmax (peak, std::abs (v));
    }

    const float gain = math::clamp (peak > 1.0e-4f ? 1.0f / peak : 1.0f,
                                    kAutoGainMin, kAutoGainMax);

    const float midY = plotBounds.getCentreY();
    const float amp  = plotBounds.getHeight() * 0.5f * kAmplitudeRatio;

    curve.preallocateSpace (numPoints * 3 + 8);

    for (int i = 0; i < numPoints; ++i)
    {
        const float phase = -kCycleMargin + spanCycles * (float) i / (float) (numPoints - 1);
        const float x = plotBounds.getX() + phase * pixelsPerCycle;
        const float y = midY - math::clamp (scratch[(size_t) i] * gain, -1.0f, 1.0f) * amp;

        if (i == 0) curve.startNewSubPath (x, y);
        else        curve.lineTo (x, y);
    }
}

//==============================================================================
void WaveformView::paint (juce::Graphics& g)
{
    const auto& theme = RippleTheme::get();
    const auto  well  = getLocalBounds().toFloat().reduced ((float) RippleTheme::xs);

    if (well.getWidth() <= 0.0f || well.getHeight() <= 0.0f)
        return;

    drawWell (g, well, theme);

    // Being painted means we are on screen: if animation is wanted but the
    // timer was stopped while hidden, this is where it comes back.
    if (animated && ! isTimerRunning())
        updateTimerState();

    if (curve.isEmpty())
        return;

    juce::Path clipShape;
    clipShape.addRoundedRectangle (well.reduced (theme.borderWidth), theme.controlRadius);

    const juce::Graphics::ScopedSaveState saved (g);
    g.reduceClipRegion (clipShape);

    // Zero axis.
    g.setColour (theme.panelBorderSoft);
    g.drawLine (plotBounds.getX(), plotBounds.getCentreY(),
                plotBounds.getRight(), plotBounds.getCentreY(), theme.borderWidth);

    const auto drift = juce::AffineTransform::translation (-driftPhase * pixelsPerCycle, 0.0f);

    const float stroke = theme.borderWidth * kStrokeScale;
    const auto  joint  = juce::PathStrokeType::curved;
    const auto  cap    = juce::PathStrokeType::rounded;

    // Cheap bloom: two wide, faint passes beneath the crisp line. No blurred
    // layer, no image allocation.
    g.setColour (accentColour.withAlpha (kGlowAlphaOuter * theme.glowAmount));
    g.strokePath (curve, juce::PathStrokeType (stroke * kGlowWidthOuter, joint, cap), drift);

    g.setColour (accentColour.withAlpha (kGlowAlphaInner * theme.glowAmount));
    g.strokePath (curve, juce::PathStrokeType (stroke * kGlowWidthInner, joint, cap), drift);

    g.setColour (accentColour);
    g.strokePath (curve, juce::PathStrokeType (stroke, joint, cap), drift);
}

} // namespace ripples
