#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Presets/PresetManager.h"
#include "UI/Components/RippleButton.h"
#include "UI/Components/RippleKnob.h"
#include "Utilities/VisualizationState.h"

#include <functional>
#include <memory>

namespace ripples
{

//==============================================================================
/**
    The top band: identity on the left, the preset on the centre line, the
    preset actions next to it, and the output on the right.

    The only moving part is the output meter, which is driven by a timer that
    stops the moment the header is off screen, so a hidden or minimised editor
    costs nothing.
*/
class Header final : public juce::Component,
                     private juce::Timer
{
public:
    Header (juce::AudioProcessorValueTreeState& apvts,
            VisualizationState& vis,
            PresetManager& presets);
    ~Header() override;

    /** Raised when the player asks for the browser (BROWSE, or the preset name). */
    std::function<void()> onBrowseRequested;

    /** Re-reads the current preset from the manager and repaints. */
    void presetChanged();

    //==========================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void visibilityChanged() override;
    void parentHierarchyChanged() override;

private:
    //==========================================================================
    class LevelMeter;      // vertical stereo output meter
    class SettingsPanel;   // voice + master controls, shown in a call-out

    void timerCallback() override;
    void updateTimerState();
    void showSettings();
    void updateButtonTexts (bool compact);

    //==========================================================================
    juce::AudioProcessorValueTreeState& state;
    VisualizationState& visuals;
    PresetManager& presetManager;

    RippleButton prevButton, nextButton;
    RippleButton favouriteButton, browseButton, randomButton, initButton, settingsButton;

    std::unique_ptr<LevelMeter> meter;
    std::unique_ptr<RippleKnob> outputGain;

    // Painted areas, filled in by resized().
    juce::Rectangle<int> logoArea, presetArea;

    juce::String presetName, presetDetail;
    bool presetIsModified = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Header)
};

} // namespace ripples
