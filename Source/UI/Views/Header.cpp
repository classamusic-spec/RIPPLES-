#include "UI/Views/Header.h"

#include "Parameters/ParameterEnums.h"
#include "Parameters/ParameterIDs.h"
#include "UI/Components/RippleSelector.h"
#include "UI/Components/SectionHeader.h"
#include "UI/Theme/RippleTheme.h"

#include <cmath>
#include <vector>

namespace ripples
{

namespace
{
    const juce::String kLogoText { "R I P P L E S" };
    const juce::String kTagline  { "DIVE INTO SOUND" };

    /** Metering range. Levels are linear peak; the meter is drawn in decibels. */
    constexpr float kMeterFloorDb    = -60.0f;
    constexpr float kMeterDecaySec   = 0.35f;
    constexpr float kPeakDecaySec    = 1.60f;
    constexpr float kMeterFrameShare = 2.0f;   // meter runs at half the animation rate

    juce::String u8 (const char* utf8) { return juce::String::fromUTF8 (utf8); }

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
}

//==============================================================================
/** Vertical stereo peak meter with a slow peak-hold tick. */
class Header::LevelMeter final : public juce::Component
{
public:
    LevelMeter()
    {
        setInterceptsMouseClicks (false, false);
    }

    void setLevels (float l, float r)
    {
        const auto& t = RippleTheme::get();
        const float dt = kMeterFrameShare / (float) juce::jmax (1, t.targetFrameRate);

        const float fall     = std::exp (-dt / kMeterDecaySec);
        const float peakFall = std::exp (-dt / kPeakDecaySec);

        const float targetL = toNormalised (l);
        const float targetR = toNormalised (r);

        const float newL = juce::jmax (targetL, displayL * fall);
        const float newR = juce::jmax (targetR, displayR * fall);
        const float newPL = juce::jmax (newL, peakL * peakFall);
        const float newPR = juce::jmax (newR, peakR * peakFall);

        const float epsilon = RippleTheme::get().modRingEpsilon;

        if (std::abs (newL - displayL) > epsilon || std::abs (newR - displayR) > epsilon
            || std::abs (newPL - peakL) > epsilon || std::abs (newPR - peakR) > epsilon)
        {
            displayL = newL; displayR = newR; peakL = newPL; peakR = newPR;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        const auto& t = RippleTheme::get();
        auto bounds = getLocalBounds();

        const int gap = RippleTheme::unit;
        const int barW = juce::jmax (1, (bounds.getWidth() - gap) / 2);

        auto left  = bounds.removeFromLeft (barW);
        bounds.removeFromLeft (juce::jmax (0, bounds.getWidth() - barW));
        auto right = bounds;

        paintBar (g, left.toFloat(),  displayL, peakL, t);
        paintBar (g, right.toFloat(), displayR, peakR, t);
    }

private:
    static float toNormalised (float linearGain)
    {
        const auto db = juce::Decibels::gainToDecibels (juce::jmax (0.0f, linearGain),
                                                        kMeterFloorDb);
        return juce::jlimit (0.0f, 1.0f, (db - kMeterFloorDb) / -kMeterFloorDb);
    }

    static void paintBar (juce::Graphics& g, juce::Rectangle<float> area,
                          float level, float peak, const RippleTheme& t)
    {
        g.setColour (t.panelSunken);
        g.fillRoundedRectangle (area, t.smallRadius);

        if (level > 0.0f)
        {
            auto fill = area.withTop (area.getBottom() - area.getHeight() * level);

            juce::ColourGradient grad (t.cyanDim, area.getCentreX(), area.getBottom(),
                                       t.cyanBright, area.getCentreX(), area.getY(), false);
            grad.addColour (0.7, t.cyan);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (fill, t.smallRadius);
        }

        if (peak > 0.0f)
        {
            const float y = area.getBottom() - area.getHeight() * peak;
            g.setColour (peak >= 1.0f ? t.coral : t.cyanBright);
            g.fillRect (juce::Rectangle<float> (area.getX(), y - t.borderWidth,
                                                area.getWidth(), t.borderWidth * 2.0f));
        }

        g.setColour (t.panelBorderSoft);
        g.drawRoundedRectangle (area, t.smallRadius, t.borderWidth);
    }

    float displayL = 0.0f, displayR = 0.0f, peakL = 0.0f, peakR = 0.0f;
};

//==============================================================================
/** The call-out behind SETTINGS: voice behaviour and the master shaper. */
class Header::SettingsPanel final : public juce::Component
{
public:
    explicit SettingsPanel (juce::AudioProcessorValueTreeState& apvts)
        : voiceHeader ("VOICE", "GLOBAL"),
          masterHeader ("MASTER", "OUTPUT SHAPING")
    {
        const auto& t = RippleTheme::get();

        voiceHeader.setAccent (t.cyan);
        masterHeader.setAccent (t.violet);
        addAndMakeVisible (voiceHeader);
        addAndMakeVisible (masterHeader);

        glideMode.setAccent (t.cyan);
        glideMode.setItems (toStringArray (glideModeNames));
        glideMode.attach (apvts, pid::glideMode);
        addAndMakeVisible (glideMode);

        voices  = makeKnob (*this, apvts, "VOICES", pid::voiceCount, RippleKnob::Size::Small, t.cyan,
                            "Maximum simultaneous voices.");
        bend    = makeKnob (*this, apvts, "BEND",   pid::bendRange,  RippleKnob::Size::Small, t.cyan,
                            "Pitch bend range in semitones.");
        tune    = makeKnob (*this, apvts, "TUNE",   pid::masterTune, RippleKnob::Size::Small, t.cyan,
                            "Master tuning in cents.");
        glide   = makeKnob (*this, apvts, "GLIDE",  pid::glideTime,  RippleKnob::Size::Small, t.cyan,
                            "Portamento time between notes.");

        low     = makeKnob (*this, apvts, "LOW",     pid::mastLow,     RippleKnob::Size::Small, t.violet);
        mid     = makeKnob (*this, apvts, "MID",     pid::mastMid,     RippleKnob::Size::Small, t.violet);
        high    = makeKnob (*this, apvts, "HIGH",    pid::mastHigh,    RippleKnob::Size::Small, t.violet);
        drive   = makeKnob (*this, apvts, "DRIVE",   pid::mastDrive,   RippleKnob::Size::Small, t.violet,
                            "Gentle saturation before the safety limiter.");
        ceiling = makeKnob (*this, apvts, "CEILING", pid::mastCeiling, RippleKnob::Size::Small, t.violet,
                            "Output ceiling the limiter never lets the sound past.");

        setSize (RippleTheme::grid (124), RippleTheme::grid (58));
    }

