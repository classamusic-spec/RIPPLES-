#include "UI/Views/ModulationPage.h"

#include "Parameters/ParameterEnums.h"
#include "Parameters/ParameterIDs.h"
#include "UI/Components/ModulationMeter.h"
#include "UI/Components/RippleSelector.h"
#include "UI/Components/RippleToggle.h"
#include "UI/Theme/RippleTheme.h"

#include <vector>

namespace ripples
{

namespace
{
    constexpr int kPanelInset   = RippleTheme::md;

    /** Two columns of six once the matrix is at least this wide. */
    constexpr int kTwoColumnWidth = RippleTheme::grid (215);   // 860

    constexpr int kMeterWidth   = RippleTheme::grid (8);       // 32
    constexpr int kMinComboW    = RippleTheme::grid (17);      // 68
    constexpr int kMaxComboW    = RippleTheme::grid (30);      // 120
    constexpr int kIndexWidth   = RippleTheme::grid (6);       // 24
    constexpr int kMinAmountW   = RippleTheme::grid (18);      // 72
}

//==============================================================================
/** The scrolling body of the matrix: twelve slots in one or two columns. */
class ModulationPage::MatrixContent final : public juce::Component
{
public:
    MatrixContent (juce::AudioProcessorValueTreeState& apvts, VisualizationState& vis)
        : visuals (vis)
    {
        const auto& t = RippleTheme::get();

        rows.reserve ((size_t) pid::kNumModSlots);

        for (int i = 0; i < pid::kNumModSlots; ++i)
        {
            auto row = std::make_unique<Row>();

            row->source.setAccent (t.aqua);
            row->source.setItems (toStringArray (modSourceNames));
            row->source.attach (apvts, pid::modSourceID (i));
            addAndMakeVisible (row->source);

            row->destination.setAccent (t.cyan);
            row->destination.setItems (toStringArray (modDestNames));
            row->destination.attach (apvts, pid::modDestID (i));
            addAndMakeVisible (row->destination);

            row->amount.setSliderStyle (juce::Slider::LinearHorizontal);
            row->amount.setTextBoxStyle (juce::Slider::TextBoxRight, false,
                                         RippleTheme::grid (11), t.selectorHeight);
            row->amount.setColour (juce::Slider::backgroundColourId, t.knobTrack);
            row->amount.setColour (juce::Slider::trackColourId, t.aqua);
            row->amount.setColour (juce::Slider::thumbColourId, t.cyanBright);
            row->amount.setColour (juce::Slider::textBoxTextColourId, t.secondaryText);
            row->amount.setColour (juce::Slider::textBoxBackgroundColourId, t.controlFill);
            row->amount.setColour (juce::Slider::textBoxOutlineColourId, t.controlBorder);
            row->amount.setTooltip ("How far this source moves its destination.");
            addAndMakeVisible (row->amount);

            row->amountAttachment = std::make_unique<Attachment> (apvts, pid::modAmountID (i),
                                                                  row->amount);

            row->bipolar.setAccent (t.violet);
            row->bipolar.attach (apvts, pid::modBipolarID (i));
            row->bipolar.getButton().setTooltip ("Treat a one-directional source as bipolar.");
            addAndMakeVisible (row->bipolar);

            row->meter.setAccent (t.aqua);
            addAndMakeVisible (row->meter);

            row->sourceValue = apvts.getRawParameterValue (pid::modSourceID (i));
            row->amountValue = apvts.getRawParameterValue (pid::modAmountID (i));

            rows.push_back (std::move (row));
        }

        fluidX = apvts.getParameter (pid::fluidX);
        fluidY = apvts.getParameter (pid::fluidY);
    }

    /** The height this content needs at the given width. */
    int getPreferredHeight (int width) const
    {
        const auto& t = RippleTheme::get();

        const int columns = width >= kTwoColumnWidth ? 2 : 1;
        const int perCol  = (pid::kNumModSlots + columns - 1) / columns;
        const int rowH    = t.selectorHeight + RippleTheme::xs;

        return t.sectionHeaderHeight + RippleTheme::sm + perCol * rowH;
    }

