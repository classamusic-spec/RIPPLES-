#include "UI/Components/LiquidBackdrop.h"

#include "UI/Theme/RippleTheme.h"
#include "Utilities/MathUtils.h"

#include <cmath>

namespace ripples
{

namespace
{
    // The caustic field is rendered at this many pixels on its longer axis and
    // scaled up. The motion is soft and low-frequency, so the eye never reads
    // the low resolution -- but the per-pixel cost is quartered against a
    // half-scale field, which is what keeps a full-window animated layer
    // affordable.
    constexpr int   kFieldLong   = 360;

    // Reduced update rate: calm water gains nothing from 60 fps, and every
    // frame here recomposites the translucent panels above.
    constexpr int   kFrameRate   = 30;

    constexpr float kDriftRate   = 0.16f;   // radians / second, main flow
    constexpr float kWarpRate    = 0.09f;   // radians / second, domain warp

    inline float onePole (float dt, float tau) noexcept
    {
        return 1.0f - std::exp (-dt / juce::jmax (1.0e-4f, tau));
    }
}

//==============================================================================
LiquidBackdrop::LiquidBackdrop (VisualizationState& vis)
    : visual (vis)
{
    // Pure decoration behind everything: never intercept a click.
    setInterceptsMouseClicks (false, false);
    setOpaque (true);   // the base fill covers every pixel, so no blending under it
}

LiquidBackdrop::~LiquidBackdrop() = default;

//==============================================================================
void LiquidBackdrop::resized()
{
    const int w = getWidth(), h = getHeight();

    if (w <= 0 || h <= 0)
    {
        field = juce::Image();
        return;
    }

    const float aspect = (float) w / (float) h;

    int fw, fh;
    if (aspect >= 1.0f) { fw = kFieldLong; fh = juce::jmax (16, juce::roundToInt (kFieldLong / aspect)); }
    else                { fh = kFieldLong; fw = juce::jmax (16, juce::roundToInt (kFieldLong * aspect)); }

    field = juce::Image (juce::Image::RGB, fw, fh, false);
    renderInto (field);
}

void LiquidBackdrop::visibilityChanged()      { updateTimerState(); }
void LiquidBackdrop::parentHierarchyChanged() { updateTimerState(); }

void LiquidBackdrop::updateTimerState()
{
    const bool shouldRun = isVisible() && (getParentComponent() == nullptr || isShowing());

    if (shouldRun)
    {
        if (! isTimerRunning())
        {
            lastTickMs = juce::Time::getMillisecondCounterHiRes();
            startTimerHz (kFrameRate);
        }
    }
    else if (isTimerRunning())
    {
        stopTimer();
    }
}

void LiquidBackdrop::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    float dt = (float) ((now - lastTickMs) * 0.001);
    lastTickMs = now;

    if (! std::isfinite (dt) || dt <= 0.0f)
        dt = 1.0f / (float) kFrameRate;

    dt = juce::jmin (dt, 0.1f);

    phase += math::twoPi * kDriftRate * dt;
    if (phase > math::twoPi) phase -= math::twoPi;

    warp += math::twoPi * kWarpRate * dt;
    if (warp > math::twoPi) warp -= math::twoPi;

    const float c = onePole (dt, 0.4f);
    glowSmoothed += (math::clamp (visual.getGlow(), 0.0f, 1.0f) - glowSmoothed) * c;
    rmsSmoothed  += (math::clamp (visual.getOutputRMS() * 3.0f, 0.0f, 1.0f) - rmsSmoothed) * c;

    if (field.isValid())
    {
        renderInto (field);
        repaint();
    }
}

//==============================================================================
void LiquidBackdrop::renderInto (juce::Image& image) const
{
    const auto& t = RippleTheme::get();

    const int w = image.getWidth();
    const int h = image.getHeight();

    juce::Image::BitmapData px (image, juce::Image::BitmapData::writeOnly);

    // Base water: a vertical gradient, lighter at the top, sinking to near
    // black. The caustics ride on top of this.
    const auto top = t.backgroundLift.interpolatedWith (t.background, 0.35f);
    const auto bot = t.backgroundDeep;
    const auto lit = t.cyan;                          // caustic glow colour

    const float topR = top.getFloatRed(),  topG = top.getFloatGreen(),  topB = top.getFloatBlue();
    const float botR = bot.getFloatRed(),  botG = bot.getFloatGreen(),  botB = bot.getFloatBlue();
    const float litR = lit.getFloatRed(),  litG = lit.getFloatGreen(),  litB = lit.getFloatBlue();

    // How bright the caustics glow. Kept low so panels stay legible; lifts a
    // little with the sound.
    const float glow = 0.16f + 0.13f * glowSmoothed + 0.07f * rmsSmoothed;

    const float ph = phase;
    const float wp = warp;

    const float invW = 1.0f / (float) w;
    const float invH = 1.0f / (float) h;

    const auto q = [] (float f) noexcept
    {
        return (juce::uint8) math::clamp ((int) (f * 255.0f + 0.5f), 0, 255);
    };

    for (int y = 0; y < h; ++y)
    {
        auto* row = px.getLinePointer (y);
        const float v = (float) y * invH;

        // Vertical base gradient for this row.
        const float g = math::smootherstep (v);
        const float baseR = topR + (botR - topR) * g;
        const float baseG = topG + (botG - topG) * g;
        const float baseB = topB + (botB - topB) * g;

        for (int x = 0; x < w; ++x)
        {
            const float u = (float) x * invW;

            // Domain warp: displace the sample point by a slow field, so the
            // caustics curl and flow rather than sliding rigidly.
            const float wx = u + 0.10f * std::sin (v * 6.10f + wp)
                                + 0.06f * std::sin (v * 11.3f - ph * 0.7f);
            const float wy = v + 0.10f * std::sin (u * 5.30f - wp * 0.8f)
                                + 0.06f * std::sin (u * 9.70f + ph * 0.6f);

            // Two octaves of crossed sines make the interlocking caustic net.
            float c = std::sin (wx * 7.5f + ph)        * std::sin (wy * 6.3f - ph * 0.8f);
            c      += 0.55f * std::sin (wx * 14.0f - ph * 1.3f) * std::sin (wy * 12.4f + ph * 0.6f);

            c = c * 0.5f + 0.5f;                        // -> 0..1
            c = c * c;                                  // sharpen into bands

            // Broad, slow light shafts drifting sideways, as light coming down
            // through a moving surface. Gives the flow a clear direction rather
            // than an even shimmer.
            const float shaft = 0.5f + 0.5f * std::sin (wx * 2.3f - ph * 0.9f + wy * 0.8f);
            c *= 0.55f + 0.75f * shaft;

            c *= glow;

            // A soft vignette so the edges sit down into the frame.
            const float dx = (u - 0.5f) * 2.0f;
            const float dy = (v - 0.5f) * 2.0f;
            const float vign = math::clamp (1.0f - 0.45f * (dx * dx + dy * dy), 0.30f, 1.0f);

            float r = (baseR + (litR - baseR) * c) * vign;
            float gg = (baseG + (litG - baseG) * c) * vign;
            float b = (baseB + (litB - baseB) * c) * vign;

            // BitmapData for an RGB image is B, G, R in memory on little-endian.
            auto* p = row + x * px.pixelStride;
            p[0] = q (b);
            p[1] = q (gg);
            p[2] = q (r);
        }
    }
}

void LiquidBackdrop::paint (juce::Graphics& g)
{
    if (field.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
        g.drawImage (field, getLocalBounds().toFloat());
    }
    else
    {
        g.fillAll (RippleTheme::get().background);
    }
}

} // namespace ripples
