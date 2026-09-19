#include "Core/PluginEditor.h"
#include "UI/Theme/RippleTheme.h"

namespace ripples
{

namespace
{
    // The design is drawn for this logical size; resizing scales the layout
    // rather than stretching a bitmap.
    constexpr int kDefaultWidth  = 1280;
    constexpr int kDefaultHeight = 760;
    constexpr int kMinWidth      = 960;
    constexpr int kMinHeight     = 600;
    constexpr int kMaxWidth      = 2560;
    constexpr int kMaxHeight     = 1520;
}

RipplesAudioProcessorEditor::RipplesAudioProcessorEditor (RipplesAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processorRef (p),
      mainView (p.getAPVTS(), p.getVisualisation(), p.getPresetManager())
{
    setLookAndFeel (&lookAndFeel);
    tooltipWindow.setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (mainView);

    setResizable (true, true);
    setResizeLimits (kMinWidth, kMinHeight, kMaxWidth, kMaxHeight);
    getConstrainer()->setFixedAspectRatio ((double) kDefaultWidth / (double) kDefaultHeight);
    setSize (kDefaultWidth, kDefaultHeight);
}

RipplesAudioProcessorEditor::~RipplesAudioProcessorEditor()
{
    tooltipWindow.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void RipplesAudioProcessorEditor::paint (juce::Graphics& g)
{
    // MainView paints the full background; this is only the fallback beneath it.
    g.fillAll (RippleTheme::get().background);
}

void RipplesAudioProcessorEditor::resized()
{
    mainView.setBounds (getLocalBounds());
}

} // namespace ripples
