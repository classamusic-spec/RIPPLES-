#include "Core/PluginEditor.h"
#include "UI/Theme/RippleTheme.h"

namespace ripples
{
RipplesAudioProcessorEditor::RipplesAudioProcessorEditor (RipplesAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    setResizable (true, true);
    setResizeLimits (960, 600, 2560, 1520);
    setSize (1280, 760);
}

RipplesAudioProcessorEditor::~RipplesAudioProcessorEditor() = default;

void RipplesAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto& t = RippleTheme::get();
    g.fillAll (t.background);
    g.setColour (t.cyan);
    g.setFont (t.titleFont());
    g.drawText ("R I P P L E S", getLocalBounds(), juce::Justification::centred);
}

void RipplesAudioProcessorEditor::resized() {}
} // namespace ripples
