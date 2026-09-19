#include "UI/Components/RippleKnob.h"

#include "UI/Theme/RippleLookAndFeel.h"

namespace ripples
{

namespace
{
    bool isFineModifier (const juce::ModifierKeys& mods) noexcept
    {
        return mods.isShiftDown() || mods.isCtrlDown() || mods.isCommandDown();
    }
}

//==============================================================================
void RippleKnob::KnobSlider::mouseWheelMove (const juce::MouseEvent& e,
                                             const juce::MouseWheelDetails& wheel)
{
    if (isFineModifier (e.mods))
    {
        // Same gesture, smaller steps — no separate fine-adjust mode to learn.
        auto fine = wheel;
        fine.deltaX *= RippleTheme::get().knobWheelFineScale;
        fine.deltaY *= RippleTheme::get().knobWheelFineScale;
        juce::Slider::mouseWheelMove (e, fine);
        return;
    }

    juce::Slider::mouseWheelMove (e, wheel);
}

void RippleKnob::KnobSlider::focusGained (juce::Component::FocusChangeType type)
{
    juce::Slider::focusGained (type);
    owner.repaint();
}

void RippleKnob::KnobSlider::focusLost (juce::Component::FocusChangeType type)
{
    juce::Slider::focusLost (type);
    owner.repaint();
}

//==============================================================================
RippleKnob::RippleKnob (const juce::String& label, Size size)
    : labelText (label), knobSize (size)
{
    const auto& theme = RippleTheme::get();

    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (theme.knobStartAngle, theme.knobEndAngle, true);
    slider.setMouseDragSensitivity (theme.knobDragPixels);

    // Holding Shift / Ctrl / Cmd swaps into velocity mode, which is the fine
    // adjustment. JUCE re-anchors the pointer when the modifier is released,
    // so the value never jumps.
    slider.setVelocityBasedMode (false);
    slider.setVelocityModeParameters ((double) theme.knobFineVelocity,
                                      theme.knobVelocityThreshold,
                                      0.0, true,
                                      (juce::ModifierKeys::Flags) (juce::ModifierKeys::shiftModifier
                                                                 | juce::ModifierKeys::ctrlModifier
                                                                 | juce::ModifierKeys::commandModifier));

    slider.setWantsKeyboardFocus (true);
    slider.setTitle (labelText);
    slider.addListener (this);
    slider.addMouseListener (this, false);

    addAndMakeVisible (slider);

    setTitle (labelText);
    setInterceptsMouseClicks (false, true);
}

RippleKnob::~RippleKnob()
{
    stopTimer();
    slider.removeMouseListener (this);
    slider.removeListener (this);
    attachment.reset();
}

//==============================================================================
int RippleKnob::getDiameter (Size size)
{
    const auto& theme = RippleTheme::get();

    switch (size)
    {
        case Size::Large:  return theme.knobLargeDiameter;
        case Size::Small:  return theme.knobSmallDiameter;
        case Size::Medium:
        default:           return theme.knobMediumDiameter;
    }
}

juce::Point<int> RippleKnob::getPreferredSize (Size size)
{
    const auto& theme = RippleTheme::get();
    const auto d = getDiameter (size);

    return { d + theme.md,
             theme.knobLabelHeight + d + theme.knobValueHeight + theme.xs };
}

float RippleKnob::getDetailLevel() const
{
    const auto& theme = RippleTheme::get();

    switch (knobSize)
    {
        case Size::Large:  return theme.knobDetailLarge;
        case Size::Small:  return theme.knobDetailSmall;
        case Size::Medium:
        default:           return theme.knobDetailMedium;
    }
}

juce::Font RippleKnob::getLabelFontForSize() const
{
    const auto& theme = RippleTheme::get();
    return knobSize == Size::Small ? theme.smallFont() : theme.labelFont();
}

juce::Font RippleKnob::getValueFontForSize() const
{
    const auto& theme = RippleTheme::get();
    return knobSize == Size::Small ? theme.smallFont() : theme.valueFont();
}

//==============================================================================
void RippleKnob::attach (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID)
{
    attachment.reset();
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramID, slider);

    // The attachment brings the parameter's range, its text formatting and its
    // default value (used by double-click) across with it.
    if (auto* param = apvts.getParameter (paramID))
    {
        const auto name = param->getName (64);

        if (labelText.isEmpty())
            setLabel (name);

        slider.setTitle (name);
        slider.setDescription (name);
    }

    repaint();
}

void RippleKnob::setAccent (juce::Colour accent)
{
    if (accentColour == accent)
        return;

    accentColour = accent;
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    repaint (knobArea);
}

