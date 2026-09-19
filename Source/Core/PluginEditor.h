#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Core/PluginProcessor.h"
#include "UI/Theme/RippleLookAndFeel.h"
#include "UI/Views/MainView.h"

namespace ripples
{

/**
    The editor is deliberately thin: it owns the LookAndFeel and the tooltip
    window, hosts MainView, and manages resizing. All layout lives in the views.
*/
class RipplesAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit RipplesAudioProcessorEditor (RipplesAudioProcessor&);
    ~RipplesAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    RipplesAudioProcessor& processorRef;

    RippleLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow { this, 700 };

    MainView mainView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RipplesAudioProcessorEditor)
};

} // namespace ripples
