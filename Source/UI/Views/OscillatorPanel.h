#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "UI/Components/GlassPanel.h"
#include "UI/Components/RippleButton.h"
#include "UI/Components/RippleKnob.h"
#include "UI/Components/RippleSelector.h"
#include "UI/Components/SectionHeader.h"
#include "UI/Components/WaveformView.h"

#include <memory>

namespace ripples
{

//==============================================================================
/**
    One oscillator, either TIDE (A) or CURRENT (B).

    The face of the panel carries only what is played with: the wave, its shape,
    tuning by octave, detune and level, over a live drawing of the actual
    waveform. Everything finer — semitone, fine tune, start phase, unison, stereo
    spread and pan — lives behind MORE, which slides an inner panel over the
    lower two thirds so the front of the instrument never gets crowded.

    CURRENT additionally exposes how it interacts with TIDE (hard sync, FM, phase
    modulation, ring modulation, crossfade) and by how much.
*/
class OscillatorPanel final : public GlassPanel,
                              private juce::Slider::Listener,
                              private juce::ComboBox::Listener
{
public:
    enum class Slot { A, B };

    OscillatorPanel (juce::AudioProcessorValueTreeState& apvts, Slot slot);
    ~OscillatorPanel() override;

    void resized() override;

private:
    //==========================================================================
    void sliderValueChanged (juce::Slider*) override;
    void comboBoxChanged (juce::ComboBox*) override;

    void setMoreVisible (bool shouldBeVisible);
    void refreshWaveform();

    bool isB() const noexcept { return slot == Slot::B; }
    juce::String paramID (const char* forA, const char* forB) const;

    //==========================================================================
    juce::AudioProcessorValueTreeState& state;
    const Slot slot;
    const juce::Colour accent;

    SectionHeader  sectionHeader;
    RippleButton   moreButton { "MORE" };
    RippleSelector waveSelector;
    WaveformView   waveform;

    // CURRENT only.
    std::unique_ptr<RippleSelector> interactionSelector;
    std::unique_ptr<RippleKnob>     interactionAmount;

    std::unique_ptr<RippleKnob> octave, shape, detune, level;

    // The MORE sub-panel and its contents.
    GlassPanel morePanel;
    std::unique_ptr<RippleKnob> semitone, fine, phase, unison, stereo, pan;

    bool moreVisible = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OscillatorPanel)
};

} // namespace ripples
