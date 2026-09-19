#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "UI/Theme/RippleTheme.h"

namespace ripples
{

//==============================================================================
/**
    The signature tactile control.

    A dark shaded body, a thin outer ring, a bright accent value arc, a small
    crisp indicator and a soft shadow. A juce::Slider sits on top and owns the
    interaction: vertical drag, fine adjustment while Shift / Ctrl / Cmd is
    held, double-click to return to the parameter default, and the mouse wheel.

    setModulationAmount() adds a second, thinner aqua ring outside the value
    arc showing how far the modulation matrix is currently pushing the value.
*/
class RippleKnob : public juce::Component,
                   private juce::Slider::Listener,
                   private juce::Timer
{
public:
    enum class Size { Large, Medium, Small };

    explicit RippleKnob (const juce::String& label, Size size = Size::Medium);
    ~RippleKnob() override;

    /** Connects the knob to an APVTS parameter. Also brings across the
        parameter's default (double-click reset) and value formatting. */
    void attach (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID);

    void setAccent (juce::Colour accent);
    void setLabel (const juce::String& label);
    void setTooltipText (const juce::String& tip);

    /** -1..1. Draws the outer modulation ring from the current value toward
        where the modulation is pushing it. Cheap enough to call per frame. */
    void setModulationAmount (float amount);

    juce::Slider& getSlider() noexcept;

    /** The size this knob would like to be. Views may use or ignore it. */
    static juce::Point<int> getPreferredSize (Size size);

    /** The knob body diameter for a given size. */
    static int getDiameter (Size size);

    //==========================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit  (const juce::MouseEvent&) override;
    void enablementChanged() override;

private:
    //==========================================================================
    /** The interaction layer. It never paints — RippleKnob draws the knob — but
        it owns dragging, the wheel, the keyboard and the accessibility role. */
    class KnobSlider : public juce::Slider
    {
    public:
        explicit KnobSlider (RippleKnob& o) : owner (o) {}

        void paint (juce::Graphics&) override {}
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
        void focusGained (juce::Component::FocusChangeType) override;
        void focusLost   (juce::Component::FocusChangeType) override;

    private:
        RippleKnob& owner;
    };

    void sliderValueChanged (juce::Slider*) override;
    void timerCallback() override;

    void setHovered (bool shouldBeHovered);
    void refreshHoverFromMouse();
    float getNormalisedValue();
    float getDetailLevel() const;
    juce::Font getLabelFontForSize() const;
    juce::Font getValueFontForSize() const;

    juce::String labelText;
    juce::String tooltipText;
    Size         knobSize { Size::Medium };
    juce::Colour accentColour { RippleTheme::get().cyan };

    KnobSlider slider { *this };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    float modulationAmount = 0.0f;
    float hoverTarget      = 0.0f;
    float hoverAmount      = 0.0f;

    // Cached on resize; paint() only fills in the value-dependent parts.
    juce::Rectangle<int> knobArea, labelArea, valueArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RippleKnob)
};

} // namespace ripples
