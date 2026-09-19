#include "Core/PluginProcessor.h"
#include "Core/PluginEditor.h"

namespace ripples
{
static juce::AudioProcessorValueTreeState::ParameterLayout makeTemporaryLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "smokeTest", 1 }, "Smoke Test",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f));
    return layout;
}

RipplesAudioProcessor::RipplesAudioProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RIPPLES", makeTemporaryLayout())
{
}

RipplesAudioProcessor::~RipplesAudioProcessor() = default;

void RipplesAudioProcessor::prepareToPlay (double, int) {}
void RipplesAudioProcessor::releaseResources() {}

bool RipplesAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void RipplesAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
}

juce::AudioProcessorEditor* RipplesAudioProcessor::createEditor()
{
    return new RipplesAudioProcessorEditor (*this);
}

void RipplesAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
}

void RipplesAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}
} // namespace ripples

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ripples::RipplesAudioProcessor();
}
