#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "UI/Components/EnvelopeView.h"
#include "UI/Components/FilterResponseView.h"
#include "UI/Components/GlassPanel.h"
#include "UI/Components/LFOView.h"
#include "UI/Components/RippleButton.h"
#include "UI/Components/RippleKnob.h"
#include "UI/Components/RippleSelector.h"
#include "UI/Components/RippleToggle.h"
#include "UI/Components/SectionHeader.h"

#include <memory>

namespace ripples
{

//==============================================================================
/**
    The SYNTH page: the four panels a player edits most.

      DEPTH / FILTER      response curve, cutoff, resonance, drive
      ENVELOPE            amp and mod envelopes over one shared graph
      MODULATORS          tide, current, drift and ripple, one at a time
      SUB / NOISE         the two extra sources, plus the droplet engine and
                          the water resonator behind their own drawers

    Secondary controls live behind MORE sub-panels and two full-width drawers.
    Nothing is hidden that a player needs while playing; nothing that is only
    needed while designing takes space from what is.
*/
class SynthPage final : public juce::Component,
                        private juce::Slider::Listener,
                        private juce::ComboBox::Listener
{
public:
    explicit SynthPage (juce::AudioProcessorValueTreeState& apvts);
    ~SynthPage() override;

    void resized() override;

private:
    //==========================================================================
    enum class Modulator { Tide = 0, Current, Drift, Ripple };
    enum class EnvSelection { Amp = 0, Mod };

    void sliderValueChanged (juce::Slider*) override;
    void comboBoxChanged (juce::ComboBox*) override;

    void refreshFilterView();
    void refreshEnvelopeView();
    void refreshLfoView();

    void setFilterMoreVisible (bool);
    void setModMoreVisible (bool);
    void setEnvSelection (EnvSelection);
    void updateModulatorVisibility();
    void showDrawer (juce::Component* drawer);

    void layoutFilterPanel();
    void layoutEnvelopePanel();
    void layoutModulatorPanel();
    void layoutSourcePanel();
    void layoutDrawers (juce::Rectangle<int> pageArea);

    Modulator currentModulator() const noexcept;

    /** Watches a control so the graphs can follow it. Unwatched in the dtor. */
    void observe (RippleKnob& knob);
    void observe (RippleSelector& selector);

    //==========================================================================
    juce::AudioProcessorValueTreeState& state;

    juce::Array<juce::Slider*>   observedSliders;
    juce::Array<juce::ComboBox*> observedBoxes;

    // --- DEPTH / FILTER -----------------------------------------------------
    GlassPanel         filterPanel;
    SectionHeader      filterHeader { "DEPTH", "FILTER" };
    RippleSelector     filterMode;
    FilterResponseView filterResponse;
    RippleButton       filterMoreButton { "MORE" };
    GlassPanel         filterMorePanel;
    std::unique_ptr<RippleKnob> cutoff, resonance, drive;
    std::unique_ptr<RippleKnob> keyTrack, filterEnvAmount, movement, pressure;
    bool filterMoreVisible = false;

    // --- ENVELOPE -----------------------------------------------------------
    GlassPanel    envPanel;
    SectionHeader envHeader { "ENVELOPE", "SHAPE OVER TIME" };
    RippleButton  ampTabButton { "AMP" }, modTabButton { "MOD" };
    EnvelopeView  envView;
    std::unique_ptr<RippleKnob> ampAttack, ampDecay, ampSustain, ampRelease, ampVelocity;
    std::unique_ptr<RippleKnob> modAttack, modDecay, modSustain, modRelease, modVelocity;
    EnvSelection envSelection = EnvSelection::Amp;

    // --- MODULATORS ---------------------------------------------------------
    GlassPanel     modPanel;
    SectionHeader  modHeader { "MODULATORS", "MOTION" };
    RippleSelector modulatorSelector;
    LFOView        lfoView;
    RippleButton   modMoreButton { "MORE" };
    GlassPanel     modMorePanel;

    std::unique_ptr<RippleKnob> tideRate, tideDepth, tidePhase, tideStereo;
    RippleSelector tideShape, tideSyncRate;
    RippleToggle   tideSync { "SYNC" };

    std::unique_ptr<RippleKnob> currentRate, currentAmount, currentSmooth,
                                currentDrift, currentStereo;

    std::unique_ptr<RippleKnob> driftRate, driftAmount, driftStereo;

    std::unique_ptr<RippleKnob> rippleRate, rippleDepth, rippleDecay,
                                rippleCycles, rippleSpread;
    RippleSelector rippleTrigger;
    RippleToggle   ripplePolarity { "INVERT" };
    bool modMoreVisible = false;

    // --- SUB / NOISE --------------------------------------------------------
    GlassPanel     sourcePanel;
    SectionHeader  sourceHeader { "SUB / NOISE", "WATER SOURCES" };
    RippleSelector subWave, noiseType;
    RippleButton   dropsButton { "DROPS" }, resoButton { "RESO" };
    std::unique_ptr<RippleKnob> subOctave, subLevel, noiseLevel, noiseTone;

    // --- Drawers ------------------------------------------------------------
    GlassPanel     dropsDrawer;
    SectionHeader  dropsHeader { "DROPLETS", "SCATTERED WATER IMPACTS" };
    RippleSelector dropMode;
    RippleButton   dropsClose { "CLOSE" };
    std::unique_ptr<RippleKnob> dropAmount, dropDensity, dropSize, dropTone, dropSplash,
                                dropGravity, dropBounce, dropRandom, dropSpread;

    GlassPanel     resoDrawer;
    SectionHeader  resoHeader { "WATER RESONATOR", "BUBBLES, GLASS AND BELLS" };
    RippleButton   resoClose { "CLOSE" };
    std::unique_ptr<RippleKnob> resoAmount, resoSize, resoDecay, resoDamping,
                                resoScatter, resoMotion;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthPage)
};

} // namespace ripples
