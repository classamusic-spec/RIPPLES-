#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Utilities/VisualizationState.h"

namespace ripples
{
class RipplesAudioProcessor : public juce::AudioProcessor
{
public:
    RipplesAudioProcessor();
    ~RipplesAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "RIPPLES"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Submerged Dreams"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    VisualizationState& getVisualisation() noexcept { return visualisation; }

private:
    juce::AudioProcessorValueTreeState apvts;
    VisualizationState visualisation;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RipplesAudioProcessor)
};
} // namespace ripples
