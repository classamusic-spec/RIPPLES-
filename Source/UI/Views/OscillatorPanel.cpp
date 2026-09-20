#include "UI/Views/OscillatorPanel.h"

#include "Parameters/ParameterEnums.h"
#include "Parameters/ParameterIDs.h"
#include "UI/Theme/RippleTheme.h"

#include <vector>

namespace ripples
{

namespace
{
    /** Builds an attached knob and parents it. */
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

    /** Lays components out as equal cells across a row, without rounding drift. */
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
}

//==============================================================================
OscillatorPanel::OscillatorPanel (juce::AudioProcessorValueTreeState& apvts, Slot slotToUse)
    : state (apvts),
      slot (slotToUse),
      accent (slotToUse == Slot::A ? RippleTheme::get().cyan : RippleTheme::get().aqua),
      sectionHeader (slotToUse == Slot::A ? "TIDE" : "CURRENT",
                     slotToUse == Slot::A ? "OSC A" : "OSC B")
{
    setContentInset (RippleTheme::lg);
    setAccent (accent);

    sectionHeader.setAccent (accent);
    addAndMakeVisible (sectionHeader);

    moreButton.setAccent (accent);
    moreButton.onClick = [this] { setMoreVisible (! moreVisible); };
    addAndMakeVisible (moreButton);

    waveSelector.setAccent (accent);
    waveSelector.setItems (toStringArray (oscWaveNames));
    waveSelector.attach (apvts, paramID (pid::oscAWave, pid::oscBWave));
    waveSelector.getComboBox().addListener (this);
    addAndMakeVisible (waveSelector);

    waveform.setAccent (accent);
    waveform.setAnimated (true);
    addAndMakeVisible (waveform);

    octave = makeKnob (*this, apvts, "OCTAVE", paramID (pid::oscAOctave, pid::oscBOctave),
                       RippleKnob::Size::Medium, accent, "Transposes this oscillator in octaves.");
    shape  = makeKnob (*this, apvts, "SHAPE",  paramID (pid::oscAShape,  pid::oscBShape),
                       RippleKnob::Size::Medium, accent,
                       "Reshapes the wave — width, fold or internal motion, depending on the wave.");
    detune = makeKnob (*this, apvts, "DETUNE", paramID (pid::oscADetune, pid::oscBDetune),
                       RippleKnob::Size::Medium, accent, "Spreads the unison voices apart.");
    level  = makeKnob (*this, apvts, "LEVEL",  paramID (pid::oscALevel,  pid::oscBLevel),
                       RippleKnob::Size::Medium, accent);

    shape->getSlider().addListener (this);

    if (isB())
    {
        interactionSelector = std::make_unique<RippleSelector>();
        interactionSelector->setAccent (accent);
        interactionSelector->setItems (toStringArray (interactionNames));
        interactionSelector->attach (apvts, pid::oscBInterMode);
        addAndMakeVisible (*interactionSelector);

        interactionAmount = makeKnob (*this, apvts, "AMOUNT", pid::oscBInterAmt,
                                      RippleKnob::Size::Medium, accent,
                                      "How strongly CURRENT acts on TIDE.");
    }

    // --- MORE sub-panel -----------------------------------------------------
    morePanel.setAccent (accent);
    morePanel.setContentInset (RippleTheme::md);
    addChildComponent (morePanel);

    semitone = makeKnob (morePanel, apvts, "SEMI",   paramID (pid::oscASemitone, pid::oscBSemitone),
                         RippleKnob::Size::Small, accent, "Transposes in semitones.");
    fine     = makeKnob (morePanel, apvts, "FINE",   paramID (pid::oscAFine,     pid::oscBFine),
                         RippleKnob::Size::Small, accent, "Fine tuning in cents.");
    phase    = makeKnob (morePanel, apvts, "PHASE",  paramID (pid::oscAPhase,    pid::oscBPhase),
                         RippleKnob::Size::Small, accent,
                         "Start phase on each note. Fully left runs free.");
    unison   = makeKnob (morePanel, apvts, "UNISON", paramID (pid::oscAUnison,   pid::oscBUnison),
                         RippleKnob::Size::Small, accent, "Number of stacked voices.");
    stereo   = makeKnob (morePanel, apvts, "STEREO", paramID (pid::oscAStereo,   pid::oscBStereo),
                         RippleKnob::Size::Small, accent, "Stereo spread of the unison voices.");
    pan      = makeKnob (morePanel, apvts, "PAN",    paramID (pid::oscAPan,      pid::oscBPan),
                         RippleKnob::Size::Small, accent);

    refreshWaveform();
}

OscillatorPanel::~OscillatorPanel()
{
    waveSelector.getComboBox().removeListener (this);

    if (shape != nullptr)
        shape->getSlider().removeListener (this);
}

//==============================================================================
juce::String OscillatorPanel::paramID (const char* forA, const char* forB) const
{
    return juce::String (isB() ? forB : forA);
}

void OscillatorPanel::sliderValueChanged (juce::Slider*)   { refreshWaveform(); }
void OscillatorPanel::comboBoxChanged (juce::ComboBox*)    { refreshWaveform(); }

void OscillatorPanel::refreshWaveform()
{
    const auto index = juce::jlimit (0, (int) OscWave::NumWaves - 1,
                                     waveSelector.getComboBox().getSelectedItemIndex());

    waveform.setWave ((OscWave) index, (float) shape->getSlider().getValue());
}

//==============================================================================
void OscillatorPanel::setMoreVisible (bool shouldBeVisible)
{
    moreVisible = shouldBeVisible;
    moreButton.setButtonText (moreVisible ? "LESS" : "MORE");

    // Hide what the sub-panel covers, so nothing shows through the glass.
    waveform.setVisible (! moreVisible);
    octave->setVisible (! moreVisible);
    shape->setVisible (! moreVisible);
    detune->setVisible (! moreVisible);
    level->setVisible (! moreVisible);

    if (interactionAmount != nullptr)
        interactionAmount->setVisible (! moreVisible);

    morePanel.setVisible (moreVisible);

    if (moreVisible)
        morePanel.toFront (false);

    waveform.setAnimated (! moreVisible);
    resized();
}

//==============================================================================
void OscillatorPanel::resized()
{
    // Let the vessel work out its own glass area first; getContentBounds()
    // below depends on it, and without this the panel silently paints no
    // glass at all.
    GlassPanel::resized();

    const auto& t = RippleTheme::get();

    auto content = getContentBounds();

    if (content.isEmpty())
        content = getLocalBounds().reduced (RippleTheme::lg);

    const bool  tight = content.getHeight() < RippleTheme::grid (58);   // 232
    const int   gap   = tight ? RippleTheme::sm : RippleTheme::md;

    // --- Title row, with MORE parked at its right ---------------------------
    const int titleH = juce::jmax (t.sectionHeaderHeight, t.buttonHeight);
    auto titleRow = content.removeFromTop (titleH);

    const int moreW = juce::jmin (RippleTheme::grid (16), titleRow.getWidth() / 3);
    auto moreArea = titleRow.removeFromRight (moreW);
    moreButton.setBounds (moreArea.withSizeKeepingCentre (moreArea.getWidth(),
                                                          juce::jmin (t.buttonHeight, titleH)));
    titleRow.removeFromRight (RippleTheme::sm);
    sectionHeader.setBounds (titleRow);

    content.removeFromTop (gap);

    // --- Selector row -------------------------------------------------------
    auto selectorRow = content.removeFromTop (juce::jmin (t.selectorHeight, content.getHeight()));

    if (interactionSelector != nullptr)
    {
        const int half = (selectorRow.getWidth() - RippleTheme::sm) / 2;
        waveSelector.setBounds (selectorRow.removeFromLeft (half));
        selectorRow.removeFromLeft (RippleTheme::sm);
        interactionSelector->setBounds (selectorRow);
    }
    else
    {
        waveSelector.setBounds (selectorRow);
    }

    content.removeFromTop (gap);

    // Everything below the selector row is what MORE covers.
    const auto bodyArea = content;

    if (moreVisible)
    {
        morePanel.setBounds (bodyArea);

        auto inner = morePanel.getContentBounds();

        if (inner.isEmpty())
            inner = morePanel.getLocalBounds().reduced (RippleTheme::md);

        const int rowH = (inner.getHeight() - RippleTheme::sm) / 2;

        auto top    = inner.removeFromTop (rowH);
        inner.removeFromTop (RippleTheme::sm);
        auto bottom = inner.removeFromTop (rowH);

        layoutRow (top,    { semitone.get(), fine.get(),   phase.get() }, RippleTheme::sm);
        layoutRow (bottom, { unison.get(),   stereo.get(), pan.get()   }, RippleTheme::sm);
        return;
    }

    // --- Waveform over the knob row ----------------------------------------
    auto body = bodyArea;

    const int idealKnobRow = t.knobMediumDiameter + t.knobLabelHeight + t.knobValueHeight;
    const int minKnobRow   = t.knobSmallDiameter + t.knobLabelHeight;
    const int minGraph     = RippleTheme::grid (7);

    const int knobRowH = juce::jlimit (juce::jmin (minKnobRow, body.getHeight()),
                                       idealKnobRow,
                                       body.getHeight() - minGraph - gap);

    auto knobRow = body.removeFromBottom (knobRowH);
    body.removeFromBottom (gap);
    waveform.setBounds (body);

    std::vector<juce::Component*> knobRowItems { octave.get(), shape.get(),
                                                 detune.get(), level.get() };

    if (interactionAmount != nullptr)
        knobRowItems.push_back (interactionAmount.get());

    layoutRow (knobRow, knobRowItems, tight ? RippleTheme::xs : RippleTheme::sm);
}

} // namespace ripples
