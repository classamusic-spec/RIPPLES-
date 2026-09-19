#include "UI/Views/SynthPage.h"

#include "Parameters/ParameterEnums.h"
#include "Parameters/ParameterIDs.h"
#include "UI/Theme/RippleTheme.h"

#include <vector>

namespace ripples
{

namespace
{
    std::unique_ptr<RippleKnob> makeKnob (juce::Component& parent,
                                          juce::AudioProcessorValueTreeState& apvts,
                                          const juce::String& label,
                                          const juce::String& paramID,
                                          RippleKnob::Size size,
                                          juce::Colour accent,
                                          const juce::String& tooltip = {})
    {
        auto knob = std::make_unique<RippleKnob> (label, size);
        knob->setAccent (accent);

        if (tooltip.isNotEmpty())
            knob->setTooltipText (tooltip);

        knob->attach (apvts, paramID);
        parent.addAndMakeVisible (*knob);
        return knob;
    }

    void layoutRow (juce::Rectangle<int> area,
                    const std::vector<juce::Component*>& items,
                    int gap)
    {
        const int n = (int) items.size();

        if (n <= 0 || area.isEmpty())
            return;

        const float cellW = (float) (area.getWidth() - gap * (n - 1)) / (float) n;
        float x = (float) area.getX();

        for (auto* c : items)
        {
            const juce::Rectangle<float> cell { x, (float) area.getY(),
                                                cellW, (float) area.getHeight() };
            c->setBounds (cell.toNearestInt());
            x += cellW + (float) gap;
        }
    }

    juce::Rectangle<int> contentOf (GlassPanel& panel, int fallbackInset)
    {
        const auto r = panel.getContentBounds();
        return r.isEmpty() ? panel.getLocalBounds().reduced (fallbackInset) : r;
    }

    /** The natural height of a row of knobs of the given size. */
    int knobRowHeight (RippleKnob::Size size)
    {
        const auto& t = RippleTheme::get();

        const int diameter = size == RippleKnob::Size::Large  ? t.knobLargeDiameter
                           : size == RippleKnob::Size::Medium ? t.knobMediumDiameter
                                                              : t.knobSmallDiameter;

        return diameter + t.knobLabelHeight + t.knobValueHeight;
    }