void RippleKnob::setLabel (const juce::String& label)
{
    if (labelText == label)
        return;

    labelText = label;
    slider.setTitle (labelText);
    setTitle (labelText);
    repaint (labelArea);
}

void RippleKnob::setTooltipText (const juce::String& tip)
{
    tooltipText = tip;
    slider.setTooltip (tip);
    slider.setHelpText (tip);
    setHelpText (tip);
}

void RippleKnob::setModulationAmount (float amount)
{
    const auto clamped = juce::jlimit (-1.0f, 1.0f, amount);

    if (std::abs (clamped - modulationAmount) < RippleTheme::get().modRingEpsilon)
        return;

    modulationAmount = clamped;
    repaint (knobArea);
}

juce::Slider& RippleKnob::getSlider() noexcept
{
    return slider;
}

float RippleKnob::getNormalisedValue()
{
    return (float) slider.valueToProportionOfLength (slider.getValue());
}

//==============================================================================
void RippleKnob::resized()
{
    const auto& theme = RippleTheme::get();

    auto area = getLocalBounds();

    labelArea = area.removeFromTop (theme.knobLabelHeight);
    valueArea = area.removeFromBottom (theme.knobValueHeight);

    const auto diameter = juce::jmin (area.getWidth(), area.getHeight());
    knobArea = area.withSizeKeepingCentre (diameter, diameter);

    // The slider covers the whole control, so a drag started on the label
    // works exactly like a drag started on the body.
    slider.setBounds (getLocalBounds());
}

void RippleKnob::paint (juce::Graphics& g)
{
    const auto& theme = RippleTheme::get();
    const auto enabled = isEnabled();
    const auto range = slider.getRange();

    ui::KnobStyle style;
    style.accent    = accentColour;
    style.value01   = getNormalisedValue();
    style.hover     = hoverAmount;
    style.modAmount = modulationAmount;
    style.detail    = getDetailLevel();
    style.enabled   = enabled;
    style.focused   = slider.hasKeyboardFocus (false);
    style.bipolar   = range.getStart() < 0.0 && range.getEnd() > 0.0;

    ui::drawKnob (g, knobArea.toFloat(), style);

    // Never an unlabelled mystery control: name above, value below.
    if (labelText.isNotEmpty())
    {
        g.setFont (getLabelFontForSize());
        g.setColour (enabled ? theme.secondaryText : theme.disabledText);
        g.drawFittedText (labelText.toUpperCase(), labelArea, juce::Justification::centred, 1, 0.8f);
    }

    const auto valueText = slider.getTextFromValue (slider.getValue());

    if (valueText.isNotEmpty())
    {
        g.setFont (getValueFontForSize());
        g.setColour (enabled ? theme.primaryText.interpolatedWith (accentColour, hoverAmount * 0.5f)
                             : theme.disabledText);
        g.drawFittedText (valueText, valueArea, juce::Justification::centred, 1, 0.8f);
    }
}

//==============================================================================
void RippleKnob::mouseEnter (const juce::MouseEvent&)
{
    setHovered (true);
}

void RippleKnob::mouseExit (const juce::MouseEvent&)
{
    refreshHoverFromMouse();
}

void RippleKnob::refreshHoverFromMouse()
{
    setHovered (isMouseOver (true) || slider.isMouseOverOrDragging());
}

void RippleKnob::enablementChanged()
{
    slider.setEnabled (isEnabled());
    refreshHoverFromMouse();
    repaint();
}

void RippleKnob::sliderValueChanged (juce::Slider*)
{
    repaint();
}

//==============================================================================
void RippleKnob::setHovered (bool shouldBeHovered)
{
    const auto target = (shouldBeHovered && isEnabled()) ? 1.0f : 0.0f;

    if (juce::approximatelyEqual (hoverTarget, target))
        return;

    hoverTarget = target;

    // Hover lifts the control over hoverFadeSeconds — never a hard snap.
    if (! isTimerRunning())
        startTimerHz (juce::jmax (1, RippleTheme::get().targetFrameRate));
}

void RippleKnob::timerCallback()
{
    const auto& theme = RippleTheme::get();
    const auto dt = 1.0f / (float) juce::jmax (1, theme.targetFrameRate);
    const auto step = theme.hoverFadeSeconds > 0.0f ? dt / theme.hoverFadeSeconds : 1.0f;

    hoverAmount = hoverAmount < hoverTarget ? juce::jmin (hoverTarget, hoverAmount + step)
                                            : juce::jmax (hoverTarget, hoverAmount - step);

    if (std::abs (hoverAmount - hoverTarget) <= theme.modRingEpsilon)
    {
        hoverAmount = hoverTarget;
        stopTimer();
    }

    repaint();
}

} // namespace ripples
