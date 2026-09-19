#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Core/PluginProcessor.h"

namespace ripples
{
class RipplesAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit RipplesAudioProcessorEditor (RipplesAudioProcessor&);
    ~RipplesAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    RipplesAudioProcessor& processorRef;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RipplesAudioProcessorEditor)
};
} // namespace ripples