    void resized() override
    {
        const auto& t = RippleTheme::get();

        geometry.clear();

        auto area = getLocalBounds();

        const int columns = area.getWidth() >= kTwoColumnWidth ? 2 : 1;
        const int perCol  = (pid::kNumModSlots + columns - 1) / columns;
        const int rowH    = t.selectorHeight + RippleTheme::xs;

        headerArea = area.removeFromTop (t.sectionHeaderHeight);
        area.removeFromTop (RippleTheme::sm);

        const float colW = (float) (area.getWidth() - RippleTheme::lg * (columns - 1))
                           / (float) columns;

        for (int c = 0; c < columns; ++c)
        {
            const auto columnArea = juce::Rectangle<float> ((float) area.getX()
                                                                + (colW + (float) RippleTheme::lg) * (float) c,
                                                            (float) area.getY(),
                                                            colW, (float) area.getHeight())
                                        .toNearestInt();

            Column column;
            column.bounds = columnArea;

            for (int r = 0; r < perCol; ++r)
            {
                const int index = c * perCol + r;

                if (index >= (int) rows.size())
                    break;

                auto& row = *rows[(size_t) index];
                auto cell = columnArea.withHeight (rowH).withY (columnArea.getY() + r * rowH);
                cell = cell.reduced (0, RippleTheme::unit / 2);

                cell.removeFromLeft (kIndexWidth);

                auto meterArea = cell.removeFromRight (kMeterWidth);
                cell.removeFromRight (RippleTheme::xs);

                auto toggleArea = cell.removeFromRight (t.toggleWidth);
                cell.removeFromRight (RippleTheme::xs);

                const int comboW = juce::jlimit (juce::jmin (kMinComboW, cell.getWidth() / 3),
                                                 kMaxComboW,
                                                 (cell.getWidth() - kMinAmountW) / 2
                                                     - RippleTheme::xs);

                auto sourceArea = cell.removeFromLeft (comboW);
                cell.removeFromLeft (RippleTheme::xs);
                auto destArea = cell.removeFromLeft (comboW);
                cell.removeFromLeft (RippleTheme::xs);

                row.source.setBounds (sourceArea);
                row.destination.setBounds (destArea);
                row.amount.setBounds (cell);
                row.bipolar.setBounds (toggleArea.withSizeKeepingCentre (toggleArea.getWidth(),
                                                                         t.toggleHeight));
                row.meter.setBounds (meterArea.reduced (0, RippleTheme::unit / 2));

                if (r == 0)
                {
                    column.sourceX = sourceArea.getX();
                    column.sourceW = sourceArea.getWidth();
                    column.destX   = destArea.getX();
                    column.destW   = destArea.getWidth();
                    column.amountX = cell.getX();
                    column.amountW = cell.getWidth();
                }
            }

            geometry.push_back (column);
        }
    }

    void paint (juce::Graphics& g) override
    {
        const auto& t = RippleTheme::get();

        g.setFont (t.smallFont());
        g.setColour (t.tertiaryText);

        for (const auto& column : geometry)
        {
            const auto label = [&] (int x, int w, const juce::String& text,
                                    juce::Justification just)
            {
                g.drawText (text, juce::Rectangle<int> (x, headerArea.getY(),
                                                        w, headerArea.getHeight()),
                            just, false);
            };

            label (column.sourceX, column.sourceW, "SOURCE", juce::Justification::centredLeft);
            label (column.destX,   column.destW,   "DESTINATION", juce::Justification::centredLeft);
            label (column.amountX, column.amountW, "AMOUNT", juce::Justification::centredLeft);
        }

        // Slot numbers, and a hairline under every row.
        g.setFont (t.smallFont());

        for (size_t i = 0; i < rows.size(); ++i)
        {
            const auto& row = *rows[i];
            const auto bounds = row.source.getBounds();

            if (bounds.isEmpty())
                continue;

            g.setColour (t.disabledText);
            g.drawText (juce::String ((int) i + 1).paddedLeft ('0', 2),
                        juce::Rectangle<int> (bounds.getX() - kIndexWidth, bounds.getY(),
                                              kIndexWidth - RippleTheme::xs, bounds.getHeight()),
                        juce::Justification::centredRight, false);
        }
    }

    /** Pushes the live source values into the meters. */
    void refreshMeters()
    {
        for (auto& rowPtr : rows)
        {
            auto& row = *rowPtr;

            const auto source = (ModSource) juce::jlimit (0, (int) ModSource::NumSources - 1,
                                                          (int) (row.sourceValue != nullptr
                                                                     ? row.sourceValue->load()
                                                                     : 0.0f));

            const float amount = row.amountValue != nullptr ? row.amountValue->load() : 0.0f;

            row.meter.setValue (juce::jlimit (-1.0f, 1.0f, valueOf (source) * amount));
        }
    }

private:
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    struct Row
    {
        RippleSelector source, destination;
        juce::Slider   amount;
        std::unique_ptr<Attachment> amountAttachment;
        RippleToggle    bipolar;
        ModulationMeter meter;

        std::atomic<float>* sourceValue = nullptr;
        std::atomic<float>* amountValue = nullptr;
    };

    struct Column
    {
        juce::Rectangle<int> bounds;
        int sourceX = 0, sourceW = 0, destX = 0, destW = 0, amountX = 0, amountW = 0;
    };

    float valueOf (ModSource source) const
    {
        switch (source)
        {
            case ModSource::AmpEnvelope: return visuals.getAmpEnvValue();
            case ModSource::ModEnvelope: return visuals.getModEnvValue();
            case ModSource::Tide:        return visuals.getTideValue();
            case ModSource::Current:     return visuals.getCurrentValue();
            case ModSource::Drift:       return visuals.getDriftValue();
            case ModSource::Ripple:      return visuals.getRippleValue();
            case ModSource::FluidFieldX: return fluidX != nullptr ? fluidX->getValue() * 2.0f - 1.0f : 0.0f;
            case ModSource::FluidFieldY: return fluidY != nullptr ? fluidY->getValue() * 2.0f - 1.0f : 0.0f;

            case ModSource::None:
            case ModSource::Velocity:
            case ModSource::Note:
            case ModSource::KeyTrack:
            case ModSource::ModWheel:
            case ModSource::Aftertouch:
            case ModSource::RandomPerNote:
            case ModSource::NumSources:
            default:
                return 0.0f;
        }
    }

