#include "UI/Components/RippleButton.h"

#include "UI/Theme/RippleLookAndFeel.h"

namespace ripples
{

RippleButton::RippleButton (const juce::String& text)
    : juce::TextButton (text)
{
    setTitle (text);
}

void RippleButton::setAccent (juce::Colour accent)
{
    if (accentColour == accent)
        return;

    accentColour = accent;
    repaint();
}

void RippleButton::setIsPrimary (bool shouldBePrimary)
{
    if (primary == shouldBePrimary)
        return;

    primary = shouldBePrimary;
    repaint();
}

void RippleButton::paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted,
                                bool shouldDrawButtonAsDown)
{
    const auto& theme = RippleTheme::get();
    const auto enabled = isEnabled();
    const auto bounds  = getLocalBounds().toFloat().reduced (theme.borderWidth * 0.5f);

    juce::Colour textColour;

    if (primary || getToggleState())
    {
        auto fill = accentColour;

        if (shouldDrawButtonAsDown)             fill = fill.darker (theme.pressDarken);
        else if (shouldDrawButtonAsHighlighted) fill = fill.brighter (theme.hoverBrighten);

        g.setColour (ui::dimForState (fill, enabled));
        g.fillRoundedRectangle (bounds, theme.controlRadius);

        // Dark type on the lit fill keeps the contrast high both ways round.
        textColour = theme.background;
    }
    else
    {
        ui::drawControlWell (g, bounds, theme.controlRadius,
                             accentColour.withAlpha (theme.controlBorderHover.getFloatAlpha()),
                             shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown, enabled);

        textColour = shouldDrawButtonAsHighlighted ? theme.primaryText : theme.secondaryText;
    }

    if (hasKeyboardFocus (false))
        ui::drawFocusRing (g, bounds.reduced (theme.focusRingPadding), theme.controlRadius);

    if (getButtonText().isNotEmpty())
    {
        g.setFont (getHeight() > theme.buttonHeight ? theme.bodyFont() : theme.valueFont());
        g.setColour (ui::dimForState (textColour, enabled));
        g.drawFittedText (getButtonText(), getLocalBounds().reduced (theme.md, 0),
                          juce::Justification::centred, 1, 0.85f);
    }
}

} // namespace ripples
