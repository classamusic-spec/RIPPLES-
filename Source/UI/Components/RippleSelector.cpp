#include "UI/Components/RippleSelector.h"

#include "UI/Theme/RippleLookAndFeel.h"

namespace ripples
{

RippleSelector::RippleSelector()
{
    const auto& theme = RippleTheme::get();

    combo.setJustificationType (juce::Justification::centredLeft);
    combo.setTextWhenNothingSelected ({});
    combo.setColour (juce::ComboBox::arrowColourId, accentColour);
    combo.setColour (juce::ComboBox::backgroundColourId, theme.controlFill);
    combo.setColour (juce::ComboBox::textColourId, theme.primaryText);
    combo.setColour (juce::ComboBox::outlineColourId, theme.controlBorder);
    combo.setColour (juce::ComboBox::focusedOutlineColourId, theme.focusRing);
    combo.setWantsKeyboardFocus (true);

    addAndMakeVisible (combo);
}

RippleSelector::~RippleSelector()
{
    attachment.reset();
}

void RippleSelector::setItems (const juce::StringArray& items)
{
    const auto previous = combo.getSelectedId();

    combo.clear (juce::dontSendNotification);
    combo.addItemList (items, 1);

    if (previous > 0 && previous <= items.size())
        combo.setSelectedId (previous, juce::dontSendNotification);
    else if (! items.isEmpty())
        combo.setSelectedId (1, juce::dontSendNotification);
}

void RippleSelector::attach (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID)
{
    attachment.reset();
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, paramID, combo);

    if (auto* param = apvts.getParameter (paramID))
    {
        const auto name = param->getName (64);
        combo.setTitle (name);
        setTitle (name);
    }
}

void RippleSelector::setAccent (juce::Colour accent)
{
    if (accentColour == accent)
        return;

    accentColour = accent;
    combo.setColour (juce::ComboBox::arrowColourId, accent);
    repaint();
}

juce::ComboBox& RippleSelector::getComboBox() noexcept
{
    return combo;
}

void RippleSelector::resized()
{
    combo.setBounds (getLocalBounds());
}

} // namespace ripples
