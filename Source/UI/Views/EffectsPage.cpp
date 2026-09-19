#include "UI/Views/EffectsPage.h"

#include "Parameters/ParameterEnums.h"
#include "Parameters/ParameterIDs.h"
#include "UI/Components/GlassPanel.h"
#include "UI/Components/RippleButton.h"
#include "UI/Components/RippleKnob.h"
#include "UI/Components/RippleSelector.h"
#include "UI/Components/RippleToggle.h"
#include "UI/Components/SectionHeader.h"
#include "UI/Theme/RippleTheme.h"

#include <cmath>

namespace ripples
{

namespace
{
    constexpr int kCardInset = RippleTheme::md;

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

    int knobRowHeight()
    {
        const auto& t = RippleTheme::get();
        return t.knobSmallDiameter + t.knobLabelHeight + t.knobValueHeight;
    }

    /** Lays knobs out in as few rows as the area can carry without crushing them. */
    void layoutKnobGrid (juce::Rectangle<int> area,
                         const std::vector<juce::Component*>& items,
                         int gap)
    {
        const int n = (int) items.size();

        if (n <= 0 || area.isEmpty())
            return;

        const auto& t = RippleTheme::get();

        const int idealH   = knobRowHeight();
        const int minCellW = t.knobSmallDiameter + RippleTheme::sm;

        const int maxRows  = juce::jmax (1, (area.getHeight() + gap) / (idealH + gap));
        const int wanted   = (int) std::ceil ((double) (n * minCellW)
                                              / (double) juce::jmax (1, area.getWidth()));
        const int rows     = juce::jlimit (1, juce::jmin (maxRows, n), wanted);
        const int perRow   = (n + rows - 1) / rows;

        const int rowH = juce::jmin (idealH, (area.getHeight() - gap * (rows - 1)) / rows);

        auto block = area.withSizeKeepingCentre (area.getWidth(), rowH * rows + gap * (rows - 1));

        for (int r = 0; r < rows; ++r)
        {
            std::vector<juce::Component*> line;

            for (int i = r * perRow; i < juce::jmin (n, (r + 1) * perRow); ++i)
                line.push_back (items[(size_t) i]);

            auto rowArea = block.removeFromTop (rowH);
            block.removeFromTop (gap);
            layoutRow (rowArea, line, RippleTheme::xs);
        }
    }

    juce::Rectangle<int> contentOf (GlassPanel& panel, int fallbackInset)
    {
        const auto r = panel.getContentBounds();
        return r.isEmpty() ? panel.getLocalBounds().reduced (fallbackInset) : r;
    }
}

//==============================================================================
/** One effect: bypass, the controls that get played, and MORE for the rest. */
class EffectsPage::EffectCard final : public GlassPanel
{
public:
    EffectCard (juce::AudioProcessorValueTreeState& apvtsToUse,
                const juce::String& title,
                const juce::String& subtitle,
                const juce::String& enableParamID,
                juce::Colour cardAccent)
        : apvts (apvtsToUse),
          accent (cardAccent),
          header (title, subtitle)
    {
        setContentInset (kCardInset);
        setAccent (accent);

        header.setAccent (accent);
        addAndMakeVisible (header);

        bypass.setAccent (accent);
        bypass.attach (apvts, enableParamID);
        bypass.getButton().setTooltip ("Switches this effect in and out.");
        addAndMakeVisible (bypass);

        moreButton.setAccent (accent);
        moreButton.onClick = [this] { setMoreVisible (! moreVisible); };
        addChildComponent (moreButton);

        morePanel.setAccent (accent);
        morePanel.setContentInset (RippleTheme::md);
        addChildComponent (morePanel);
    }

    void addMain (const juce::String& label, const juce::String& paramID,
                  const juce::String& tooltip = {})
    {
        mainKnobs.push_back (makeKnob (*this, label, paramID, tooltip));
    }

    void addMore (const juce::String& label, const juce::String& paramID,
                  const juce::String& tooltip = {})
    {
        moreKnobs.push_back (makeKnob (morePanel, label, paramID, tooltip));
        moreButton.setVisible (true);
    }

    void addMoreSelector (const juce::StringArray& items, const juce::String& paramID)
    {
        auto selector = std::make_unique<RippleSelector>();
        selector->setAccent (accent);
        selector->setItems (items);
        selector->attach (apvts, paramID);
        morePanel.addAndMakeVisible (*selector);
        moreShortControls.push_back (selector.get());
        moreSelectors.push_back (std::move (selector));
        moreButton.setVisible (true);
    }

