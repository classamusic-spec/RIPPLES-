#include "UI/Theme/RippleLookAndFeel.h"
#include "Utilities/MathUtils.h"

#include <cmath>

namespace ripples::ui
{

//==============================================================================
juce::Colour dimForState (juce::Colour c, bool enabled)
{
    return enabled ? c : c.withMultipliedAlpha (RippleTheme::get().disabledAlpha);
}

//==============================================================================
void drawSoftCircleShadow (juce::Graphics& g, juce::Point<float> centre, float radius,
                           juce::Colour colour, float spread, float yOffset)
{
    if (radius <= 0.0f || spread <= 0.0f || colour.isTransparent())
        return;

    const auto c     = centre.translated (0.0f, yOffset);
    const auto outer = radius + spread;

    juce::ColourGradient grad (colour, c,
                               colour.withAlpha (0.0f), c.translated (outer, 0.0f), true);

    // Hold the shadow solid under the body, then fall away through the spread.
    const auto knee = juce::jlimit (0.05, 0.9, (double) (radius / outer));
    grad.addColour (knee * 0.85, colour);
    grad.addColour (knee, colour.withMultipliedAlpha (0.55f));

    g.setGradientFill (grad);
    g.fillEllipse (juce::Rectangle<float> (outer * 2.0f, outer * 2.0f).withCentre (c));
}

void drawSoftShadow (juce::Graphics& g, juce::Rectangle<float> bounds, float cornerRadius,
                     juce::Colour colour, float spread, float yOffset)
{
    if (spread <= 0.0f || bounds.isEmpty() || colour.isTransparent())
        return;

    const auto& theme = RippleTheme::get();
    const auto layers = juce::jmax (1, theme.shadowLayers);
    const auto step   = spread / (float) layers;

    // Layered strokes rather than a blurred image: no allocation, resolution
    // independent, and cheap enough to sit behind every panel.
    for (int i = layers; i >= 1; --i)
    {
        const auto f = ((float) i - 0.5f) / (float) layers;
        const auto r = bounds.translated (0.0f, yOffset).expanded (spread * f);

        g.setColour (colour.withMultipliedAlpha (theme.shadowLayerAlpha * (1.0f - f)));
        g.drawRoundedRectangle (r, cornerRadius + spread * f, step * 2.0f);
    }
}

//==============================================================================
void drawGlassSurface (juce::Graphics& g, juce::Rectangle<float> bounds, float cornerRadius,
                       juce::Colour accent, bool withHighlight)
{
    const auto& theme = RippleTheme::get();

    if (bounds.getWidth() < 2.0f || bounds.getHeight() < 2.0f)
        return;

    juce::Path shape;
    shape.addRoundedRectangle (bounds, cornerRadius);

    //--------------------------------------------------------------------------
    // 1. The water column. More water to look through at the bottom, so it is
    //    denser and darker there. The accent tints the body very slightly, which
    //    is what keeps a violet-accented panel from looking like a cyan one.
    //--------------------------------------------------------------------------
    {
        juce::ColourGradient column (theme.liquidTop,  bounds.getCentreX(), bounds.getY(),
                                     theme.liquidDeep, bounds.getCentreX(), bounds.getBottom(), false);
        column.addColour (0.45, theme.liquidMid.interpolatedWith (accent.withAlpha (theme.liquidMid.getFloatAlpha()), 0.10f));
        g.setGradientFill (column);
        g.fillPath (shape);
    }

    const juce::Graphics::ScopedSaveState inside (g);
    g.reduceClipRegion (shape, {});

    const auto inner = bounds.reduced (theme.glassWallThickness);
    auto       work  = inner;   // scratch copy for the passes that slice

    //--------------------------------------------------------------------------
    // 2. Edge refraction. Light bending through the curved wall concentrates in
    //    a narrow band against each side, brightest low down where the glass is
    //    looking through the most liquid.
    //--------------------------------------------------------------------------
    {
        const float band = juce::jmax (4.0f, bounds.getWidth() * theme.refractionBandRatio);

        g.setGradientFill ({ theme.edgeRefraction,                inner.getX(), inner.getCentreY(),
                             theme.edgeRefraction.withAlpha (0.0f), inner.getX() + band, inner.getCentreY(), false });
        g.fillRect (inner.withWidth (band));

        g.setGradientFill ({ theme.edgeRefraction.withMultipliedAlpha (0.62f), inner.getRight(), inner.getCentreY(),
                             theme.edgeRefraction.withAlpha (0.0f), inner.getRight() - band, inner.getCentreY(), false });
        g.fillRect (work.removeFromRight (band));
    }

    //--------------------------------------------------------------------------
    // 3. Caustic floor — light focused through the water pools at the base.
    //--------------------------------------------------------------------------
    {
        const float h = juce::jmax (6.0f, bounds.getHeight() * theme.causticHeightRatio);
        const auto pool = bounds.withTop (bounds.getBottom() - h);

        g.setGradientFill ({ theme.causticFloor.withAlpha (0.0f), pool.getCentreX(), pool.getY(),
                             theme.causticFloor,                  pool.getCentreX(), pool.getBottom(), false });
        g.fillRect (pool);
    }

    //--------------------------------------------------------------------------
    // 4. Specular streaks. Cylindrical glass catches light in a LINE down its
    //    length, not as a wash across the top — this is the cue that reads as a
    //    curved surface. The opposite side gets a weaker catch.
    //--------------------------------------------------------------------------
    if (withHighlight)
    {
        const float w = juce::jmax (2.0f, bounds.getWidth() * theme.specularWidthRatio);

        // A light catch on curved glass is an elongated soft highlight, not a
        // bar: it fades out at BOTH ends as the wall turns away from the light.
        //
        // Drawn as a circular radial gradient squashed into an ellipse by a
        // transform. An earlier version stacked horizontal slices to shape the
        // falloff, which banded visibly on short wide panels like the macro
        // strip -- a gradient under a transform has no seams to show.
        const auto streak = [&] (float centreRatio, float strength)
        {
            const juce::Graphics::ScopedSaveState local (g);

            const float cx = bounds.getX() + bounds.getWidth() * centreRatio;
            const float cy = inner.getY() + inner.getHeight() * 0.38f;
            const float hw = juce::jmax (1.5f, w * 0.5f);
            const float hh = inner.getHeight() * 0.62f;

            g.addTransform (juce::AffineTransform::translation (-cx, -cy)
                                .scaled (1.0f, hh / hw)
                                .translated (cx, cy));

            juce::ColourGradient bead (theme.glassSpecular.withMultipliedAlpha (strength), cx, cy,
                                       theme.glassSpecular.withAlpha (0.0f), cx + hw, cy, true);
            bead.isRadial = true;
            g.setGradientFill (bead);
            g.fillEllipse (cx - hw, cy - hw, hw * 2.0f, hw * 2.0f);
        };

        streak (theme.specularLeftRatio,  1.0f);
        streak (theme.specularRightRatio, 0.38f);
    }

    //--------------------------------------------------------------------------
    // 5. The meniscus — liquid climbing the inside of the glass. Brightest
    //    across the middle and fading into the corners, where the curve of the
    //    wall turns it away from the eye.
    //--------------------------------------------------------------------------
    {
        const float y = inner.getY() + theme.meniscusInset;
        const float fade = juce::jmin (cornerRadius * 2.0f, inner.getWidth() * 0.32f);

        juce::ColourGradient m (theme.meniscus.withAlpha (0.0f), inner.getX(), y,
                                theme.meniscus.withAlpha (0.0f), inner.getRight(), y, false);
        m.addColour (juce::jlimit (0.01, 0.49, (double) (fade / inner.getWidth())), theme.meniscus);
        m.addColour (juce::jlimit (0.51, 0.99, (double) (1.0f - fade / inner.getWidth())), theme.meniscus);
        g.setGradientFill (m);
        g.fillRect (inner.getX(), y, inner.getWidth(), theme.meniscusThickness);
    }
}

//==============================================================================
/** The glass wall: a lit outer rim and the refracted inner edge behind it.
    Drawn after the contents so the wall sits in front of the liquid, which is
    what gives a panel its thickness. */
void drawGlassRim (juce::Graphics& g, juce::Rectangle<float> bounds, float cornerRadius,
                   juce::Colour accent)
{
    const auto& theme = RippleTheme::get();

    if (bounds.getWidth() < 2.0f || bounds.getHeight() < 2.0f)
        return;

    // Inner wall first — the back of the glass, seen through the liquid.
    g.setColour (theme.glassInnerWall);
    g.drawRoundedRectangle (bounds.reduced (theme.glassWallThickness),
                            juce::jmax (0.0f, cornerRadius - theme.glassWallThickness),
                            theme.borderWidth);

    // Then the outer rim, lit from above: bright along the top, almost gone by
    // the base. A single flat stroke is what makes glass look like a sticker.
    const auto top = theme.glassRimTop.interpolatedWith (
                         accent.withAlpha (theme.glassRimTop.getFloatAlpha()), 0.35f);

    g.setGradientFill ({ top,                  bounds.getCentreX(), bounds.getY(),
                         theme.glassRimBottom, bounds.getCentreX(), bounds.getBottom(), false });

    juce::Path rim;
    rim.addRoundedRectangle (bounds.reduced (theme.glassRimWidth * 0.5f), cornerRadius);
    g.strokePath (rim, juce::PathStrokeType (theme.glassRimWidth));

    // The lit upper lip. On a real vessel this is the single brightest thing:
    // the top edge of the glass catching the light square-on. Without it the
    // wall reads as a drawn outline rather than a physical edge. It fades into
    // the corners, where the curve turns away.
    {
        const float y  = bounds.getY() + theme.glassLipWidth * 0.5f;
        const float x0 = bounds.getX() + cornerRadius * 0.75f;
        const float x1 = bounds.getRight() - cornerRadius * 0.75f;

        if (x1 > x0)
        {
            juce::ColourGradient lip (top.withAlpha (0.0f), x0, y,
                                      top.withAlpha (0.0f), x1, y, false);
            lip.addColour (0.22, top);
            lip.addColour (0.78, top);
            g.setGradientFill (lip);
            g.fillRect (x0, bounds.getY(), x1 - x0, theme.glassLipWidth);
        }
    }
}

void drawControlWell (juce::Graphics& g, juce::Rectangle<float> bounds, float cornerRadius,
                      juce::Colour border, bool hovered, bool down, bool enabled)
{
    const auto& theme = RippleTheme::get();

    const auto fill = down     ? theme.controlFillDown
                    : hovered  ? theme.controlFillHover
                               : theme.controlFill;

    g.setColour (dimForState (fill, enabled));
    g.fillRoundedRectangle (bounds, cornerRadius);

    g.setColour (dimForState (hovered ? border : theme.controlBorder, enabled));
    g.drawRoundedRectangle (bounds.reduced (theme.borderWidth * 0.5f), cornerRadius, theme.borderWidth);
}

void drawChevron (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour, float thickness)
{
    const auto& theme = RippleTheme::get();
    const auto s = (float) theme.chevronSize;
    const auto c = area.getCentre();

    juce::Path p;
    p.startNewSubPath (c.x - s, c.y - s * 0.5f);
    p.lineTo          (c.x,     c.y + s * 0.5f);
    p.lineTo          (c.x + s, c.y - s * 0.5f);

    g.setColour (colour);
    g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));
}

