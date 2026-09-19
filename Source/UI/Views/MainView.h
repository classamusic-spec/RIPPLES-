#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Presets/PresetManager.h"
#include "UI/Views/EffectsPage.h"
#include "UI/Views/FluidField.h"
#include "UI/Views/Header.h"
#include "UI/Views/MacroStrip.h"
#include "UI/Views/ModulationPage.h"
#include "UI/Views/OscillatorPanel.h"
#include "UI/Views/PresetBrowser.h"
#include "UI/Views/SynthPage.h"
#include "UI/Views/TabBar.h"
#include "Utilities/VisualizationState.h"

#include <functional>

namespace ripples
{

//==============================================================================
/**
    The whole instrument.

        HEADER          identity, preset, preset actions, output
        UPPER           TIDE  |  FLUID FIELD  |  CURRENT
        MACROS          the eight controls that shape the sound
        TABS + PAGES    SYNTH / MODULATION / FX / PRESETS

    Everything is laid out from the theme's 4 px grid against the real bounds —
    there is no fixed design that is merely scaled. As the window shrinks, the
    decorative spacing goes first, then optional labels, then graph height. No
    parameter is ever dropped.
*/
class MainView final : public juce::Component
{
public:
    MainView (juce::AudioProcessorValueTreeState& apvts,
              VisualizationState& vis,
              PresetManager& presets);
    ~MainView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    //==========================================================================
    void showPage (int index);

    PresetManager& presetManager;
    std::function<void()> previousPresetCallback;

    Header          header;
    OscillatorPanel oscillatorA, oscillatorB;
    FluidField      fluidField;
    MacroStrip      macros;
    TabBar          tabs;

    SynthPage      synthPage;
    ModulationPage modulationPage;
    EffectsPage    effectsPage;
    PresetBrowser  presetBrowser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainView)
};

} // namespace ripples
