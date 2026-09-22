#include "UI/Components/SectionHeader.h"

#include <array>

namespace ripples
{

namespace
{
    /** How far the cyan glyph is pulled toward the panel's own accent. Keeps
        glyphStroke in charge of how bright the icon is, while a violet panel
        still gets a violet-leaning icon. */
    constexpr float kGlyphAccentTint = 0.40f;

    /** Cubic control offset that makes a bezier half-arch peak at exactly the
        amplitude asked for — the usual 4/3. */
    constexpr float kSineControl = 4.0f / 3.0f;

    //==========================================================================
    /** One icon, drawn inside the unit square (0,0)-(1,1) with y pointing down.
        A unit path can be scaled to any glyph size by a transform at paint
        time, so nothing is ever rebuilt because a window was resized. */
    struct GlyphArt
    {
        juce::Path path;
        bool       filled = false;   // dots are filled; every other icon is stroked
    };

    /** Appends one period of a sine, centred on `mid` with amplitude `amp`,
        running from x0 to x1. Two cubics — smooth at any zoom, unlike sampling. */
    void addSine (juce::Path& p, float x0, float x1, float mid, float amp, bool downFirst)
    {
        const auto half = (x1 - x0) * 0.5f;
        const auto c    = amp * kSineControl * (downFirst ? -1.0f : 1.0f);

        p.startNewSubPath (x0, mid);
        p.cubicTo (x0 + half / 3.0f,        mid - c,
                   x0 + half * 2.0f / 3.0f, mid - c,
                   x0 + half,               mid);
        p.cubicTo (x0 + half * 4.0f / 3.0f, mid + c,
                   x0 + half * 5.0f / 3.0f, mid + c,
                   x1,                      mid);
    }

    void addDot (juce::Path& p, float cx, float cy, float r)
    {
        p.addEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    }

    std::array<GlyphArt, 7> buildGlyphArt()
    {
        std::array<GlyphArt, 7> art;

        // Tick — never drawn from a path (the rounded bar is cheaper and keeps
        // the pre-existing look byte for byte), but the slot has to exist.

        // Wave — one clean period.
        addSine (art[(size_t) SectionHeader::Glyph::Wave].path,
                 0.06f, 0.94f, 0.50f, 0.36f, false);

        // DualWave — a quieter wave stacked over a louder one. They are kept
        // clear of each other: at eighteen pixels two crossing waves silt up
        // into an X and stop reading as two layers at all.
        {
            auto& p = art[(size_t) SectionHeader::Glyph::DualWave].path;
            addSine (p, 0.08f, 0.92f, 0.26f, 0.13f, false);
            addSine (p, 0.06f, 0.94f, 0.69f, 0.21f, false);
        }

        // Filter — a flat band, a resonant bump, then the skirt falling away.
        {
            auto& p = art[(size_t) SectionHeader::Glyph::Filter].path;
            p.startNewSubPath (0.04f, 0.40f);
            p.lineTo (0.40f, 0.40f);
            p.cubicTo (0.52f, 0.40f, 0.54f, 0.10f, 0.64f, 0.12f);
            p.cubicTo (0.72f, 0.14f, 0.74f, 0.62f, 0.80f, 0.80f);
            p.cubicTo (0.85f, 0.93f, 0.90f, 0.97f, 0.96f, 0.98f);
        }

        // Envelope — attack, decay, sustain, release.
        {
            auto& p = art[(size_t) SectionHeader::Glyph::Envelope].path;
            p.startNewSubPath (0.04f, 0.94f);
            p.lineTo (0.24f, 0.07f);
            p.lineTo (0.44f, 0.46f);
            p.lineTo (0.70f, 0.46f);
            p.lineTo (0.96f, 0.94f);
        }

        // Droplet — a point at the top, a full belly at the bottom.
        {
            auto& p = art[(size_t) SectionHeader::Glyph::Droplet].path;
            p.startNewSubPath (0.50f, 0.04f);
            p.cubicTo (0.60f, 0.26f, 0.82f, 0.46f, 0.82f, 0.64f);
            p.cubicTo (0.82f, 0.85f, 0.68f, 0.97f, 0.50f, 0.97f);
            p.cubicTo (0.32f, 0.97f, 0.18f, 0.85f, 0.18f, 0.64f);
            p.cubicTo (0.18f, 0.46f, 0.40f, 0.26f, 0.50f, 0.04f);
            p.closeSubPath();
        }

        // Scatter — impacts on water. Filled, because a ring this small would
        // just silt up into a blob anyway.
        {
            auto& g = art[(size_t) SectionHeader::Glyph::Scatter];
            g.filled = true;
            addDot (g.path, 0.18f, 0.30f, 0.085f);
            addDot (g.path, 0.45f, 0.13f, 0.058f);
            addDot (g.path, 0.73f, 0.34f, 0.100f);
            addDot (g.path, 0.26f, 0.69f, 0.062f);
            addDot (g.path, 0.55f, 0.59f, 0.085f);
            addDot (g.path, 0.84f, 0.75f, 0.056f);
        }

        return art;
    }

