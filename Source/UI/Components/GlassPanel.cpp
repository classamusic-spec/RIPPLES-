#include "UI/Components/GlassPanel.h"

#include "UI/Theme/RippleLookAndFeel.h"

namespace ripples
{

GlassPanel::GlassPanel()
{
    // A container, not a control: clicks fall through to whatever is behind,
    // while child components still receive their own.
    setInterceptsMouseClicks (false, true);
}

GlassPanel::GlassPanel (const juce::String& title)
    : GlassPanel()
{
    setTitle (title);
}

void GlassPanel::setTitle (const juce::String& title)
{
    if (panelTitle == title)
        return;

    panelTitle = title;

    // Keep the accessibility title in step with the visible one.
    juce::Component::setTitle (title);

    resized();
    invalidateGlassCache();
    repaint();
}

void GlassPanel::setAccent (juce::Colour accent)
{
    if (accentColour == accent)
        return;

    accentColour = accent;
    invalidateGlassCache();
    repaint();
}

void GlassPanel::setContentInset (int inset)
{
    if (contentInset == inset)
        return;

    contentInset = juce::jmax (0, inset);
    resized();
    repaint();
}

juce::Rectangle<int> GlassPanel::getGlassBounds() const
{
    // A small margin so the panel's own soft shadow has somewhere to fall.
    return getLocalBounds().reduced (RippleTheme::get().xs);
}

juce::Rectangle<int> GlassPanel::getContentBounds() const
{
    const auto& theme = RippleTheme::get();

    auto inner = getGlassBounds().reduced (contentInset);

    if (panelTitle.isNotEmpty())
        inner.removeFromTop (theme.panelTitleHeight + theme.sm);

    return inner;
}

void GlassPanel::paintOverChildren (juce::Graphics& g)
{
    if (glassArea.isEmpty())
        return;

    // The near wall of the vessel, drawn over the contents.
    ensureGlassCache (g);

    if (rimLayer.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (rimLayer, getLocalBounds().toFloat());
    }
}

void GlassPanel::resized()
{
    invalidateGlassCache();

    const auto& theme = RippleTheme::get();

    glassArea = getGlassBounds().toFloat();

    titleArea = getGlassBounds().reduced (contentInset);
    titleArea = titleArea.removeFromTop (theme.panelTitleHeight);
}

void GlassPanel::ensureGlassCache (juce::Graphics& g)
{
    const auto& theme = RippleTheme::get();

    const float scale = juce::jlimit (0.5f, 4.0f,
                                      g.getInternalContext().getPhysicalPixelScaleFactor());

    const int pw = juce::jmax (1, juce::roundToInt ((float) getWidth()  * scale));
    const int ph = juce::jmax (1, juce::roundToInt ((float) getHeight() * scale));

    if (liquidLayer.isValid() && std::abs (scale - glassCacheScale) < 0.01f
         && liquidLayer.getWidth() == pw && liquidLayer.getHeight() == ph)
        return;

    glassCacheScale = scale;

    if (getWidth() < 4 || getHeight() < 4 || glassArea.isEmpty())
    {
        liquidLayer = {};
        rimLayer = {};
        return;
    }

    liquidLayer = juce::Image (juce::Image::ARGB, pw, ph, true);
    {
        juce::Graphics lg (liquidLayer);
        lg.addTransform (juce::AffineTransform::scale (scale));
        ui::drawSoftShadow (lg, glassArea, theme.panelRadius, theme.shadow,
                            theme.panelShadowSpread * 0.5f, theme.panelShadowOffset * 0.5f);
        ui::drawGlassSurface (lg, glassArea, theme.panelRadius, accentColour, true);
    }

    rimLayer = juce::Image (juce::Image::ARGB, pw, ph, true);
    {
        juce::Graphics rg (rimLayer);
        rg.addTransform (juce::AffineTransform::scale (scale));
        ui::drawGlassRim (rg, glassArea, theme.panelRadius, accentColour);
    }
}

void GlassPanel::paint (juce::Graphics& g)
{
    const auto& theme = RippleTheme::get();

    if (glassArea.isEmpty())
        return;

    // Depth first: a soft outer shadow, then the liquid inside the vessel. The
    // glass WALL is deliberately not drawn here -- it goes on in
    // paintOverChildren, so the wall sits in front of the panel's contents the
    // way a real one would. That front/back separation is what gives the panel
    // thickness instead of making it look like a printed card.
    //
    // Both layers are static for a given size, so they are cached as images
    // and blitted; drawing the gradients live made a full repaint several
    // times more expensive for no visible difference.
    ensureGlassCache (g);

    if (liquidLayer.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (liquidLayer, getLocalBounds().toFloat());
    }

    if (panelTitle.isEmpty())
        return;

    auto row = titleArea;

    // A small accent tick, then the title. The tick is the only saturated
    // element in the panel chrome.
    auto tick = row.removeFromLeft ((int) theme.sectionTickWidth * 2)
                   .withSizeKeepingCentre ((int) theme.sectionTickWidth, theme.sectionTickHeight);

    g.setColour (accentColour);
    g.fillRoundedRectangle (tick.toFloat(), theme.sectionTickWidth * 0.5f);

    row.removeFromLeft (theme.sm);

    g.setFont (theme.sectionFont());
    g.setColour (theme.secondaryText);
    g.drawFittedText (panelTitle.toUpperCase(), row, juce::Justification::centredLeft, 1, 0.9f);

    // Quiet divider between chrome and content.
    const auto divider = juce::Rectangle<float> (glassArea.getX() + (float) contentInset,
                                                 (float) titleArea.getBottom() + (float) theme.xs,
                                                 glassArea.getWidth() - (float) contentInset * 2.0f,
                                                 theme.borderWidth);
    g.setColour (theme.panelBorderSoft);
    g.fillRect (divider);
}

} // namespace ripples
