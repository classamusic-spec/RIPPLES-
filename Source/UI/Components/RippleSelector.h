#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "UI/Theme/RippleTheme.h"

namespace ripples
{

//==============================================================================
/**
    A dropdown in the same language as the rest of the instrument: a flat dark
    well, a thin border, a cyan chevron and clean type. The popup itself is
    drawn by RippleLookAndFeel, so opening it does not break the illusion.
*/
class RippleSelector : public juce::Component
{
public:
    RippleSelector();
    ~RippleSelector() override;

    /** Replaces the item list. Item IDs are 1..n, which is what the APVTS
        choice attachment expects. */
    void setItems (const juce::StringArray& items);

    void attach (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID);
    void setAccent (juce::Colour accent);

    juce::ComboBox& getComboBox() noexcept;

    void resized() override;

private:
    juce::ComboBox combo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    juce::Colour accentColour { RippleTheme::get().cyan };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RippleSelector)
};

} // namespace ripples
