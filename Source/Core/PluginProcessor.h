#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Parameters/ParameterCache.h"
#include "Presets/PresetManager.h"
#include "Utilities/VisualizationState.h"

#include "DSP/RippleSynth.h"
#include "DSP/MacroEngine.h"
#include "DSP/Effects/StereoCurrent.h"
#include "DSP/Effects/LiquidChorus.h"
#include "DSP/Effects/LiquidDelay.h"
#include "DSP/Effects/DiffusionNetwork.h"
#include "DSP/Effects/AbyssReverb.h"
#include "DSP/Effects/MasterShaper.h"

namespace ripples
{

/**
    The plug-in. Owns the parameter tree, the polyphonic engine, the global
    effect chain and the preset manager.

    Threading: processBlock only ever reads plain values out of ParameterCache
    and writes plain values into VisualizationState. Presets are loaded on the
    message thread through the APVTS, never from here.
*/
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
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==========================================================================
    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    VisualizationState& getVisualisation() noexcept { return visualisation; }
    PresetManager& getPresetManager() noexcept { return presetManager; }

    /** Message-thread helper used by the editor and the offline render harness. */
    void loadFactoryPreset (int index) { presetManager.loadFactoryPreset (index); }

private:
    /** Folds the cache and the macro modulation into one VoiceParams block. */
    VoiceParams buildVoiceParams (const MacroModulation& macro, double bpm) const noexcept;

    /** Applies the macro modulation to the global effect chain. */
    void updateEffectParams (const MacroModulation& macro, double bpm) noexcept;

    juce::AudioProcessorValueTreeState apvts;
    ParameterCache cache;
    PresetManager presetManager;
    VisualizationState visualisation;

    RippleSynth synth;

    StereoCurrent    stereoCurrent;
    LiquidChorus     chorus;
    LiquidDelay      delay;
    DiffusionNetwork diffusion;
    AbyssReverb      reverb;
    MasterShaper     master;

    double currentSampleRate = 44100.0;
    double lastKnownBpm = 120.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RipplesAudioProcessor)
};

} // namespace ripples