    void addMoreToggle (const juce::String& text, const juce::String& paramID)
    {
        auto toggle = std::make_unique<RippleToggle> (text);
        toggle->setAccent (accent);
        toggle->attach (apvts, paramID);
        morePanel.addAndMakeVisible (*toggle);
        moreShortControls.push_back (toggle.get());
        moreToggles.push_back (std::move (toggle));
        moreButton.setVisible (true);
    }

    void resized() override
    {
        const auto& t = RippleTheme::get();

        auto content = contentOf (*this, kCardInset);

        if (content.isEmpty())
            return;

        const int titleH = juce::jmax (t.sectionHeaderHeight, t.buttonHeight);
        auto titleRow = content.removeFromTop (juce::jmin (titleH, content.getHeight()));

        if (moreButton.isVisible())
        {
            auto moreArea = titleRow.removeFromRight (juce::jmin (RippleTheme::grid (13),
                                                                  titleRow.getWidth() / 3));
            moreButton.setBounds (moreArea.withSizeKeepingCentre (moreArea.getWidth(),
                                                                  t.buttonHeight));
            titleRow.removeFromRight (RippleTheme::xs);
        }

        auto toggleArea = titleRow.removeFromRight (juce::jmin (bypass.getPreferredSize().x,
                                                                titleRow.getWidth() / 2));
        bypass.setBounds (toggleArea.withSizeKeepingCentre (toggleArea.getWidth(),
                                                            t.toggleHeight));
        titleRow.removeFromRight (RippleTheme::sm);
        header.setBounds (titleRow);

        content.removeFromTop (RippleTheme::sm);

        if (moreVisible)
        {
            morePanel.setBounds (content);

            auto inner = contentOf (morePanel, RippleTheme::md);

            if (! moreShortControls.empty())
            {
                auto shortRow = inner.removeFromTop (juce::jmin (t.selectorHeight,
                                                                 inner.getHeight()));
                layoutRow (shortRow, moreShortControls, RippleTheme::xs);

                for (auto* c : moreShortControls)
                    if (dynamic_cast<RippleToggle*> (c) != nullptr)
                        c->setBounds (c->getBounds().withSizeKeepingCentre (c->getWidth(),
                                                                            t.toggleHeight));

                inner.removeFromTop (RippleTheme::sm);
            }

            layoutKnobGrid (inner, rawPointers (moreKnobs), RippleTheme::sm);
            return;
        }

        layoutKnobGrid (content, rawPointers (mainKnobs), RippleTheme::sm);
    }

private:
    std::unique_ptr<RippleKnob> makeKnob (juce::Component& parent,
                                          const juce::String& label,
                                          const juce::String& paramID,
                                          const juce::String& tooltip)
    {
        auto knob = std::make_unique<RippleKnob> (label, RippleKnob::Size::Small);
        knob->setAccent (accent);

        if (tooltip.isNotEmpty())
            knob->setTooltipText (tooltip);

        knob->attach (apvts, paramID);
        parent.addAndMakeVisible (*knob);
        return knob;
    }

    static std::vector<juce::Component*> rawPointers (const std::vector<std::unique_ptr<RippleKnob>>& knobs)
    {
        std::vector<juce::Component*> result;
        result.reserve (knobs.size());

        for (const auto& k : knobs)
            result.push_back (k.get());

        return result;
    }

    void setMoreVisible (bool shouldBeVisible)
    {
        moreVisible = shouldBeVisible;
        moreButton.setButtonText (shouldBeVisible ? "LESS" : "MORE");

        for (const auto& k : mainKnobs)
            k->setVisible (! shouldBeVisible);

        morePanel.setVisible (shouldBeVisible);

        if (shouldBeVisible)
            morePanel.toFront (false);

        resized();
    }

    juce::AudioProcessorValueTreeState& apvts;
    const juce::Colour accent;

    SectionHeader header;
    RippleToggle  bypass { "ON" };
    RippleButton  moreButton { "MORE" };
    GlassPanel    morePanel;