void drawFocusRing (juce::Graphics& g, juce::Rectangle<float> bounds, float cornerRadius)
{
    const auto& theme = RippleTheme::get();

    g.setColour (theme.focusRing);
    g.drawRoundedRectangle (bounds.expanded (theme.focusRingPadding),
                            cornerRadius + theme.focusRingPadding,
                            theme.focusRingWidth);
}

//==============================================================================
void drawSwitch (juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour accent,
                 float onAmount, bool hovered, bool down, bool enabled, bool focused)
{
    const auto& theme = RippleTheme::get();

    const auto track  = bounds.reduced (theme.borderWidth * 0.5f);
    const auto radius = track.getHeight() * 0.5f;
    const auto pos    = juce::jlimit (0.0f, 1.0f, onAmount);

    // Track: dark well when off, accent-filled when on.
    const auto offFill = down ? theme.controlFillDown
                              : (hovered ? theme.controlFillHover : theme.toggleTrackOff);
    const auto onFill  = down ? accent.darker (theme.pressDarken)
                              : (hovered ? accent.brighter (theme.hoverBrighten) : accent);

    g.setColour (dimForState (offFill.interpolatedWith (onFill, pos), enabled));
    g.fillRoundedRectangle (track, radius);

    g.setColour (dimForState (theme.toggleTrackBorder.interpolatedWith (onFill, pos * 0.6f), enabled));
    g.drawRoundedRectangle (track, radius, theme.borderWidth);

    // Thumb: moves, and changes shape — a hollow ring when off, a solid disc
    // when on — so the state never depends on colour alone.
    const auto inset    = theme.toggleThumbInset;
    const auto thumbR   = juce::jmax (theme.borderWidth, radius - inset);
    const auto travel   = track.getWidth() - (thumbR + inset) * 2.0f;
    const auto centre   = juce::Point<float> (track.getX() + thumbR + inset + travel * pos,
                                              track.getCentreY());
    const auto thumbBox = juce::Rectangle<float> (thumbR * 2.0f, thumbR * 2.0f).withCentre (centre);

    const auto thumbCol = theme.toggleThumbOff.interpolatedWith (theme.toggleThumbOn, pos);

    if (pos > 0.5f)
    {
        g.setColour (dimForState (thumbCol, enabled));
        g.fillEllipse (thumbBox);
    }
    else
    {
        g.setColour (dimForState (thumbCol, enabled));
        g.drawEllipse (thumbBox.reduced (theme.borderWidth), theme.knobRingThickness);
    }

    if (focused)
        drawFocusRing (g, track, radius);
}