    VisualizationState& visuals;

    std::vector<std::unique_ptr<Row>> rows;
    std::vector<Column> geometry;
    juce::Rectangle<int> headerArea;

    juce::RangedAudioParameter* fluidX = nullptr;
    juce::RangedAudioParameter* fluidY = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MatrixContent)
};

//==============================================================================
ModulationPage::ModulationPage (juce::AudioProcessorValueTreeState& apvts,
                                VisualizationState& vis)
{
    const auto& t = RippleTheme::get();

    matrixPanel.setContentInset (kPanelInset);
    matrixPanel.setAccent (t.aqua);
    addAndMakeVisible (matrixPanel);

    matrix = std::make_unique<MatrixContent> (apvts, vis);

    viewport.setViewedComponent (matrix.get(), false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (t.scrollbarWidth);
    viewport.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId, t.scrollbarThumb);
    viewport.getVerticalScrollBar().setColour (juce::ScrollBar::trackColourId, t.panelSunken);
    matrixPanel.addAndMakeVisible (viewport);

    fluidPanel.setContentInset (kPanelInset);
    fluidPanel.setAccent (t.violet);
    addAndMakeVisible (fluidPanel);

    fluidHeader.setAccent (t.violet);
    fluidPanel.addAndMakeVisible (fluidHeader);

    auto makeFluidKnob = [&] (const juce::String& label, const juce::String& paramID,
                              const juce::String& tip)
    {
        auto knob = std::make_unique<RippleKnob> (label, RippleKnob::Size::Small);
        knob->setAccent (t.violet);
        knob->setTooltipText (tip);
        knob->attach (apvts, paramID);
        fluidPanel.addAndMakeVisible (*knob);
        return knob;
    };

    fluidXAmount = makeFluidKnob ("X DEPTH", pid::fluidXAmount,
                                  "How far the CALM / CHAOS axis reaches into this preset.");
    fluidYAmount = makeFluidKnob ("Y DEPTH", pid::fluidYAmount,
                                  "How far the SURFACE / DEPTH axis reaches into this preset.");
}

ModulationPage::~ModulationPage()
{
    viewport.setViewedComponent (nullptr, false);
}

//==============================================================================
void ModulationPage::resized()
{
    const auto& t = RippleTheme::get();

    auto r = getLocalBounds();

    const int fluidW = juce::jlimit (RippleTheme::grid (30), RippleTheme::grid (44),
                                     r.getWidth() / 6);

    fluidPanel.setBounds (r.removeFromRight (fluidW));
    r.removeFromRight (RippleTheme::md);
    matrixPanel.setBounds (r);

    // --- Matrix -------------------------------------------------------------
    auto inner = matrixPanel.getContentBounds();

    if (inner.isEmpty())
        inner = matrixPanel.getLocalBounds().reduced (kPanelInset);

    viewport.setBounds (inner);

    int contentW = inner.getWidth();
    int contentH = matrix->getPreferredHeight (contentW);

    if (contentH > inner.getHeight())
    {
        contentW = juce::jmax (RippleTheme::grid (40), contentW - t.scrollbarWidth);
        contentH = matrix->getPreferredHeight (contentW);
    }

    matrix->setSize (contentW, juce::jmax (contentH, inner.getHeight()));

    // --- Fluid field depth --------------------------------------------------
    auto fluid = fluidPanel.getContentBounds();

    if (fluid.isEmpty())
        fluid = fluidPanel.getLocalBounds().reduced (kPanelInset);

    fluidHeader.setBounds (fluid.removeFromTop (juce::jmin (t.sectionHeaderHeight,
                                                            fluid.getHeight())));
    fluid.removeFromTop (RippleTheme::sm);

    const int knobRowH = juce::jmin (t.knobSmallDiameter + t.knobLabelHeight + t.knobValueHeight,
                                     fluid.getHeight());
    auto knobRow = fluid.removeFromTop (knobRowH);

    const int half = (knobRow.getWidth() - RippleTheme::sm) / 2;
    fluidXAmount->setBounds (knobRow.removeFromLeft (half));
    knobRow.removeFromLeft (RippleTheme::sm);
    fluidYAmount->setBounds (knobRow);
}

//==============================================================================
void ModulationPage::visibilityChanged()      { updateTimerState(); }
void ModulationPage::parentHierarchyChanged() { updateTimerState(); }

void ModulationPage::updateTimerState()
{
    const auto& t = RippleTheme::get();

    if (isShowing())
        startTimerHz (juce::jmax (1, t.targetFrameRate / 2));
    else
        stopTimer();
}

void ModulationPage::timerCallback()
{
    matrix->refreshMeters();
}

} // namespace ripples
