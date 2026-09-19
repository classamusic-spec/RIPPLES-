#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "UI/Theme/RippleTheme.h"

namespace ripples
{

//==============================================================================
/**
    A small modern switch.

    State is never signalled by colour alone: the thumb moves, and it changes
    shape — a hollow ring when off, a solid disc when on — so the control stays
    readable without colour vision, and while disabled.
*/
class RippleToggle : public juce::Component,
                     private juce::Value::Listener,
                     private juce::Timer
{
public:
    explicit RippleToggle (const juce::String& text = {});
    ~RippleToggle() override;

    void attach (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID);
    void setAccent (juce::Colour accent);

    juce::ToggleButton& getButton() noexcept;

    /** The size this switch would like to be, including its text. */
    juce::Point<int> getPreferredSize() const;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit  (const juce::MouseEvent&) override;
    void mouseDown  (const juce::MouseEvent&) override;
    void mouseUp    (const juce::MouseEvent&) override;
    void enablementChanged() override;

private:
    /** Clicks, keyboard and accessibility live here; RippleToggle paints. */
    class SwitchButton : public juce::ToggleButton
    {
    public:
        explicit SwitchButton (RippleToggle& o) : owner (o) {}
        void paintButton (juce::Graphics&, bool, bool) override {}
        void focusGained (juce::Component::FocusChangeType) override;
        void focusLost   (juce::Component::FocusChangeType) override;

    private:
        RippleToggle& owner;
    };

    void valueChanged (juce::Value&) override;
    void timerCallback() override;
    void startAnimation();

    SwitchButton button { *this };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;

    juce::Value  toggleState;
    juce::Colour accentColour { RippleTheme::get().cyan };

    float onAmount = 0.0f;
    juce::Rectangle<int> switchArea, textArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RippleToggle)
};

} // namespace ripples