//==============================================================================
void drawKnob (juce::Graphics& g, juce::Rectangle<float> area, const KnobStyle& style)
{
    const auto& theme = RippleTheme::get();

    const auto diameter = juce::jmin (area.getWidth(), area.getHeight());

    if (diameter <= 1.0f)
        return;

    const auto square = area.withSizeKeepingCentre (diameter, diameter);
    const auto centre = square.getCentre();
    const auto radius = diameter * 0.5f;

    // Stroke widths scale with the control, but only within sane limits, so a
    // small knob keeps a legible arc instead of a hairline.
    const auto stroke = juce::jlimit (0.8f, 1.6f, diameter / (float) theme.knobMediumDiameter);

    const auto hover     = juce::jlimit (0.0f, 1.0f, style.hover);
    const auto ornament  = style.detail >= theme.knobDetailThreshold;
    const auto alpha     = style.enabled ? 1.0f : theme.disabledAlpha;
    const auto accent    = style.accent.brighter (theme.hoverBrighten * hover);

    const auto bodyRadius = radius * theme.knobBodyRadiusRatio;
    const auto ringRadius = radius * theme.knobRingRadiusRatio;
    const auto arcRadius  = radius * theme.knobArcRadiusRatio;
    const auto modRadius  = radius * theme.knobModRadiusRatio;

    //--- soft drop shadow, lifting a little on hover --------------------------
    drawSoftCircleShadow (g, centre, bodyRadius,
                          theme.knobShadow.withMultipliedAlpha (alpha),
                          (theme.knobShadowSpread + theme.knobShadowSpread * 0.35f * hover) * stroke,
                          theme.knobShadowOffset * stroke);

    //--- body, with subtle radial shading: lighter top, darker bottom ---------
    const auto body = juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre);
    {
        juce::ColourGradient shade (theme.knobBodyTop.withMultipliedAlpha (alpha),
                                    centre.translated (0.0f, -bodyRadius * theme.knobShadeOffsetRatio),
                                    theme.knobBodyBottom.withMultipliedAlpha (alpha),
                                    centre.translated (0.0f,  bodyRadius * theme.knobShadeSpreadRatio),
                                    true);
        shade.addColour (0.55, theme.knobBody.withMultipliedAlpha (alpha));
        g.setGradientFill (shade);
        g.fillEllipse (body);
    }

    //--- glass cap: a specular bead high on the body -------------------------
    // Every other surface in the instrument is glass, so the knobs are too.
    // A real dome catches the light in a small bright bead up and to the left,
    // with a much fainter bounce coming back off the opposite inner wall.
    if (ornament)
    {
        const auto bead = juce::Point<float> (centre.x - bodyRadius * 0.34f,
                                              centre.y - bodyRadius * 0.40f);
        const float beadR = bodyRadius * 0.52f;

        juce::ColourGradient spec (theme.glassSpecular.withMultipliedAlpha (0.55f * alpha), bead.x, bead.y,
                                   theme.glassSpecular.withAlpha (0.0f), bead.x, bead.y + beadR, true);
        spec.isRadial = true;
        g.setGradientFill (spec);
        g.fillEllipse (juce::Rectangle<float> (beadR * 1.7f, beadR * 1.25f).withCentre (bead));

        const auto bounce = juce::Point<float> (centre.x + bodyRadius * 0.30f,
                                                centre.y + bodyRadius * 0.44f);
        const float bounceR = bodyRadius * 0.46f;

        juce::ColourGradient back (theme.edgeRefraction.withMultipliedAlpha (0.30f * alpha), bounce.x, bounce.y,
                                   theme.edgeRefraction.withAlpha (0.0f), bounce.x, bounce.y + bounceR, true);
        back.isRadial = true;
        g.setGradientFill (back);
        g.fillEllipse (juce::Rectangle<float> (bounceR * 1.8f, bounceR * 1.1f).withCentre (bounce));
    }

    //--- mild inner highlight across the top of the body ---------------------
    if (ornament)
    {
        const auto hlRadius = radius * theme.knobHighlightRadiusRatio;

        juce::Path highlight;
        highlight.addCentredArc (centre.x, centre.y, hlRadius, hlRadius, 0.0f,
                                 -juce::MathConstants<float>::halfPi * 1.15f,
                                  juce::MathConstants<float>::halfPi * 1.15f, true);

        g.setGradientFill ({ theme.knobHighlight.withMultipliedAlpha (alpha), centre.x, centre.y - hlRadius,
                             theme.knobHighlight.withAlpha (0.0f),            centre.x, centre.y,
                             false });
        g.strokePath (highlight, juce::PathStrokeType (theme.knobRingThickness * stroke,
                                                       juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
    }

    //--- thin outer ring -----------------------------------------------------
    {
        const auto ring = theme.knobRing.interpolatedWith (theme.knobRingHover, hover);
        g.setColour (ring.withMultipliedAlpha (alpha));
        g.drawEllipse (juce::Rectangle<float> (ringRadius * 2.0f, ringRadius * 2.0f).withCentre (centre),
                       theme.knobRingThickness * stroke);
    }

    //--- value arc: unfilled track, then the bright accent arc ---------------
    const auto sweep       = theme.knobEndAngle - theme.knobStartAngle;
    const auto valueAngle  = theme.knobStartAngle + sweep * juce::jlimit (0.0f, 1.0f, style.value01);
    const auto originAngle = style.bipolar ? theme.knobStartAngle + sweep * 0.5f : theme.knobStartAngle;
    const auto arcStroke   = juce::PathStrokeType (theme.knobArcThickness * stroke,
                                                   juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded);
    {
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                             theme.knobStartAngle, theme.knobEndAngle, true);
        g.setColour (theme.knobTrack.withMultipliedAlpha (alpha));
        g.strokePath (track, arcStroke);
    }

    if (std::abs (valueAngle - originAngle) > theme.modRingEpsilon)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                             juce::jmin (originAngle, valueAngle),
                             juce::jmax (originAngle, valueAngle), true);

        // A restrained bloom under the arc — never a neon halo.
        if (ornament)
        {
            g.setColour (accent.withAlpha (theme.arcGlowAlpha * theme.glowAmount * (0.4f + 0.6f * hover) * alpha));
            g.strokePath (value, juce::PathStrokeType (theme.knobArcThickness * stroke * 2.4f,
                                                       juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
        }

        g.setColour (accent.withMultipliedAlpha (alpha));
        g.strokePath (value, arcStroke);
    }

    //--- modulation ring: how far the matrix is pushing this value -----------
    if (std::abs (style.modAmount) > theme.modRingEpsilon)
    {
        const auto target   = juce::jlimit (0.0f, 1.0f, style.value01 + style.modAmount);
        const auto modAngle = theme.knobStartAngle + sweep * target;

        juce::Path mod;
        mod.addCentredArc (centre.x, centre.y, modRadius, modRadius, 0.0f,
                           juce::jmin (valueAngle, modAngle),
                           juce::jmax (valueAngle, modAngle), true);

        g.setColour (theme.modRing.withMultipliedAlpha (theme.modRingAlpha * alpha));
        g.strokePath (mod, juce::PathStrokeType (theme.knobModArcThickness * stroke,
                                                 juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

        if (ornament)
        {
            const auto dot = centre.getPointOnCircumference (modRadius, modAngle);
            const auto dotR = theme.knobModArcThickness * stroke;
            g.fillEllipse (juce::Rectangle<float> (dotR * 2.0f, dotR * 2.0f).withCentre (dot));
        }
    }

    //--- small crisp indicator ----------------------------------------------
    {
        juce::Path indicator;
        indicator.startNewSubPath (centre.getPointOnCircumference (radius * theme.knobIndicatorInnerRatio, valueAngle));
        indicator.lineTo          (centre.getPointOnCircumference (radius * theme.knobIndicatorOuterRatio, valueAngle));

        g.setColour (theme.knobIndicator.withMultipliedAlpha (alpha));
        g.strokePath (indicator, juce::PathStrokeType (theme.knobIndicatorThickness * stroke,
                                                       juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
    }

    if (style.focused)
    {
        g.setColour (theme.focusRing);
        g.drawEllipse (square.reduced (theme.focusRingWidth * 0.5f), theme.focusRingWidth);
    }
}

} // namespace ripples::ui

//==============================================================================
namespace ripples
{

RippleLookAndFeel::RippleLookAndFeel()
{
    applyColourScheme();
}

RippleLookAndFeel::~RippleLookAndFeel() = default;

void RippleLookAndFeel::applyColourScheme()
{
    const auto& t = RippleTheme::get();
    const auto clear = t.background.withAlpha (0.0f);

    setColour (juce::ResizableWindow::backgroundColourId, t.background);
    setColour (juce::DocumentWindow::textColourId,        t.primaryText);

    setColour (juce::Label::backgroundColourId,           clear);
    setColour (juce::Label::outlineColourId,              clear);
    setColour (juce::Label::textColourId,                 t.primaryText);
    setColour (juce::Label::textWhenEditingColourId,      t.primaryText);

    setColour (juce::Slider::backgroundColourId,          t.knobTrack);
    setColour (juce::Slider::trackColourId,               t.cyanDim);
    setColour (juce::Slider::thumbColourId,               t.cyan);
    setColour (juce::Slider::rotarySliderFillColourId,    t.cyan);
    setColour (juce::Slider::rotarySliderOutlineColourId, t.knobRing);
    setColour (juce::Slider::textBoxTextColourId,         t.primaryText);
    setColour (juce::Slider::textBoxBackgroundColourId,   t.controlFill);
    setColour (juce::Slider::textBoxOutlineColourId,      t.controlBorder);
    setColour (juce::Slider::textBoxHighlightColourId,    t.selectionHighlight);

    setColour (juce::TextButton::buttonColourId,          t.controlFill);
    setColour (juce::TextButton::buttonOnColourId,        t.cyan);
    setColour (juce::TextButton::textColourOffId,         t.primaryText);
    setColour (juce::TextButton::textColourOnId,          t.background);

    setColour (juce::ToggleButton::textColourId,          t.primaryText);
    setColour (juce::ToggleButton::tickColourId,          t.cyan);
    setColour (juce::ToggleButton::tickDisabledColourId,  t.disabledText);

    setColour (juce::ComboBox::backgroundColourId,        t.controlFill);
    setColour (juce::ComboBox::textColourId,              t.primaryText);
    setColour (juce::ComboBox::outlineColourId,           t.controlBorder);
    setColour (juce::ComboBox::buttonColourId,            clear);
    setColour (juce::ComboBox::arrowColourId,             t.cyan);
    setColour (juce::ComboBox::focusedOutlineColourId,    t.focusRing);

    setColour (juce::PopupMenu::backgroundColourId,            t.menuBackground);
    setColour (juce::PopupMenu::textColourId,                  t.primaryText);
    setColour (juce::PopupMenu::headerTextColourId,            t.secondaryText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, t.menuHighlight);
    setColour (juce::PopupMenu::highlightedTextColourId,       t.cyanBright);

    setColour (juce::TextEditor::backgroundColourId,      t.controlFill);
    setColour (juce::TextEditor::textColourId,            t.primaryText);
    setColour (juce::TextEditor::highlightColourId,       t.selectionHighlight);
    setColour (juce::TextEditor::highlightedTextColourId, t.primaryText);
    setColour (juce::TextEditor::outlineColourId,         t.controlBorder);
    setColour (juce::TextEditor::focusedOutlineColourId,  t.focusRing);
    setColour (juce::TextEditor::shadowColourId,          clear);
    setColour (juce::CaretComponent::caretColourId,       t.cyan);

    setColour (juce::ScrollBar::backgroundColourId,       clear);
    setColour (juce::ScrollBar::thumbColourId,            t.scrollbarThumb);
    setColour (juce::ScrollBar::trackColourId,            t.panelSunken);

    setColour (juce::TooltipWindow::backgroundColourId,   t.tooltipBackground);
    setColour (juce::TooltipWindow::textColourId,         t.primaryText);
    setColour (juce::TooltipWindow::outlineColourId,      t.menuBorder);

    setColour (juce::AlertWindow::backgroundColourId,     t.menuBackground);
    setColour (juce::AlertWindow::textColourId,           t.primaryText);
    setColour (juce::AlertWindow::outlineColourId,        t.menuBorder);

    setColour (juce::BubbleComponent::backgroundColourId, t.tooltipBackground);
    setColour (juce::BubbleComponent::outlineColourId,    t.menuBorder);

    setColour (juce::ListBox::backgroundColourId,         t.panelSunken);
    setColour (juce::ListBox::outlineColourId,            t.panelBorderSoft);
    setColour (juce::ListBox::textColourId,               t.primaryText);

    setColour (juce::GroupComponent::outlineColourId,     t.panelBorderSoft);
    setColour (juce::GroupComponent::textColourId,        t.secondaryText);

    setColour (juce::ProgressBar::backgroundColourId,     t.panelSunken);
    setColour (juce::ProgressBar::foregroundColourId,     t.cyan);

    setColour (juce::HyperlinkButton::textColourId,       t.cyan);
}

//==============================================================================
void RippleLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPosProportional, float rotaryStartAngle,
                                          float rotaryEndAngle, juce::Slider& slider)
{
    juce::ignoreUnused (rotaryStartAngle, rotaryEndAngle);

    const auto& t = RippleTheme::get();
    const auto diameter = juce::jmin (width, height);
    const auto range = slider.getRange();

    ui::KnobStyle style;
    style.accent  = slider.findColour (juce::Slider::rotarySliderFillColourId);
    style.value01 = sliderPosProportional;
    style.hover   = slider.isMouseOverOrDragging() ? 1.0f : 0.0f;
    style.enabled = slider.isEnabled();
    style.focused = slider.hasKeyboardFocus (false);
    style.bipolar = range.getStart() < 0.0 && range.getEnd() > 0.0;
    style.detail  = diameter >= t.knobLargeDiameter ? t.knobDetailLarge
                  : diameter >= t.knobSmallDiameter ? t.knobDetailMedium
                                                    : t.knobDetailSmall;

    ui::drawKnob (g, juce::Rectangle<int> (x, y, width, height).toFloat(), style);
}

void RippleLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float minSliderPos, float maxSliderPos,
                                          juce::Slider::SliderStyle style, juce::Slider& slider)
{
    juce::ignoreUnused (minSliderPos, maxSliderPos, style);

    const auto& t = RippleTheme::get();
    const auto bounds  = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto enabled = slider.isEnabled();
    const auto accent  = slider.findColour (juce::Slider::thumbColourId);
    const auto thick   = t.knobArcThickness;
    const auto thumbR  = (float) getSliderThumbRadius (slider);
    const auto vertical = slider.isVertical();

    const auto track = vertical ? juce::Rectangle<float> (thick, bounds.getHeight()).withCentre (bounds.getCentre())
                                : juce::Rectangle<float> (bounds.getWidth(), thick).withCentre (bounds.getCentre());

    g.setColour (ui::dimForState (t.knobTrack, enabled));
    g.fillRoundedRectangle (track, thick * 0.5f);

    auto filled = track;

    if (vertical)
        filled = filled.withTop (sliderPos);
    else
        filled = filled.withRight (sliderPos);

    if (! filled.isEmpty())
    {
        g.setColour (ui::dimForState (accent, enabled));
        g.fillRoundedRectangle (filled, thick * 0.5f);
    }

    const auto thumbCentre = vertical ? juce::Point<float> (bounds.getCentreX(), sliderPos)
                                      : juce::Point<float> (sliderPos, bounds.getCentreY());

    ui::drawSoftCircleShadow (g, thumbCentre, thumbR, t.knobShadow, t.knobShadowSpread * 0.5f, t.knobShadowOffset * 0.5f);

    g.setColour (ui::dimForState (t.knobIndicator, enabled));
    g.fillEllipse (juce::Rectangle<float> (thumbR * 2.0f, thumbR * 2.0f).withCentre (thumbCentre));

    if (slider.hasKeyboardFocus (false))
        ui::drawFocusRing (g, bounds.reduced (t.focusRingPadding), t.smallRadius);
}

int RippleLookAndFeel::getSliderThumbRadius (juce::Slider& slider)
{
    const auto& t = RippleTheme::get();
    return juce::jmin (t.sm, slider.getHeight() / 2, slider.getWidth() / 2);
}

juce::Label* RippleLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = juce::LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (RippleTheme::get().valueFont());
    label->setJustificationType (juce::Justification::centred);
    return label;
}

//==============================================================================
void RippleLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                              const juce::Colour& backgroundColour,
                                              bool shouldDrawButtonAsHighlighted,
                                              bool shouldDrawButtonAsDown)
{
    const auto& t = RippleTheme::get();
    const auto bounds  = button.getLocalBounds().toFloat().reduced (t.borderWidth * 0.5f);
    const auto enabled = button.isEnabled();

    // A bright background colour means "primary": fill it. Otherwise it is a
    // flat well, which is the default across the instrument.
    if (backgroundColour.getPerceivedBrightness() > 0.5f)
    {
        auto fill = backgroundColour;

        if (shouldDrawButtonAsDown)            fill = fill.darker (t.pressDarken);
        else if (shouldDrawButtonAsHighlighted) fill = fill.brighter (t.hoverBrighten);

        g.setColour (ui::dimForState (fill, enabled));
        g.fillRoundedRectangle (bounds, t.controlRadius);
    }
    else
    {
        ui::drawControlWell (g, bounds, t.controlRadius, t.controlBorderHover,
                             shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown, enabled);
    }

    if (button.hasKeyboardFocus (false))
        ui::drawFocusRing (g, bounds.reduced (t.focusRingPadding), t.controlRadius);
}

void RippleLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                        bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused (shouldDrawButtonAsDown);

    const auto& t = RippleTheme::get();
    const auto on = button.getToggleState();
    auto colour = button.findColour (on ? juce::TextButton::textColourOnId
                                        : juce::TextButton::textColourOffId);

    if (shouldDrawButtonAsHighlighted)
        colour = colour.brighter (t.hoverBrighten);

    g.setFont (getTextButtonFont (button, button.getHeight()));
    g.setColour (ui::dimForState (colour, button.isEnabled()));
    g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (t.sm, 0),
                      juce::Justification::centred, 1, 0.9f);
}

void RippleLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                          bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    const auto& t = RippleTheme::get();
    auto bounds = button.getLocalBounds();

    const auto trackBounds = juce::Rectangle<int> (t.toggleWidth, t.toggleHeight)
                                 .withCentre ({ bounds.getX() + t.toggleWidth / 2, bounds.getCentreY() });

    ui::drawSwitch (g, trackBounds.toFloat(),
                    button.findColour (juce::ToggleButton::tickColourId),
                    button.getToggleState() ? 1.0f : 0.0f,
                    shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown,
                    button.isEnabled(), button.hasKeyboardFocus (false));

    if (button.getButtonText().isNotEmpty())
    {
        g.setFont (t.labelFont());
        g.setColour (ui::dimForState (button.findColour (juce::ToggleButton::textColourId), button.isEnabled()));
        g.drawFittedText (button.getButtonText(), bounds.withTrimmedLeft (t.toggleWidth + t.sm),
                          juce::Justification::centredLeft, 1, 0.9f);
    }
}