    std::vector<std::unique_ptr<RippleKnob>>     mainKnobs, moreKnobs;
    std::vector<std::unique_ptr<RippleSelector>> moreSelectors;
    std::vector<std::unique_ptr<RippleToggle>>   moreToggles;
    std::vector<juce::Component*>                moreShortControls;

    bool moreVisible = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectCard)
};

//==============================================================================
EffectsPage::EffectsPage (juce::AudioProcessorValueTreeState& apvts)
{
    const auto& t = RippleTheme::get();

    auto addCard = [&] (const juce::String& title, const juce::String& subtitle,
                        const juce::String& enableID, juce::Colour accent) -> EffectCard&
    {
        auto card = std::make_unique<EffectCard> (apvts, title, subtitle, enableID, accent);
        addAndMakeVisible (*card);
        cards.push_back (std::move (card));
        return *cards.back();
    };

    // --- STEREO CURRENT -----------------------------------------------------
    {
        auto& card = addCard ("STEREO CURRENT", "FLUID MOTION", pid::scurEnable, t.aqua);
        card.addMain ("AMOUNT", pid::scurAmount, "How much the stereo image drifts.");
        card.addMain ("RATE",   pid::scurRate,   "Speed of the drift.");
        card.addMain ("WIDTH",  pid::scurWidth,  "Overall stereo width.");
    }

    // --- LIQUID CHORUS ------------------------------------------------------
    {
        auto& card = addCard ("LIQUID CHORUS", "THICKEN", pid::chorEnable, t.aqua);
        card.addMain ("RATE",  pid::chorRate);
        card.addMain ("DEPTH", pid::chorDepth);
        card.addMain ("MIX",   pid::chorMix, "Dry to wet balance for the chorus.");
        card.addMore ("DELAY", pid::chorDelay);
        card.addMore ("FDBK",  pid::chorFeedback);
        card.addMore ("WIDTH", pid::chorWidth);
    }

    // --- LIQUID DELAY -------------------------------------------------------
    {
        auto& card = addCard ("LIQUID DELAY", "MOVING REPEATS", pid::dlyEnable, t.cyan);
        card.addMain ("TIME", pid::dlyTime, "Delay time. Tempo sync lives behind MORE.");
        card.addMain ("FDBK", pid::dlyFeedback);
        card.addMain ("MIX",  pid::dlyMix);
        card.addMoreToggle ("SYNC", pid::dlySync);
        card.addMoreSelector (toStringArray (syncDivisionNames), pid::dlySyncTime);
        card.addMore ("MOTION", pid::dlyMotion, "Pitch movement in the repeats.");
        card.addMore ("SPREAD", pid::dlySpread);
        card.addMore ("DAMP",   pid::dlyDamping);
        card.addMore ("DIFF",   pid::dlyDiffusion);
    }

    // --- DIFFUSION ----------------------------------------------------------
    {
        auto& card = addCard ("DIFFUSION", "BLUR AND MIST", pid::diffEnable, t.cyan);
        card.addMain ("AMOUNT", pid::diffAmount, "How far the sound is smeared.");
        card.addMain ("SIZE",   pid::diffSize);
        card.addMain ("MIX",    pid::diffMix);
        card.addMore ("DAMP",   pid::diffDamping);
    }

    // --- ABYSS REVERB -------------------------------------------------------
    {
        auto& card = addCard ("ABYSS REVERB", "THE SPACE AROUND", pid::verbEnable, t.violet);
        card.addMain ("SIZE",  pid::verbSize, "How large the space is.");
        card.addMain ("DECAY", pid::verbDecay);
        card.addMain ("MIX",   pid::verbMix);
        card.addMore ("PREDLY", pid::verbPredelay);
        card.addMore ("DAMP",   pid::verbDamping);
        card.addMore ("LOW CUT", pid::verbLowCut);
        card.addMore ("HI CUT",  pid::verbHighCut);
        card.addMore ("MOD",     pid::verbMod, "Movement inside the reverb tail.");
    }
}

EffectsPage::~EffectsPage() = default;

//==============================================================================
void EffectsPage::resized()
{
    auto r = getLocalBounds();

    const int gap = r.getWidth() >= RippleTheme::grid (250) ? RippleTheme::md : RippleTheme::sm;

    std::vector<juce::Component*> items;
    items.reserve (cards.size());

    for (const auto& card : cards)
        items.push_back (card.get());

    layoutRow (r, items, gap);
}

} // namespace ripples
