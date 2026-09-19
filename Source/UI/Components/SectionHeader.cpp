#include "UI/Components/SectionHeader.h"

namespace ripples
{

SectionHeader::SectionHeader (const juce::String& title, const juce::String& subtitle)
    : titleText (title), subtitleText (subtitle)
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

void SectionHeader::paint (juce::Graphics& g)
{
    const auto& theme = RippleTheme::get();

    auto area = getLocalBounds();

    if (area.isEmpty())
        return;

    // The accent tick is the only saturated mark in the header.
    auto tick = area.removeFromLeft ((int) theme.sectionTickWidth)
                    .withSizeKeepingCentre ((int) theme.sectionTickWidth, theme.sectionTickHeight);

    g.setColour (accentColour);
    g.fillRoundedRectangle (tick.toFloat(), theme.sectionTickWidth * 0.5f);

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