juce::Font RippleLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    const auto& t = RippleTheme::get();
    return buttonHeight > t.buttonHeight ? t.bodyFont() : t.valueFont();
}

//==============================================================================
void RippleLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                                      int buttonX, int buttonY, int buttonW, int buttonH,
                                      juce::ComboBox& box)
{
    const auto& t = RippleTheme::get();
    const auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (t.borderWidth * 0.5f);
    const auto accent = box.findColour (juce::ComboBox::arrowColourId);
    const auto hovered = box.isMouseOver (true);

    ui::drawControlWell (g, bounds, t.controlRadius,
                         accent.withAlpha (t.controlBorderHover.getFloatAlpha()),
                         hovered, isButtonDown, box.isEnabled());

    const auto arrowArea = juce::Rectangle<int> (buttonX, buttonY, buttonW, buttonH).toFloat();
    ui::drawChevron (g, arrowArea, ui::dimForState (accent, box.isEnabled()), t.chevronThickness);

    if (box.hasKeyboardFocus (true))
        ui::drawFocusRing (g, bounds.reduced (t.focusRingPadding), t.controlRadius);
}

juce::Font RippleLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return RippleTheme::get().valueFont();
}

void RippleLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    const auto& t = RippleTheme::get();
    label.setBounds (t.md, 0, box.getWidth() - t.md - t.xl, box.getHeight());
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

