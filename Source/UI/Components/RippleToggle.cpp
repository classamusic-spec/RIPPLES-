#include "UI/Components/RippleToggle.h"

#include "UI/Theme/RippleLookAndFeel.h"

namespace ripples
{

void RippleToggle::SwitchButton::focusGained (juce::Component::FocusChangeType type)
{
    juce::ToggleButton::focusGained (type);
    owner.repaint();
}

void RippleToggle::SwitchButton::focusLost (juce::Component::FocusChangeType type)
{
    juce::ToggleButton::focusLost (type);
    owner.repaint();
}

//==============================================================================
RippleToggle::RippleToggle (const juce::String& text)
{
    button.setButtonText (text);
    button.setColour (juce::ToggleButton::tickColourId, accentColour);
    button.setWantsKeyboardFocus (true);
    button.addMouseListener (this, false);

    addAndMakeVisible (button);

    // Follows the toggle state however it changes — click, keyboard, host
    // automation through the attachment — without claiming onClick.
    toggleState.referTo (button.getToggleStateValue());
    toggleState.addListener (this);

    onAmount = button.getToggleState() ? 1.0f : 0.0f;

    setTitle (text);
    setInterceptsMouseClicks (false, true);
}

RippleToggle::~RippleToggle()
{
    stopTimer();
    toggleState.removeListener (this);
    button.removeMouseListener (this);
    attachment.reset();
}

//==============================================================================
void RippleToggle::attach (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID)
{
    attachment.reset();
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, paramID, button);

    if (auto* param = apvts.getParameter (paramID))
    {
        const auto name = param->getName (64);

        if (button.getButtonText().isEmpty())
            button.setButtonText (name);

        button.setTitle (name);
        setTitle (name);
    }

    onAmount = button.getToggleState() ? 1.0f : 0.0f;
    repaint();
}

void RippleToggle::setAccent (juce::Colour accent)
{
    if (accentColour == accent)
        return;

    accentColour = accent;
    button.setColour (juce::ToggleButton::tickColourId, accent);
    repaint();
}

juce::ToggleButton& RippleToggle::getButton() noexcept
{
    return button;
}

juce::Point<int> RippleToggle::getPreferredSize() const
{
    const auto& theme = RippleTheme::get();
    const auto font = theme.labelFont();

    auto width = theme.toggleWidth;

    if (button.getButtonText().isNotEmpty())
        width += theme.sm + juce::GlyphArrangement::getStringWidthInt (font, button.getButtonText().toUpperCase());

    return { width, juce::jmax (theme.toggleHeight, (int) font.getHeight() + theme.xs) };
}

//==============================================================================
void RippleToggle::resized()
{
    const auto& theme = RippleTheme::get();

    auto area = getLocalBounds();

    switchArea = area.removeFromLeft (theme.toggleWidth)
                     .withSizeKeepingCentre (theme.toggleWidth, theme.toggleHeight);

    area.removeFromLeft (theme.sm);
    textArea = area;

    button.setBounds (getLocalBounds());
}

void RippleToggle::paint (juce::Graphics& g)
{
    const auto& theme = RippleTheme::get();
    const auto enabled = isEnabled();

    ui::drawSwitch (g, switchArea.toFloat(), accentColour, onAmount,
                    button.isOver(), button.isDown(), enabled,
                    button.hasKeyboardFocus (false));

    if (button.getButtonText().isNotEmpty() && ! textArea.isEmpty())
    {
        g.setFont (theme.labelFont());
        g.setColour (enabled ? theme.secondaryText : theme.disabledText);
        g.drawFittedText (button.getButtonText().toUpperCase(), textArea,
                          juce::Justification::centredLeft, 1, 0.85f);
    }
}

//==============================================================================
void RippleToggle::mouseEnter (const juce::MouseEvent&) { repaint (switchArea); }
void RippleToggle::mouseExit  (const juce::MouseEvent&) { repaint (switchArea); }
void RippleToggle::mouseDown  (const juce::MouseEvent&) { repaint (switchArea); }
void RippleToggle::mouseUp    (const juce::MouseEvent&) { repaint (switchArea); }

void RippleToggle::enablementChanged()
{
    button.setEnabled (isEnabled());
    repaint();
}

void RippleToggle::valueChanged (juce::Value&)
{
    startAnimation();
}

void RippleToggle::startAnimation()
{
    if (! isTimerRunning())
        startTimerHz (juce::jmax (1, RippleTheme::get().targetFrameRate));
}

void RippleToggle::timerCallback()
{
    const auto& theme = RippleTheme::get();
    const auto target = button.getToggleState() ? 1.0f : 0.0f;
    const auto dt = 1.0f / (float) juce::jmax (1, theme.targetFrameRate);
    const auto step = theme.hoverFadeSeconds > 0.0f ? dt / theme.hoverFadeSeconds : 1.0f;

    onAmount = onAmount < target ? juce::jmin (target, onAmount + step)
                                 : juce::jmax (target, onAmount - step);

    if (std::abs (onAmount - target) <= theme.modRingEpsilon)
    {
        onAmount = target;
        stopTimer();
    }

    repaint (switchArea);
}

} // namespace ripples