    /** The shared artwork. Built once, on first use, and never again — every
        SectionHeader in the instrument strokes the same paths. */
    const GlyphArt& glyphArtFor (SectionHeader::Glyph glyph) noexcept
    {
        static const std::array<GlyphArt, 7> art = buildGlyphArt();

        const auto index = (size_t) glyph;
        return art[index < art.size() ? index : 0];
    }
}

//==============================================================================
SectionHeader::SectionHeader (const juce::String& title, const juce::String& subtitle, Glyph glyph)
    : titleText (title), subtitleText (subtitle), glyphKind (glyph)
{
    setInterceptsMouseClicks (false, false);
    juce::Component::setTitle (title);
}

void SectionHeader::setAccent (juce::Colour accent)
{
    if (accentColour == accent)
        return;

    accentColour = accent;
    repaint();
}

void SectionHeader::setTitle (const juce::String& title)
{
    if (titleText == title)
        return;

    titleText = title;
    juce::Component::setTitle (title);
    repaint();
}

void SectionHeader::setSubtitle (const juce::String& subtitle)
{
    if (subtitleText == subtitle)
        return;

    subtitleText = subtitle;
    repaint();
}

void SectionHeader::setGlyph (Glyph glyph)
{
    if (glyphKind == glyph)
        return;

    glyphKind = glyph;
    repaint();
}

void SectionHeader::paint (juce::Graphics& g)
{
    const auto& theme = RippleTheme::get();

    auto area = getLocalBounds();

    if (area.isEmpty())
        return;

    //--- the glyph, the only saturated mark in the header ---------------------
    if (glyphKind == Glyph::Tick)
    {
        auto tick = area.removeFromLeft ((int) theme.sectionTickWidth)
                        .withSizeKeepingCentre ((int) theme.sectionTickWidth, theme.sectionTickHeight);

        g.setColour (accentColour);
        g.fillRoundedRectangle (tick.toFloat(), theme.sectionTickWidth * 0.5f);
    }
    else
    {
        // The reserved width never changes with the icon, so titles line up
        // across a row of panels whatever glyphs they carry.
        const auto box  = area.removeFromLeft (juce::roundToInt (theme.glyphSize)).toFloat();
        const auto side = juce::jmin (theme.glyphSize, box.getWidth(), box.getHeight());
        const auto cell = box.withSizeKeepingCentre (side, side);

        const auto& art = glyphArtFor (glyphKind);
        const auto transform = juce::AffineTransform::scale (side)
                                   .translated (cell.getX(), cell.getY());

        g.setColour (theme.glyphStroke.interpolatedWith (accentColour, kGlyphAccentTint));

        if (art.filled)
            g.fillPath (art.path, transform);
        else
            g.strokePath (art.path, juce::PathStrokeType (theme.glyphStrokeWidth,
                                                          juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded),
                          transform);
    }

    area.removeFromLeft (theme.sm);

    const auto titleFont    = theme.sectionFont();
    const auto subtitleFont = theme.labelFont();
    const auto upperTitle   = titleText.toUpperCase();

    if (upperTitle.isNotEmpty())
    {
        auto titleBox = area.removeFromLeft (juce::GlyphArrangement::getStringWidthInt (titleFont, upperTitle) + theme.xs);

        g.setFont (titleFont);
        g.setColour (theme.primaryText);
        g.drawFittedText (upperTitle, titleBox, juce::Justification::centredLeft, 1, 0.85f);
    }

    if (subtitleText.isNotEmpty() && ! area.isEmpty())
    {
        const auto upperSubtitle = subtitleText.toUpperCase();

        auto separatorBox = area.removeFromLeft (theme.md);
        g.setFont (subtitleFont);
        g.setColour (theme.cyanDim);
        g.drawFittedText ("/", separatorBox, juce::Justification::centred, 1, 1.0f);

        auto subtitleBox = area.removeFromLeft (juce::GlyphArrangement::getStringWidthInt (subtitleFont, upperSubtitle) + theme.xs);

        g.setFont (subtitleFont);
        g.setColour (theme.tertiaryText);
        g.drawFittedText (upperSubtitle, subtitleBox, juce::Justification::centredLeft, 1, 0.85f);
    }

    // A quiet rule carries the eye to the edge of the section.
    area.removeFromLeft (theme.md);

    if (area.getWidth() > theme.sm)
    {
        const auto rule = juce::Rectangle<float> ((float) area.getX(),
                                                  (float) area.getCentreY() - theme.borderWidth * 0.5f,
                                                  (float) area.getWidth(),
                                                  theme.borderWidth);
        g.setColour (theme.panelBorderSoft);
        g.fillRect (rule);
    }
}

} // namespace ripples