    constexpr int kPanelInset = RippleTheme::md;
}

//==============================================================================
SynthPage::SynthPage (juce::AudioProcessorValueTreeState& apvts)
    : state (apvts)
{
    const auto& t = RippleTheme::get();

    const auto filterAccent = t.cyan;
    const auto envAccent    = t.cyan;
    const auto modAccent    = t.aqua;
    const auto sourceAccent = t.violet;

    for (auto* panel : { &filterPanel, &envPanel, &modPanel, &sourcePanel })
    {
        panel->setContentInset (kPanelInset);
        addAndMakeVisible (*panel);
    }

    filterPanel.setAccent (filterAccent);
    envPanel   .setAccent (envAccent);
    modPanel   .setAccent (modAccent);
    sourcePanel.setAccent (sourceAccent);

    //--------------------------------------------------------------------------
    // DEPTH / FILTER
    //--------------------------------------------------------------------------
    filterHeader.setAccent (filterAccent);
    filterPanel.addAndMakeVisible (filterHeader);

    filterMode.setAccent (filterAccent);
    filterMode.setItems (toStringArray (filterModeNames));
    filterMode.attach (apvts, pid::filtMode);
    filterPanel.addAndMakeVisible (filterMode);
    observe (filterMode);

    filterResponse.setAccent (filterAccent);
    filterPanel.addAndMakeVisible (filterResponse);

    filterMoreButton.setAccent (filterAccent);
    filterMoreButton.onClick = [this] { setFilterMoreVisible (! filterMoreVisible); };
    filterPanel.addAndMakeVisible (filterMoreButton);

    cutoff    = makeKnob (filterPanel, apvts, "CUTOFF", pid::filtCutoff,
                          RippleKnob::Size::Small, filterAccent,
                          "How deep the sound sits: the filter's corner frequency.");
    resonance = makeKnob (filterPanel, apvts, "RESO",   pid::filtReso,
                          RippleKnob::Size::Small, filterAccent,
                          "Emphasis at the cutoff, up to self-oscillation.");
    drive     = makeKnob (filterPanel, apvts, "DRIVE",  pid::filtDrive,
                          RippleKnob::Size::Small, filterAccent,
                          "Saturation into the filter.");

    observe (*cutoff);
    observe (*resonance);

    filterMorePanel.setAccent (filterAccent);
    filterMorePanel.setContentInset (RippleTheme::md);
    filterPanel.addChildComponent (filterMorePanel);

    keyTrack        = makeKnob (filterMorePanel, apvts, "KEY TRK", pid::filtKeyTrack,
                                RippleKnob::Size::Small, filterAccent,
                                "How far the cutoff follows the played note.");
    filterEnvAmount = makeKnob (filterMorePanel, apvts, "ENV",     pid::filtEnvAmt,
                                RippleKnob::Size::Small, filterAccent,
                                "How far the mod envelope moves the cutoff.");
    movement        = makeKnob (filterMorePanel, apvts, "MOVE",    pid::filtMovement,
                                RippleKnob::Size::Small, filterAccent,
                                "Morph position between low pass, band pass and high pass.");
    pressure        = makeKnob (filterMorePanel, apvts, "PRESSURE", pid::filtPressure,
                                RippleKnob::Size::Small, filterAccent,
                                "Adds density, drive and low-mid weight.");

    //--------------------------------------------------------------------------
    // ENVELOPE
    //--------------------------------------------------------------------------
    envHeader.setAccent (envAccent);
    envPanel.addAndMakeVisible (envHeader);

    for (auto* b : { &ampTabButton, &modTabButton })
    {
        b->setAccent (envAccent);
        envPanel.addAndMakeVisible (*b);
    }

    ampTabButton.onClick = [this] { setEnvSelection (EnvSelection::Amp); };
    modTabButton.onClick = [this] { setEnvSelection (EnvSelection::Mod); };

    envView.setAccent (envAccent);
    envPanel.addAndMakeVisible (envView);

    ampAttack   = makeKnob (envPanel, apvts, "A",   pid::aenvAttack,   RippleKnob::Size::Small, envAccent);
    ampDecay    = makeKnob (envPanel, apvts, "D",   pid::aenvDecay,    RippleKnob::Size::Small, envAccent);
    ampSustain  = makeKnob (envPanel, apvts, "S",   pid::aenvSustain,  RippleKnob::Size::Small, envAccent);
    ampRelease  = makeKnob (envPanel, apvts, "R",   pid::aenvRelease,  RippleKnob::Size::Small, envAccent);
    ampVelocity = makeKnob (envPanel, apvts, "VEL", pid::aenvVelocity, RippleKnob::Size::Small, envAccent,
                            "How strongly playing harder opens the amplitude.");

    modAttack   = makeKnob (envPanel, apvts, "A",   pid::fenvAttack,   RippleKnob::Size::Small, envAccent);
    modDecay    = makeKnob (envPanel, apvts, "D",   pid::fenvDecay,    RippleKnob::Size::Small, envAccent);
    modSustain  = makeKnob (envPanel, apvts, "S",   pid::fenvSustain,  RippleKnob::Size::Small, envAccent);
    modRelease  = makeKnob (envPanel, apvts, "R",   pid::fenvRelease,  RippleKnob::Size::Small, envAccent);
    modVelocity = makeKnob (envPanel, apvts, "VEL", pid::fenvVelocity, RippleKnob::Size::Small, envAccent,
                            "How strongly playing harder opens the mod envelope.");

    for (auto* k : { ampAttack.get(), ampDecay.get(), ampSustain.get(), ampRelease.get(),
                     modAttack.get(), modDecay.get(), modSustain.get(), modRelease.get() })
        observe (*k);

    //--------------------------------------------------------------------------
    // MODULATORS
    //--------------------------------------------------------------------------
    modHeader.setAccent (modAccent);
    modPanel.addAndMakeVisible (modHeader);

    modulatorSelector.setAccent (modAccent);
    modulatorSelector.setItems ({ "TIDE", "CURRENT", "DRIFT", "RIPPLE" });
    modulatorSelector.getComboBox().setSelectedItemIndex (0, juce::dontSendNotification);
    modPanel.addAndMakeVisible (modulatorSelector);
    observe (modulatorSelector);

    lfoView.setAccent (modAccent);
    modPanel.addAndMakeVisible (lfoView);

    modMoreButton.setAccent (modAccent);
    modMoreButton.onClick = [this] { setModMoreVisible (! modMoreVisible); };
    modPanel.addAndMakeVisible (modMoreButton);

    modMorePanel.setAccent (modAccent);
    modMorePanel.setContentInset (RippleTheme::md);
    modPanel.addChildComponent (modMorePanel);

    // TIDE
    tideRate   = makeKnob (modPanel, apvts, "RATE",  pid::tideRate,  RippleKnob::Size::Small, modAccent,
                           "Speed of the tide.");
    tideDepth  = makeKnob (modPanel, apvts, "DEPTH", pid::tideDepth, RippleKnob::Size::Small, modAccent);
    tidePhase  = makeKnob (modPanel, apvts, "PHASE", pid::tidePhase, RippleKnob::Size::Small, modAccent);
    tideStereo = makeKnob (modMorePanel, apvts, "STEREO", pid::tideStereo,
                           RippleKnob::Size::Small, modAccent,
                           "Phase offset between the left and right values.");

    tideShape.setAccent (modAccent);
    tideShape.setItems (toStringArray (tideShapeNames));
    tideShape.attach (apvts, pid::tideShape);
    modMorePanel.addAndMakeVisible (tideShape);
    observe (tideShape);

    tideSyncRate.setAccent (modAccent);
    tideSyncRate.setItems (toStringArray (syncDivisionNames));
    tideSyncRate.attach (apvts, pid::tideSyncRate);
    modMorePanel.addAndMakeVisible (tideSyncRate);

    tideSync.setAccent (modAccent);
    tideSync.attach (apvts, pid::tideSync);
    modMorePanel.addAndMakeVisible (tideSync);

    observe (*tideDepth);
    observe (*tidePhase);

    // CURRENT
    currentRate   = makeKnob (modPanel, apvts, "RATE",   pid::currRate,   RippleKnob::Size::Small, modAccent,
                              "How fast the current wanders.");
    currentAmount = makeKnob (modPanel, apvts, "AMOUNT", pid::currAmount, RippleKnob::Size::Small, modAccent,
                              "Adds smooth, organic movement.");
    currentSmooth = makeKnob (modPanel, apvts, "SMOOTH", pid::currSmooth, RippleKnob::Size::Small, modAccent);
    currentDrift  = makeKnob (modMorePanel, apvts, "DRIFT",  pid::currDrift,  RippleKnob::Size::Small, modAccent);
    currentStereo = makeKnob (modMorePanel, apvts, "STEREO", pid::currStereo, RippleKnob::Size::Small, modAccent);
    observe (*currentAmount);

    // DRIFT
    driftRate   = makeKnob (modPanel, apvts, "RATE",   pid::driftRate,   RippleKnob::Size::Small, modAccent,
                            "Very slow random motion for pads and drones.");
    driftAmount = makeKnob (modPanel, apvts, "AMOUNT", pid::driftAmount, RippleKnob::Size::Small, modAccent);
    driftStereo = makeKnob (modPanel, apvts, "STEREO", pid::driftStereo, RippleKnob::Size::Small, modAccent);
    observe (*driftAmount);

    // RIPPLE
    rippleRate   = makeKnob (modPanel, apvts, "RATE",   pid::rippleRate,   RippleKnob::Size::Small, modAccent,
                             "Frequency of the damped wave.");
    rippleDepth  = makeKnob (modPanel, apvts, "DEPTH",  pid::rippleDepth,  RippleKnob::Size::Small, modAccent,
                             "Adds decaying wave-like modulation.");
    rippleDecay  = makeKnob (modPanel, apvts, "DECAY",  pid::rippleDecay,  RippleKnob::Size::Small, modAccent);
    rippleCycles = makeKnob (modMorePanel, apvts, "CYCLES", pid::rippleCycles, RippleKnob::Size::Small, modAccent);
    rippleSpread = makeKnob (modMorePanel, apvts, "SPREAD", pid::rippleSpread, RippleKnob::Size::Small, modAccent);
    observe (*rippleDepth);

    rippleTrigger.setAccent (modAccent);
    rippleTrigger.setItems (toStringArray (rippleTriggerNames));
    rippleTrigger.attach (apvts, pid::rippleTrigger);
    modMorePanel.addAndMakeVisible (rippleTrigger);

    ripplePolarity.setAccent (modAccent);
    ripplePolarity.attach (apvts, pid::ripplePolarity);
    modMorePanel.addAndMakeVisible (ripplePolarity);

    //--------------------------------------------------------------------------
    // SUB / NOISE
    //--------------------------------------------------------------------------
    sourceHeader.setAccent (sourceAccent);
    sourcePanel.addAndMakeVisible (sourceHeader);

    subWave.setAccent (sourceAccent);
    subWave.setItems (toStringArray (subWaveNames));
    subWave.attach (apvts, pid::subWave);
    sourcePanel.addAndMakeVisible (subWave);

    noiseType.setAccent (sourceAccent);
    noiseType.setItems (toStringArray (noiseTypeNames));
    noiseType.attach (apvts, pid::noiseType);
    sourcePanel.addAndMakeVisible (noiseType);

    subOctave  = makeKnob (sourcePanel, apvts, "SUB OCT", pid::subOctave,
                           RippleKnob::Size::Small, sourceAccent,
                           "How far below the note the sub sits.");
    subLevel   = makeKnob (sourcePanel, apvts, "SUB",     pid::subLevel,
                           RippleKnob::Size::Small, sourceAccent);
    noiseLevel = makeKnob (sourcePanel, apvts, "NOISE",   pid::noiseLevel,
                           RippleKnob::Size::Small, sourceAccent);
    noiseTone  = makeKnob (sourcePanel, apvts, "TONE",    pid::noiseTone,
                           RippleKnob::Size::Small, sourceAccent,
                           "Colour of the water noise, dark to bright.");

    for (auto* b : { &dropsButton, &resoButton })
    {
        b->setAccent (sourceAccent);
        sourcePanel.addAndMakeVisible (*b);
    }

    dropsButton.onClick = [this] { showDrawer (&dropsDrawer); };
    resoButton .onClick = [this] { showDrawer (&resoDrawer); };

    //--------------------------------------------------------------------------
    // DROPLET DRAWER
    //--------------------------------------------------------------------------
    dropsDrawer.setAccent (sourceAccent);
    dropsDrawer.setContentInset (RippleTheme::lg);
    addChildComponent (dropsDrawer);

    dropsHeader.setAccent (sourceAccent);
    dropsDrawer.addAndMakeVisible (dropsHeader);

    dropMode.setAccent (sourceAccent);
    dropMode.setItems (toStringArray (dropletModeNames));
    dropMode.attach (apvts, pid::dropMode);
    dropsDrawer.addAndMakeVisible (dropMode);

    dropsClose.setAccent (sourceAccent);
    dropsClose.onClick = [this] { showDrawer (nullptr); };
    dropsDrawer.addAndMakeVisible (dropsClose);

    dropAmount  = makeKnob (dropsDrawer, apvts, "AMOUNT",  pid::dropAmount,  RippleKnob::Size::Small, sourceAccent,
                            "How much droplet sound is mixed in.");
    dropDensity = makeKnob (dropsDrawer, apvts, "DENSITY", pid::dropDensity, RippleKnob::Size::Small, sourceAccent);
    dropSize    = makeKnob (dropsDrawer, apvts, "SIZE",    pid::dropSize,    RippleKnob::Size::Small, sourceAccent);
    dropTone    = makeKnob (dropsDrawer, apvts, "TONE",    pid::dropTone,    RippleKnob::Size::Small, sourceAccent);
    dropSplash  = makeKnob (dropsDrawer, apvts, "SPLASH",  pid::dropSplash,  RippleKnob::Size::Small, sourceAccent);
    dropGravity = makeKnob (dropsDrawer, apvts, "GRAVITY", pid::dropGravity, RippleKnob::Size::Small, sourceAccent,
                            "How quickly repeated drips accelerate.");
    dropBounce  = makeKnob (dropsDrawer, apvts, "BOUNCE",  pid::dropBounce,  RippleKnob::Size::Small, sourceAccent);
    dropRandom  = makeKnob (dropsDrawer, apvts, "RANDOM",  pid::dropRandom,  RippleKnob::Size::Small, sourceAccent);
    dropSpread  = makeKnob (dropsDrawer, apvts, "SPREAD",  pid::dropSpread,  RippleKnob::Size::Small, sourceAccent);

    //--------------------------------------------------------------------------
    // RESONATOR DRAWER
    //--------------------------------------------------------------------------
    resoDrawer.setAccent (sourceAccent);
    resoDrawer.setContentInset (RippleTheme::lg);
    addChildComponent (resoDrawer);

    resoHeader.setAccent (sourceAccent);
    resoDrawer.addAndMakeVisible (resoHeader);

    resoClose.setAccent (sourceAccent);
    resoClose.onClick = [this] { showDrawer (nullptr); };
    resoDrawer.addAndMakeVisible (resoClose);

    resoAmount  = makeKnob (resoDrawer, apvts, "AMOUNT",  pid::resoAmount,  RippleKnob::Size::Small, sourceAccent,
                            "How much of the resonator bank is heard.");
    resoSize    = makeKnob (resoDrawer, apvts, "SIZE",    pid::resoSize,    RippleKnob::Size::Small, sourceAccent);
    resoDecay   = makeKnob (resoDrawer, apvts, "DECAY",   pid::resoDecay,   RippleKnob::Size::Small, sourceAccent);
    resoDamping = makeKnob (resoDrawer, apvts, "DAMPING", pid::resoDamping, RippleKnob::Size::Small, sourceAccent);
    resoScatter = makeKnob (resoDrawer, apvts, "SCATTER", pid::resoScatter, RippleKnob::Size::Small, sourceAccent);
    resoMotion  = makeKnob (resoDrawer, apvts, "MOTION",  pid::resoMotion,  RippleKnob::Size::Small, sourceAccent);

    setEnvSelection (EnvSelection::Amp);
    updateModulatorVisibility();
    refreshFilterView();
}

SynthPage::~SynthPage()
{
    for (auto* s : observedSliders)
        s->removeListener (this);

    for (auto* c : observedBoxes)
        c->removeListener (this);
}

//==============================================================================
void SynthPage::observe (RippleKnob& knob)
{
    knob.getSlider().addListener (this);
    observedSliders.add (&knob.getSlider());
}

void SynthPage::observe (RippleSelector& selector)
{
    selector.getComboBox().addListener (this);
    observedBoxes.add (&selector.getComboBox());
}

SynthPage::Modulator SynthPage::currentModulator() const noexcept
{
    const auto index = juce::jlimit (0, 3, modulatorSelector.getComboBox().getSelectedItemIndex());
    return (Modulator) index;
}

//==============================================================================
void SynthPage::sliderValueChanged (juce::Slider* slider)
{
    if (slider == &cutoff->getSlider() || slider == &resonance->getSlider())
    {
        refreshFilterView();
        return;
    }

    for (auto* k : { ampAttack.get(), ampDecay.get(), ampSustain.get(), ampRelease.get(),
                     modAttack.get(), modDecay.get(), modSustain.get(), modRelease.get() })
    {
        if (slider == &k->getSlider())
        {
            refreshEnvelopeView();
            return;
        }
    }

    refreshLfoView();
}

void SynthPage::comboBoxChanged (juce::ComboBox* box)
{
    if (box == &filterMode.getComboBox())
    {
        refreshFilterView();
        return;
    }

    if (box == &modulatorSelector.getComboBox())
    {
        updateModulatorVisibility();
        return;
    }

    refreshLfoView();
}

void SynthPage::refreshFilterView()
{
    const auto mode = (FilterMode) juce::jlimit (0, (int) FilterMode::NumModes - 1,
                                                 filterMode.getComboBox().getSelectedItemIndex());

    filterResponse.setFilter (mode,
                              (float) cutoff->getSlider().getValue(),
                              (float) resonance->getSlider().getValue());
}

void SynthPage::refreshEnvelopeView()
{
    const bool amp = envSelection == EnvSelection::Amp;

    auto* a = amp ? ampAttack.get()  : modAttack.get();
    auto* d = amp ? ampDecay.get()   : modDecay.get();
    auto* s = amp ? ampSustain.get() : modSustain.get();
    auto* r = amp ? ampRelease.get() : modRelease.get();

    envView.setADSR ((float) a->getSlider().getValue(),
                     (float) d->getSlider().getValue(),
                     (float) s->getSlider().getValue(),
                     (float) r->getSlider().getValue());
}

void SynthPage::refreshLfoView()
{
    auto shape = TideShape::Sine;
    float depth = 0.0f;
    float phase = 0.0f;

    switch (currentModulator())
    {
        case Modulator::Tide:
            shape = (TideShape) juce::jlimit (0, (int) TideShape::NumShapes - 1,
                                              tideShape.getComboBox().getSelectedItemIndex());
            depth = (float) tideDepth->getSlider().getValue();
            phase = (float) tidePhase->getSlider().getValue();
            break;

        case Modulator::Current:
            shape = TideShape::Flow;
            depth = (float) currentAmount->getSlider().getValue();
            break;

        case Modulator::Drift:
            shape = TideShape::Sine;
            depth = (float) driftAmount->getSlider().getValue();
            break;

        case Modulator::Ripple:
            shape = TideShape::Swell;
            depth = (float) rippleDepth->getSlider().getValue();
            break;
    }

    lfoView.setShape (shape, depth);
    lfoView.setPhase (phase);
}

//==============================================================================
void SynthPage::setFilterMoreVisible (bool shouldBeVisible)
{
    filterMoreVisible = shouldBeVisible;
    filterMoreButton.setButtonText (shouldBeVisible ? "LESS" : "MORE");

    filterResponse.setVisible (! shouldBeVisible);
    filterMode.setVisible (! shouldBeVisible);
    cutoff->setVisible (! shouldBeVisible);
    resonance->setVisible (! shouldBeVisible);
    drive->setVisible (! shouldBeVisible);

    filterMorePanel.setVisible (shouldBeVisible);

    if (shouldBeVisible)
        filterMorePanel.toFront (false);

    layoutFilterPanel();
}

void SynthPage::setModMoreVisible (bool shouldBeVisible)
{
    modMoreVisible = shouldBeVisible;
    modMoreButton.setButtonText (shouldBeVisible ? "LESS" : "MORE");
    modMorePanel.setVisible (shouldBeVisible);

    if (shouldBeVisible)
        modMorePanel.toFront (false);

    updateModulatorVisibility();
}

void SynthPage::setEnvSelection (EnvSelection selection)
{
    envSelection = selection;

    const bool amp = selection == EnvSelection::Amp;

    ampTabButton.setIsPrimary (amp);
    modTabButton.setIsPrimary (! amp);

    for (auto* k : { ampAttack.get(), ampDecay.get(), ampSustain.get(),
                     ampRelease.get(), ampVelocity.get() })
        k->setVisible (amp);

    for (auto* k : { modAttack.get(), modDecay.get(), modSustain.get(),
                     modRelease.get(), modVelocity.get() })
        k->setVisible (! amp);

    refreshEnvelopeView();
    layoutEnvelopePanel();
}

void SynthPage::updateModulatorVisibility()
{
    const auto mod = currentModulator();
    const bool showing = ! modMoreVisible;

    const bool tide    = mod == Modulator::Tide;
    const bool current = mod == Modulator::Current;
    const bool drift   = mod == Modulator::Drift;
    const bool ripple  = mod == Modulator::Ripple;

    lfoView.setVisible (showing);

    tideRate->setVisible (showing && tide);
    tideDepth->setVisible (showing && tide);
    tidePhase->setVisible (showing && tide);

    currentRate->setVisible (showing && current);
    currentAmount->setVisible (showing && current);
    currentSmooth->setVisible (showing && current);

    driftRate->setVisible (showing && drift);
    driftAmount->setVisible (showing && drift);
    driftStereo->setVisible (showing && drift);

    rippleRate->setVisible (showing && ripple);
    rippleDepth->setVisible (showing && ripple);
    rippleDecay->setVisible (showing && ripple);

    // Sub-panel contents follow the selected modulator too.
    tideShape.setVisible (tide);
    tideSyncRate.setVisible (tide);
    tideSync.setVisible (tide);
    tideStereo->setVisible (tide);

    currentDrift->setVisible (current);
    currentStereo->setVisible (current);

    rippleTrigger.setVisible (ripple);
    ripplePolarity.setVisible (ripple);
    rippleCycles->setVisible (ripple);
    rippleSpread->setVisible (ripple);

    // DRIFT has nothing behind MORE.
    modMoreButton.setEnabled (! drift);

    if (drift && modMoreVisible)
    {
        modMoreVisible = false;
        modMoreButton.setButtonText ("MORE");
        modMorePanel.setVisible (false);
    }

    refreshLfoView();
    layoutModulatorPanel();
}

void SynthPage::showDrawer (juce::Component* drawer)
{
    dropsDrawer.setVisible (drawer == &dropsDrawer);
    resoDrawer .setVisible (drawer == &resoDrawer);

    if (drawer != nullptr)
        drawer->toFront (false);
}

//==============================================================================
void SynthPage::resized()
{
    auto r = getLocalBounds();

    const int gap = r.getWidth() >= RippleTheme::grid (250) ? RippleTheme::md : RippleTheme::sm;

    layoutRow (r, { &filterPanel, &envPanel, &modPanel, &sourcePanel }, gap);

    layoutFilterPanel();
    layoutEnvelopePanel();
    layoutModulatorPanel();
    layoutSourcePanel();
    layoutDrawers (getLocalBounds());
}

//==============================================================================
void SynthPage::layoutFilterPanel()
{
    const auto& t = RippleTheme::get();

    auto content = contentOf (filterPanel, kPanelInset);

    if (content.isEmpty())
        return;

    const int titleH = juce::jmax (t.sectionHeaderHeight, t.buttonHeight);
    auto titleRow = content.removeFromTop (juce::jmin (titleH, content.getHeight()));

    auto moreArea = titleRow.removeFromRight (juce::jmin (RippleTheme::grid (14),
                                                          titleRow.getWidth() / 2));
    filterMoreButton.setBounds (moreArea.withSizeKeepingCentre (moreArea.getWidth(), t.buttonHeight));
    titleRow.removeFromRight (RippleTheme::sm);
    filterHeader.setBounds (titleRow);

    content.removeFromTop (RippleTheme::sm);

    if (filterMoreVisible)
    {
        filterMorePanel.setBounds (content);

        auto inner = contentOf (filterMorePanel, RippleTheme::md);
        const int rowH = juce::jmin (knobRowHeight (RippleKnob::Size::Small), inner.getHeight());

        layoutRow (inner.withSizeKeepingCentre (inner.getWidth(), rowH),
                   { keyTrack.get(), filterEnvAmount.get(), movement.get(), pressure.get() },
                   RippleTheme::sm);
        return;
    }

    const int knobRowH = juce::jmin (knobRowHeight (RippleKnob::Size::Small), content.getHeight());

    // Wide panels put the response curve beside the controls; narrow ones stack.
    if (content.getWidth() >= RippleTheme::grid (58))
    {
        auto controls = content.removeFromRight (juce::jmax (RippleTheme::grid (30),
                                                             content.getWidth() / 2));
        content.removeFromRight (RippleTheme::sm);
        filterResponse.setBounds (content);

        const int stackH = t.selectorHeight + RippleTheme::sm + knobRowH;
        auto column = controls.withSizeKeepingCentre (controls.getWidth(),
                                                      juce::jmin (stackH, controls.getHeight()));

        filterMode.setBounds (column.removeFromTop (juce::jmin (t.selectorHeight,
                                                                column.getHeight())));
        column.removeFromTop (RippleTheme::sm);
        layoutRow (column, { cutoff.get(), resonance.get(), drive.get() }, RippleTheme::xs);
    }
    else
    {
        filterMode.setBounds (content.removeFromTop (juce::jmin (t.selectorHeight,
                                                                 content.getHeight())));
        content.removeFromTop (RippleTheme::sm);

        auto knobRow = content.removeFromBottom (juce::jmin (knobRowH, content.getHeight()));
        content.removeFromBottom (RippleTheme::sm);
        filterResponse.setBounds (content);
        layoutRow (knobRow, { cutoff.get(), resonance.get(), drive.get() }, RippleTheme::xs);
    }
}

void SynthPage::layoutEnvelopePanel()
{
    const auto& t = RippleTheme::get();

    auto content = contentOf (envPanel, kPanelInset);

    if (content.isEmpty())
        return;

    const int titleH = juce::jmax (t.sectionHeaderHeight, t.buttonHeight);
    auto titleRow = content.removeFromTop (juce::jmin (titleH, content.getHeight()));

    const int tabW = juce::jmin (RippleTheme::grid (11), titleRow.getWidth() / 4);
    auto tabs = titleRow.removeFromRight (tabW * 2 + RippleTheme::xs);
    tabs = tabs.withSizeKeepingCentre (tabs.getWidth(), t.buttonHeight);
    layoutRow (tabs, { &ampTabButton, &modTabButton }, RippleTheme::xs);

    titleRow.removeFromRight (RippleTheme::sm);
    envHeader.setBounds (titleRow);

    content.removeFromTop (RippleTheme::sm);

    const int knobRowH = juce::jmin (knobRowHeight (RippleKnob::Size::Small), content.getHeight());
    auto knobRow = content.removeFromBottom (knobRowH);
    content.removeFromBottom (RippleTheme::sm);
    envView.setBounds (content);

    const bool amp = envSelection == EnvSelection::Amp;

    std::vector<juce::Component*> knobs;

    if (amp)
        knobs = { ampAttack.get(), ampDecay.get(), ampSustain.get(),
                  ampRelease.get(), ampVelocity.get() };
    else
        knobs = { modAttack.get(), modDecay.get(), modSustain.get(),
                  modRelease.get(), modVelocity.get() };

    layoutRow (knobRow, knobs, RippleTheme::xs);
}

void SynthPage::layoutModulatorPanel()
{
    const auto& t = RippleTheme::get();

    auto content = contentOf (modPanel, kPanelInset);

    if (content.isEmpty())
        return;

    const int titleH = juce::jmax (t.sectionHeaderHeight, t.buttonHeight);
    auto titleRow = content.removeFromTop (juce::jmin (titleH, content.getHeight()));

    auto moreArea = titleRow.removeFromRight (juce::jmin (RippleTheme::grid (13),
                                                          titleRow.getWidth() / 3));
    modMoreButton.setBounds (moreArea.withSizeKeepingCentre (moreArea.getWidth(), t.buttonHeight));
    titleRow.removeFromRight (RippleTheme::xs);

    auto selectorArea = titleRow.removeFromRight (juce::jmin (RippleTheme::grid (24),
                                                              titleRow.getWidth() * 2 / 3));
    modulatorSelector.setBounds (selectorArea.withSizeKeepingCentre (selectorArea.getWidth(),
                                                                     t.selectorHeight));
    titleRow.removeFromRight (RippleTheme::sm);
    modHeader.setBounds (titleRow);

    content.removeFromTop (RippleTheme::sm);

    if (modMoreVisible)
    {
        modMorePanel.setBounds (content);

        auto inner = contentOf (modMorePanel, RippleTheme::md);
        const int knobRowH = juce::jmin (knobRowHeight (RippleKnob::Size::Small),
                                         inner.getHeight());

        switch (currentModulator())
        {
            case Modulator::Tide:
            {
                auto top = inner.removeFromTop (juce::jmin (t.selectorHeight, inner.getHeight()));
                layoutRow (top, { &tideShape, &tideSyncRate, &tideSync }, RippleTheme::xs);
                tideSync.setBounds (tideSync.getBounds()
                                        .withSizeKeepingCentre (tideSync.getWidth(), t.toggleHeight));
                inner.removeFromTop (RippleTheme::sm);
                layoutRow (inner.removeFromTop (juce::jmin (knobRowH, inner.getHeight())),
                           { tideStereo.get() }, RippleTheme::xs);
                break;
            }

            case Modulator::Current:
                layoutRow (inner.withSizeKeepingCentre (inner.getWidth(),
                                                        juce::jmin (knobRowH, inner.getHeight())),
                           { currentDrift.get(), currentStereo.get() }, RippleTheme::sm);
                break;

            case Modulator::Ripple:
            {
                auto top = inner.removeFromTop (juce::jmin (t.selectorHeight, inner.getHeight()));
                layoutRow (top, { &rippleTrigger, &ripplePolarity }, RippleTheme::xs);
                ripplePolarity.setBounds (ripplePolarity.getBounds()
                                              .withSizeKeepingCentre (ripplePolarity.getWidth(),
                                                                      t.toggleHeight));
                inner.removeFromTop (RippleTheme::sm);
                layoutRow (inner.removeFromTop (juce::jmin (knobRowH, inner.getHeight())),
                           { rippleCycles.get(), rippleSpread.get() }, RippleTheme::sm);
                break;
            }

            case Modulator::Drift:
            default:
                break;
        }

        return;
    }

    const int knobRowH = juce::jmin (knobRowHeight (RippleKnob::Size::Small), content.getHeight());
    auto knobRow = content.removeFromBottom (knobRowH);
    content.removeFromBottom (RippleTheme::sm);
    lfoView.setBounds (content);

    std::vector<juce::Component*> knobs;

    switch (currentModulator())
    {
        case Modulator::Tide:
            knobs = { tideRate.get(), tideDepth.get(), tidePhase.get() };
            break;
        case Modulator::Current:
            knobs = { currentRate.get(), currentAmount.get(), currentSmooth.get() };
            break;
        case Modulator::Drift:
            knobs = { driftRate.get(), driftAmount.get(), driftStereo.get() };
            break;
        case Modulator::Ripple:
        default:
            knobs = { rippleRate.get(), rippleDepth.get(), rippleDecay.get() };
            break;
    }

    layoutRow (knobRow, knobs, RippleTheme::xs);
}

void SynthPage::layoutSourcePanel()
{
    const auto& t = RippleTheme::get();

    auto content = contentOf (sourcePanel, kPanelInset);

    if (content.isEmpty())
        return;

    const int titleH = juce::jmax (t.sectionHeaderHeight, t.buttonHeight);
    auto titleRow = content.removeFromTop (juce::jmin (titleH, content.getHeight()));

    const int buttonW = juce::jmin (RippleTheme::grid (13), titleRow.getWidth() / 4);
    auto buttons = titleRow.removeFromRight (buttonW * 2 + RippleTheme::xs);
    buttons = buttons.withSizeKeepingCentre (buttons.getWidth(), t.buttonHeight);
    layoutRow (buttons, { &dropsButton, &resoButton }, RippleTheme::xs);

    titleRow.removeFromRight (RippleTheme::sm);
    sourceHeader.setBounds (titleRow);

    content.removeFromTop (RippleTheme::sm);

    auto selectorRow = content.removeFromTop (juce::jmin (t.selectorHeight, content.getHeight()));
    layoutRow (selectorRow, { &subWave, &noiseType }, RippleTheme::sm);

    content.removeFromTop (RippleTheme::sm);

    const int knobRowH = juce::jmin (knobRowHeight (RippleKnob::Size::Small), content.getHeight());

    layoutRow (content.removeFromTop (knobRowH),
               { subOctave.get(), subLevel.get(), noiseLevel.get(), noiseTone.get() },
               RippleTheme::xs);
}

void SynthPage::layoutDrawers (juce::Rectangle<int> pageArea)
{
    const auto& t = RippleTheme::get();

    dropsDrawer.setBounds (pageArea);
    resoDrawer .setBounds (pageArea);

    // --- Droplets -----------------------------------------------------------
    {
        auto content = contentOf (dropsDrawer, RippleTheme::lg);

        if (! content.isEmpty())
        {
            const int titleH = juce::jmax (t.sectionHeaderHeight, t.buttonHeight);
            auto titleRow = content.removeFromTop (juce::jmin (titleH, content.getHeight()));

            auto closeArea = titleRow.removeFromRight (juce::jmin (RippleTheme::grid (16),
                                                                   titleRow.getWidth() / 4));
            dropsClose.setBounds (closeArea.withSizeKeepingCentre (closeArea.getWidth(),
                                                                   t.buttonHeight));
            titleRow.removeFromRight (RippleTheme::sm);

            auto modeArea = titleRow.removeFromRight (juce::jmin (RippleTheme::grid (28),
                                                                  titleRow.getWidth() / 3));
            dropMode.setBounds (modeArea.withSizeKeepingCentre (modeArea.getWidth(),
                                                                t.selectorHeight));
            titleRow.removeFromRight (RippleTheme::sm);
            dropsHeader.setBounds (titleRow);

            content.removeFromTop (RippleTheme::md);

            const int rowH = juce::jmin (knobRowHeight (RippleKnob::Size::Small),
                                         content.getHeight());

            layoutRow (content.withSizeKeepingCentre (content.getWidth(), rowH),
                       { dropAmount.get(), dropDensity.get(), dropSize.get(), dropTone.get(),
                         dropSplash.get(), dropGravity.get(), dropBounce.get(),
                         dropRandom.get(), dropSpread.get() },
                       RippleTheme::sm);
        }
    }

    // --- Resonator ----------------------------------------------------------
    {
        auto content = contentOf (resoDrawer, RippleTheme::lg);

        if (! content.isEmpty())
        {
            const int titleH = juce::jmax (t.sectionHeaderHeight, t.buttonHeight);
            auto titleRow = content.removeFromTop (juce::jmin (titleH, content.getHeight()));

            auto closeArea = titleRow.removeFromRight (juce::jmin (RippleTheme::grid (16),
                                                                   titleRow.getWidth() / 4));
            resoClose.setBounds (closeArea.withSizeKeepingCentre (closeArea.getWidth(),
                                                                  t.buttonHeight));
            titleRow.removeFromRight (RippleTheme::sm);
            resoHeader.setBounds (titleRow);

            content.removeFromTop (RippleTheme::md);

            const int rowH = juce::jmin (knobRowHeight (RippleKnob::Size::Small),
                                         content.getHeight());

            layoutRow (content.withSizeKeepingCentre (content.getWidth(), rowH),
                       { resoAmount.get(), resoSize.get(), resoDecay.get(),
                         resoDamping.get(), resoScatter.get(), resoMotion.get() },
                       RippleTheme::sm);
        }
    }
}

} // namespace ripples