//==============================================================================
void RippleLookAndFeel::drawPopupMenuBackgroundWithOptions (juce::Graphics& g, int width, int height,
                                                            const juce::PopupMenu::Options& options)
{
    juce::ignoreUnused (options);

    const auto& t = RippleTheme::get();
    const auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (t.borderWidth * 0.5f);

    g.setColour (t.menuBackground);
    g.fillRoundedRectangle (bounds, t.controlRadius);

    // The same single mild highlight the panels use, so the popup reads as
    // part of the same material.
    {
        const juce::Graphics::ScopedSaveState state (g);
        juce::Path clip;
        clip.addRoundedRectangle (bounds, t.controlRadius);
        g.reduceClipRegion (clip, {});

        auto band = bounds.withHeight (bounds.getHeight() * t.panelHighlightRatio);
        g.setGradientFill ({ t.panelHighlight,            bounds.getCentreX(), bounds.getY(),
                             t.panelHighlight.withAlpha (0.0f), bounds.getCentreX(), band.getBottom(), false });
        g.fillRect (band);
    }

    g.setColour (t.menuBorder);
    g.drawRoundedRectangle (bounds, t.controlRadius, t.borderWidth);
}

void RippleLookAndFeel::drawPopupMenuItemWithOptions (juce::Graphics& g, const juce::Rectangle<int>& area,
                                                      bool isHighlighted, const juce::PopupMenu::Item& item,
                                                      const juce::PopupMenu::Options& options)
{
    juce::ignoreUnused (options);

    const auto& t = RippleTheme::get();

    if (item.isSeparator)
    {
        const auto line = area.toFloat().withSizeKeepingCentre ((float) area.getWidth() - (float) t.md * 2.0f,
                                                                t.borderWidth);
        g.setColour (t.menuSeparator);
        g.fillRect (line);
        return;
    }

    const auto enabled = item.isEnabled;
    auto row = area.reduced (t.xs, 0);

    if (isHighlighted && enabled)
    {
        g.setColour (t.menuHighlight);
        g.fillRoundedRectangle (row.toFloat(), t.smallRadius);
    }

    auto content  = row.reduced (t.sm, 0);
    auto tickArea = content.removeFromLeft (t.md);

    auto textColour = item.colour.isTransparent() ? (isHighlighted && enabled ? t.cyanBright : t.primaryText)
                                                  : item.colour;
    textColour = ui::dimForState (textColour, enabled);

    // A tick is a filled dot plus the ticked item's brighter text, never
    // colour alone.
    if (item.isTicked)
    {
        const auto dot = juce::jmin ((float) t.xs * 0.6f, (float) tickArea.getHeight() * 0.2f);
        g.setColour (ui::dimForState (t.cyan, enabled));
        g.fillEllipse (juce::Rectangle<float> (dot * 2.0f, dot * 2.0f).withCentre (tickArea.toFloat().getCentre()));
    }

    if (item.image != nullptr)
        item.image->drawWithin (g, tickArea.toFloat(), juce::RectanglePlacement::centred, 1.0f);

    if (item.subMenu != nullptr)
    {
        auto arrow = content.removeFromRight (t.md).toFloat();
        const auto c = arrow.getCentre();
        const auto s = (float) t.chevronSize * 0.7f;

        juce::Path p;
        p.startNewSubPath (c.x - s * 0.5f, c.y - s);
        p.lineTo          (c.x + s * 0.5f, c.y);
        p.lineTo          (c.x - s * 0.5f, c.y + s);

        g.setColour (ui::dimForState (t.secondaryText, enabled));
        g.strokePath (p, juce::PathStrokeType (t.chevronThickness, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }

    if (item.shortcutKeyDescription.isNotEmpty())
    {
        auto shortcut = content.removeFromRight (content.getWidth() / 3);
        g.setFont (t.smallFont());
        g.setColour (ui::dimForState (t.tertiaryText, enabled));
        g.drawFittedText (item.shortcutKeyDescription, shortcut, juce::Justification::centredRight, 1, 0.9f);
    }

    g.setFont (getPopupMenuFont());
    g.setColour (textColour);
    g.drawFittedText (item.text, content.withTrimmedLeft (t.xs), juce::Justification::centredLeft, 1, 0.9f);
}

void RippleLookAndFeel::getIdealPopupMenuItemSizeWithOptions (const juce::String& text, bool isSeparator,
                                                              int standardMenuItemHeight, int& idealWidth,
                                                              int& idealHeight,
                                                              const juce::PopupMenu::Options& options)
{
    juce::ignoreUnused (options);

    const auto& t = RippleTheme::get();

    if (isSeparator)
    {
        idealWidth  = t.xxl;
        idealHeight = t.sm;
        return;
    }

    const auto font = getPopupMenuFont();
    idealHeight = juce::jmax (standardMenuItemHeight, t.menuItemHeight);
    idealWidth  = juce::GlyphArrangement::getStringWidthInt (font, text) + idealHeight * 2;
}

void RippleLookAndFeel::drawPopupMenuSectionHeaderWithOptions (juce::Graphics& g, const juce::Rectangle<int>& area,
                                                               const juce::String& sectionName,
                                                               const juce::PopupMenu::Options& options)
{
    juce::ignoreUnused (options);

    const auto& t = RippleTheme::get();
    g.setFont (t.labelFont());
    g.setColour (t.tertiaryText);
    g.drawFittedText (sectionName.toUpperCase(), area.reduced (t.md, 0),
                      juce::Justification::centredLeft, 1, 0.9f);
}

juce::Font RippleLookAndFeel::getPopupMenuFont()
{
    return RippleTheme::get().bodyFont();
}

int RippleLookAndFeel::getPopupMenuBorderSize()
{
    return RippleTheme::get().menuBorderSize;
}

//==============================================================================
juce::Font RippleLookAndFeel::getLabelFont (juce::Label& label)
{
    // Keep the caller's size, but always the instrument's typeface.
    return RippleTheme::withTracking (label.getFont().getHeight(), juce::Font::plain, 0.0f);
}

void RippleLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    const auto& t = RippleTheme::get();

    g.fillAll (label.findColour (juce::Label::backgroundColourId));

    if (! label.isBeingEdited())
    {
        const auto alpha = label.isEnabled() ? 1.0f : t.disabledAlpha;
        const auto font  = getLabelFont (label);
        const auto area  = label.getBorderSize().subtractedFrom (label.getLocalBounds());

        g.setColour (label.findColour (juce::Label::textColourId).withMultipliedAlpha (alpha));
        g.setFont (font);
        g.drawFittedText (label.getText(), area, label.getJustificationType(),
                          juce::jmax (1, (int) ((float) area.getHeight() / font.getHeight())),
                          label.getMinimumHorizontalScale());

        g.setColour (label.findColour (juce::Label::outlineColourId).withMultipliedAlpha (alpha));
    }
    else
    {
        g.setColour (label.findColour (juce::Label::outlineColourId));
    }

    g.drawRect (label.getLocalBounds());
}

//==============================================================================
void RippleLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height,
                                                  juce::TextEditor& editor)
{
    const auto& t = RippleTheme::get();
    const auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (t.borderWidth * 0.5f);

    g.setColour (editor.findColour (juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle (bounds, t.smallRadius);
}

void RippleLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height,
                                               juce::TextEditor& editor)
{
    const auto& t = RippleTheme::get();
    const auto bounds  = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (t.borderWidth * 0.5f);
    const auto focused = editor.hasKeyboardFocus (true);

    g.setColour (editor.findColour (focused ? juce::TextEditor::focusedOutlineColourId
                                            : juce::TextEditor::outlineColourId));
    g.drawRoundedRectangle (bounds, t.smallRadius, focused ? t.focusRingWidth : t.borderWidth);
}

//==============================================================================
int RippleLookAndFeel::getDefaultScrollbarWidth()
{
    return RippleTheme::get().scrollbarWidth;
}

void RippleLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar& scrollbar,
                                       int x, int y, int width, int height,
                                       bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                                       bool isMouseOver, bool isMouseDown)
{
    const auto& t = RippleTheme::get();
    const auto area = juce::Rectangle<int> (x, y, width, height).toFloat();

    g.setColour (scrollbar.findColour (juce::ScrollBar::backgroundColourId));
    g.fillRect (area);

    if (thumbSize <= 0)
        return;

    const auto thumb = (isScrollbarVertical
                            ? juce::Rectangle<int> (x, thumbStartPosition, width, thumbSize)
                            : juce::Rectangle<int> (thumbStartPosition, y, thumbSize, height))
                          .toFloat().reduced (t.borderWidth * 2.0f);

    const auto colour = (isMouseOver || isMouseDown) ? t.scrollbarThumbOver
                                                     : scrollbar.findColour (juce::ScrollBar::thumbColourId);

    g.setColour (colour);
    g.fillRoundedRectangle (thumb, juce::jmin (thumb.getWidth(), thumb.getHeight()) * 0.5f);
}

