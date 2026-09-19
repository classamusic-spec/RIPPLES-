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
    repaint();
}

void GlassPanel::setAccent (juce::Colour accent)
{
    if (accentColour == accent)
        return;

    accentColour = accent;
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

void GlassPanel::resized()
{
    const auto& theme = RippleTheme::get();

    glassArea = getGlassBounds().toFloat();

    titleArea = getGlassBounds().reduced (contentInset);
    titleArea = titleArea.removeFromTop (theme.panelTitleHeight);
}

void GlassPanel::paint (juce::Graphics& g)
{
    const auto& theme = RippleTheme::get();

    if (glassArea.isEmpty())
        return;

    // Depth first: a soft outer shadow, then exactly one layer of glass.
    ui::drawSoftShadow (g, glassArea, theme.panelRadius, theme.shadow,
                        theme.panelShadowSpread * 0.5f, theme.panelShadowOffset * 0.5f);

    ui::drawGlassSurface (g, glassArea, theme.panelRadius, accentColour, true);

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