    void resized() override
    {
        const auto& t = RippleTheme::get();

        auto r = getLocalBounds().reduced (RippleTheme::md);
        const int knobRowH = t.knobSmallDiameter + t.knobLabelHeight + t.knobValueHeight;

        voiceHeader.setBounds (r.removeFromTop (t.sectionHeaderHeight));
        r.removeFromTop (RippleTheme::sm);

        auto voiceRow = r.removeFromTop (knobRowH);
        layoutRow (voiceRow, { voices.get(), bend.get(), tune.get(), glide.get(), &glideMode },
                   RippleTheme::sm);

        // The selector is a short control: centre it in the cell the row gave it.
        glideMode.setBounds (glideMode.getBounds()
                                 .withSizeKeepingCentre (glideMode.getWidth(), t.selectorHeight));

        r.removeFromTop (RippleTheme::md);

        masterHeader.setBounds (r.removeFromTop (t.sectionHeaderHeight));
        r.removeFromTop (RippleTheme::sm);

        layoutRow (r.removeFromTop (knobRowH),
                   { low.get(), mid.get(), high.get(), drive.get(), ceiling.get() },
                   RippleTheme::sm);
    }

private:
    SectionHeader  voiceHeader, masterHeader;
    RippleSelector glideMode;
    std::unique_ptr<RippleKnob> voices, bend, tune, glide;
    std::unique_ptr<RippleKnob> low, mid, high, drive, ceiling;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsPanel)
};

//==============================================================================
Header::Header (juce::AudioProcessorValueTreeState& apvts,
                VisualizationState& vis,
                PresetManager& presets)
    : state (apvts),
      visuals (vis),
      presetManager (presets),
      prevButton (u8 ("\xe2\x80\xb9")),
      nextButton (u8 ("\xe2\x80\xba"))
{
    const auto& t = RippleTheme::get();

    for (auto* b : { &prevButton, &nextButton, &favouriteButton, &browseButton,
                     &randomButton, &initButton, &settingsButton })
    {
        b->setAccent (t.cyan);
        addAndMakeVisible (*b);
    }

    prevButton.onClick = [this] { presetManager.loadPrevious(); };
    nextButton.onClick = [this] { presetManager.loadNext(); };

    favouriteButton.onClick = [this]
    {
        const auto index = presetManager.getCurrentPresetIndex();

        if (presetManager.isValidIndex (index))
            presetManager.setFavourite (index, ! presetManager.isFavourite (index));

        presetChanged();
    };

    browseButton.onClick = [this] { if (onBrowseRequested != nullptr) onBrowseRequested(); };
    randomButton.onClick = [this] { presetManager.randomise(); };
    initButton.onClick   = [this] { presetManager.loadInit(); };
    settingsButton.onClick = [this] { showSettings(); };

    browseButton.setIsPrimary (true);

    meter = std::make_unique<LevelMeter>();
    addAndMakeVisible (*meter);

    outputGain = makeKnob (*this, apvts, "OUT", pid::mastOutput,
                           RippleKnob::Size::Small, t.cyan, "Final output level.");

    updateButtonTexts (false);
    presetChanged();
}

Header::~Header() = default;

//==============================================================================
void Header::presetChanged()
{
    const auto index = presetManager.getCurrentPresetIndex();

    presetName = presetManager.getCurrentPresetName();

    if (presetName.isEmpty())
        presetName = "INIT";

    presetDetail.clear();

    if (presetManager.isValidIndex (index))
    {
        const auto& info = presetManager.getPresetInfo (index);

        juce::StringArray parts;
        parts.add (info.category.toUpperCase());

        if (info.tags.size() > 0)
            parts.add (info.tags.joinIntoString (", ").toUpperCase());

        presetDetail = parts.joinIntoString (u8 ("  \xc2\xb7  "));

        favouriteButton.setIsPrimary (presetManager.isFavourite (index));
    }

    presetIsModified = presetManager.isCurrentPresetModified();
    repaint();
}

void Header::updateButtonTexts (bool compact)
{
    favouriteButton.setButtonText (compact ? "FAV"    : "FAVOURITE");
    browseButton   .setButtonText (compact ? "BROWSE" : "BROWSE");
    randomButton   .setButtonText (compact ? "RND"    : "RANDOM");
    initButton     .setButtonText ("INIT");
    settingsButton .setButtonText (compact ? "SET"    : "SETTINGS");
}

void Header::showSettings()
{
    auto content = std::make_unique<SettingsPanel> (state);

    juce::CallOutBox::launchAsynchronously (std::move (content),
                                            settingsButton.getScreenBounds(),
                                            getTopLevelComponent());
}

//==============================================================================
void Header::resized()
{
    const auto& t = RippleTheme::get();

    auto r = getLocalBounds();

    const bool compact = getWidth() < RippleTheme::grid (280);   // 1120
    const int  gap     = compact ? RippleTheme::sm : RippleTheme::md;

    updateButtonTexts (compact);

    // --- Identity, left -----------------------------------------------------
    const int logoW = juce::roundToInt (juce::GlyphArrangement::getStringWidth (t.titleFont(),
                                                                               kLogoText))
                      + RippleTheme::md;
    logoArea = r.removeFromLeft (juce::jmin (logoW, r.getWidth() / 3));
    r.removeFromLeft (gap);

    // --- Output, right ------------------------------------------------------
    const int knobW  = compact ? RippleTheme::grid (13) : RippleTheme::grid (16);
    const int meterW = RippleTheme::grid (5);

    auto outArea = r.removeFromRight (knobW + meterW + RippleTheme::xs);

    // Cap the knob at its natural height so it never inflates in a tall header.
    const int knobH = juce::jmin (outArea.getHeight(),
                                  RippleKnob::getPreferredSize (RippleKnob::Size::Small).y);
    outputGain->setBounds (outArea.removeFromRight (knobW)
                               .withSizeKeepingCentre (knobW, knobH));
    outArea.removeFromRight (RippleTheme::xs);
    meter->setBounds (outArea.reduced (0, RippleTheme::sm));

    r.removeFromRight (gap);

    // --- Preset actions -----------------------------------------------------
    const int buttonW = compact ? RippleTheme::grid (13) : RippleTheme::grid (19);
    const int buttonGap = compact ? RippleTheme::xs : RippleTheme::sm;

    auto buttonArea = r.removeFromRight (juce::jmin (buttonW * 5 + buttonGap * 4,
                                                     juce::jmax (0, r.getWidth() / 2)));
    buttonArea = buttonArea.withSizeKeepingCentre (buttonArea.getWidth(), t.buttonHeight);

    layoutRow (buttonArea, { &favouriteButton, &browseButton, &randomButton,
                             &initButton, &settingsButton }, buttonGap);

    r.removeFromRight (gap);

    // --- Preset display, centre --------------------------------------------
    const int arrowW = RippleTheme::grid (7);

    auto prevArea = r.removeFromLeft (juce::jmin (arrowW, r.getWidth() / 4));
    auto nextArea = r.removeFromRight (juce::jmin (arrowW, r.getWidth() / 4));

    prevButton.setBounds (prevArea.withSizeKeepingCentre (prevArea.getWidth(), t.buttonHeight));
    nextButton.setBounds (nextArea.withSizeKeepingCentre (nextArea.getWidth(), t.buttonHeight));

    presetArea = r.reduced (RippleTheme::sm, 0);
}

//==============================================================================
void Header::paint (juce::Graphics& g)
{
    const auto& t = RippleTheme::get();

    // --- Logo ---------------------------------------------------------------
    auto logo = logoArea;
    const bool showTagline = logo.getHeight() >= RippleTheme::grid (16);

    auto taglineArea = showTagline ? logo.removeFromBottom (RippleTheme::grid (4))
                                   : juce::Rectangle<int>();

    g.setFont (t.titleFont());
    g.setColour (t.cyanBright);
    g.drawText (kLogoText, logo, juce::Justification::centredLeft, false);

    if (showTagline)
    {
        g.setFont (t.smallFont());
        g.setColour (t.tertiaryText);
        g.drawText (kTagline, taglineArea, juce::Justification::topLeft, false);
    }

    // --- Preset display -----------------------------------------------------
    if (presetArea.isEmpty())
        return;

    auto area = presetArea;

    const bool showDetail = area.getHeight() >= RippleTheme::grid (11)
                            && presetDetail.isNotEmpty();

    auto detailArea = showDetail ? area.removeFromBottom (RippleTheme::grid (5))
                                 : juce::Rectangle<int>();

    g.setFont (t.sectionFont());
    g.setColour (t.primaryText);
    g.drawText (presetName, area, juce::Justification::centred, true);

    if (showDetail)
    {
        g.setFont (t.smallFont());
        g.setColour (t.tertiaryText);
        g.drawText (presetDetail, detailArea, juce::Justification::centredTop, true);
    }

    if (presetIsModified)
    {
        const float dot = (float) RippleTheme::sm;
        g.setColour (t.aqua);
        g.fillEllipse ((float) presetArea.getRight() - dot,
                       (float) presetArea.getY() + dot * 0.5f, dot * 0.5f, dot * 0.5f);
    }
}

void Header::mouseDown (const juce::MouseEvent& e)
{
    if (presetArea.contains (e.getPosition()) && onBrowseRequested != nullptr)
        onBrowseRequested();
}

//==============================================================================
void Header::visibilityChanged()      { updateTimerState(); }
void Header::parentHierarchyChanged() { updateTimerState(); }

void Header::updateTimerState()
{
    const auto& t = RippleTheme::get();

    if (isShowing())
        startTimerHz (juce::jmax (1, juce::roundToInt ((float) t.targetFrameRate
                                                       / kMeterFrameShare)));
    else
        stopTimer();
}

void Header::timerCallback()
{
    meter->setLevels (visuals.getPeakL(), visuals.getPeakR());

    const auto modified = presetManager.isCurrentPresetModified();

    if (modified != presetIsModified)
    {
        presetIsModified = modified;
        repaint (presetArea);
    }
}

} // namespace ripples