//==============================================================================
void RippleLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto& t = RippleTheme::get();
    const auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (t.borderWidth * 0.5f);

    g.setColour (findColour (juce::TooltipWindow::backgroundColourId));
    g.fillRoundedRectangle (bounds, t.smallRadius);

    g.setColour (findColour (juce::TooltipWindow::outlineColourId));
    g.drawRoundedRectangle (bounds, t.smallRadius, t.borderWidth);

    g.setColour (findColour (juce::TooltipWindow::textColourId));
    g.setFont (t.valueFont());
    g.drawFittedText (text, juce::Rectangle<int> (0, 0, width, height).reduced (t.tooltipPadding, t.xs),
                      juce::Justification::centred, 2, 0.9f);
}

juce::Rectangle<int> RippleLookAndFeel::getTooltipBounds (const juce::String& tipText,
                                                          juce::Point<int> screenPos,
                                                          juce::Rectangle<int> parentArea)
{
    const auto& t = RippleTheme::get();
    const auto font = t.valueFont();

    const auto w = juce::GlyphArrangement::getStringWidthInt (font, tipText) + t.tooltipPadding * 2;
    const auto h = (int) font.getHeight() + t.sm;

    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + t.md) : screenPos.x + t.md,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + t.sm) : screenPos.y + t.xl,
                                 w, h).constrainedWithin (parentArea);
}

} // namespace ripples
