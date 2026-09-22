#include "UI/Views/MainView.h"

#include <cmath>

#include "UI/Theme/RippleTheme.h"

namespace ripples
{

namespace
{
    /** Below these the interface switches to its compact spacing. */
    constexpr int kCompactWidth  = RippleTheme::grid (280);   // 1120
    constexpr int kCompactHeight = RippleTheme::grid (175);   // 700

    constexpr int kHeaderTall    = RippleTheme::grid (20);    // 80
    constexpr int kHeaderShort   = RippleTheme::grid (16);    // 64
    constexpr int kMacroTall     = RippleTheme::grid (31);    // 124
    constexpr int kMacroShort    = RippleTheme::grid (25);    // 100

    constexpr int kMinUpper      = RippleTheme::grid (42);    // 168
    constexpr int kMinContent    = RippleTheme::grid (38);    // 152
    constexpr float kUpperShare  = 0.6f;

    constexpr int kSidePanelMin  = RippleTheme::grid (52);    // 208
    constexpr int kSidePanelMax  = RippleTheme::grid (86);    // 344
    constexpr float kSideShare   = 0.235f;

    const juce::StringArray kTabNames { "SYNTH", "MODULATION", "FX", "PRESETS" };

    //==========================================================================
    // The bottom strip. Its height and colour come from the theme; the strings
    // and the two values below are the only things the theme does not name.
    //==========================================================================
    const juce::String kFooterWordmark { "R I P P L E S" };
    const juce::String kFooterTagline  { "A N   O C E A N   O F   P O S S I B I L I T Y" };

   #if defined (JucePlugin_VersionString)
    const juce::String kFooterVersion { juce::String ("v") + JucePlugin_VersionString };
   #else
    const juce::String kFooterVersion { "v0.1.0" };
   #endif

    /** How much dimmer the version reads than the wordmark beside it. */
    constexpr float kFooterDimAlpha = 0.62f;

    /** Length of the rule that trails the tagline. */
    constexpr int kFooterRuleWidth = RippleTheme::grid (9);   // 36
}

//==============================================================================
MainView::MainView (juce::AudioProcessorValueTreeState& apvts,
                    VisualizationState& vis,
                    PresetManager& presets)
    : presetManager (presets),
      backdrop (vis),
      header (apvts, vis, presets),
      oscillatorA (apvts, OscillatorPanel::Slot::A),
      oscillatorB (apvts, OscillatorPanel::Slot::B),
      fluidField (apvts, vis),
      macros (apvts),
      synthPage (apvts),
      modulationPage (apvts, vis),
      effectsPage (apvts),
      presetBrowser (presets)
{
    addAndMakeVisible (backdrop);   // first, so it sits behind everything
    addAndMakeVisible (header);
    addAndMakeVisible (oscillatorA);
    addAndMakeVisible (fluidField);
    addAndMakeVisible (oscillatorB);
    addAndMakeVisible (macros);
    addAndMakeVisible (tabs);

    addChildComponent (synthPage);
    addChildComponent (modulationPage);
    addChildComponent (effectsPage);
    addChildComponent (presetBrowser);

    tabs.setTabs (kTabNames);
    tabs.onTabChanged = [this] (int index) { showPage (index); };

    header.onBrowseRequested = [this] { tabs.setSelectedTab (kTabNames.size() - 1, true); };

    // Keep whatever the processor already had hooked up, and put it back on the
    // way out so nothing is left pointing at a destroyed editor.
    previousPresetCallback = presetManager.onPresetChanged;

    presetManager.onPresetChanged = [this]
    {
        header.presetChanged();
        presetBrowser.presetChanged();

        if (previousPresetCallback != nullptr)
            previousPresetCallback();
    };

    showPage (0);
}

MainView::~MainView()
{
    presetManager.onPresetChanged = previousPresetCallback;
}

//==============================================================================
void MainView::showPage (int index)
{
    synthPage     .setVisible (index == 0);
    modulationPage.setVisible (index == 1);
    effectsPage   .setVisible (index == 2);
    presetBrowser .setVisible (index == 3);
}

//==============================================================================
void MainView::paint (juce::Graphics& g)
{
    // The animated water is a child (LiquidBackdrop), drawn on top of this and
    // behind every panel. All that is left here is a solid base in case the
    // backdrop has not been sized yet.
    g.fillAll (RippleTheme::get().background);
}

void MainView::paintOverChildren (juce::Graphics& g)
{
    // The footer sits over the animated backdrop, not under it.
    if (! footerArea.isEmpty())
        paintFooter (g);
}

//==============================================================================
void MainView::layoutFooter()
{
    const auto& t = RippleTheme::get();
    const auto font = t.smallFont();

    footerLeftArea = footerVersionArea = footerRightArea = footerRuleArea = {};

    auto f = footerArea;

    if (f.isEmpty())
        return;

    const int wLeft    = juce::roundToInt (juce::GlyphArrangement::getStringWidth (font, kFooterWordmark))
                           + RippleTheme::xs;
    const int wVersion = juce::roundToInt (juce::GlyphArrangement::getStringWidth (font, kFooterVersion))
                           + RippleTheme::xs;
    const int wRight   = juce::roundToInt (juce::GlyphArrangement::getStringWidth (font, kFooterTagline))
                           + RippleTheme::xs;

    // The tagline is decoration: it goes only if the identity still fits beside it.
    if (f.getWidth() >= wLeft + wVersion + wRight + kFooterRuleWidth + RippleTheme::xxl)
    {
        footerRuleArea = f.removeFromRight (kFooterRuleWidth);
        f.removeFromRight (RippleTheme::sm);
        footerRightArea = f.removeFromRight (wRight);
        f.removeFromRight (RippleTheme::lg);
    }

    footerLeftArea = f.removeFromLeft (juce::jmin (wLeft, f.getWidth()));

    if (f.getWidth() >= wVersion + RippleTheme::sm)
    {
        f.removeFromLeft (RippleTheme::sm);
        footerVersionArea = f.removeFromLeft (wVersion);
    }
}

void MainView::paintFooter (juce::Graphics& g) const
{
    const auto& t = RippleTheme::get();

    g.setFont (t.smallFont());

    if (! footerLeftArea.isEmpty())
    {
        g.setColour (t.footerText);
        g.drawText (kFooterWordmark, footerLeftArea, juce::Justification::centredLeft, false);
    }

    if (! footerVersionArea.isEmpty())
    {
        g.setColour (t.footerText.withMultipliedAlpha (kFooterDimAlpha));
        g.drawText (kFooterVersion, footerVersionArea, juce::Justification::centredLeft, false);
    }

    if (! footerRightArea.isEmpty())
    {
        g.setColour (t.footerText);
        g.drawText (kFooterTagline, footerRightArea, juce::Justification::centredRight, false);
    }

    if (! footerRuleArea.isEmpty())
    {
        const auto rule = footerRuleArea.toFloat()
                              .withSizeKeepingCentre ((float) footerRuleArea.getWidth(), t.borderWidth);

        juce::ColourGradient fade (t.footerText.withAlpha (0.0f), rule.getX(),     rule.getCentreY(),
                                   t.footerText,                  rule.getRight(), rule.getCentreY(),
                                   false);
        g.setGradientFill (fade);
        g.fillRect (rule);
    }
}

//==============================================================================
void MainView::resized()
{
    // The animated water fills the whole window, behind everything else.
    backdrop.setBounds (getLocalBounds());

    const bool compact = getWidth() < kCompactWidth || getHeight() < kCompactHeight;

    const int margin = compact ? RippleTheme::md : RippleTheme::lg;
    const int gap    = compact ? RippleTheme::sm : RippleTheme::md;

    auto r = getLocalBounds().reduced (margin);

    footerArea = footerLeftArea = footerVersionArea = footerRightArea = footerRuleArea = {};

    if (r.isEmpty())
        return;

    // --- Footer -------------------------------------------------------------
    // Claimed before anything else is measured, so no page can ever run into it.
    {
        const int footerH = juce::jmin (juce::roundToInt (RippleTheme::get().footerHeight),
                                        juce::jmax (0, r.getHeight() / 5));

        footerArea = r.removeFromBottom (footerH);
        r.removeFromBottom (juce::jmin (compact ? RippleTheme::xs : RippleTheme::sm,
                                        r.getHeight()));
        layoutFooter();
    }

    if (r.isEmpty())
        return;

    // --- Header -------------------------------------------------------------
    header.setBounds (r.removeFromTop (juce::jmin (compact ? kHeaderShort : kHeaderTall,
                                                   r.getHeight())));
    r.removeFromTop (gap);

    // --- Vertical budget ----------------------------------------------------
    const int macroH = juce::jmin (compact ? kMacroShort : kMacroTall, r.getHeight());
    const int tabsH  = juce::jmin (compact ? RippleTheme::grid (7) : tabs.getPreferredHeight(),
                                   r.getHeight());

    const int flexible = juce::jmax (0, r.getHeight() - macroH - tabsH - gap * 3);

    const int upperH = juce::jlimit (juce::jmin (kMinUpper, flexible),
                                     juce::jmax (juce::jmin (kMinUpper, flexible),
                                                 flexible - kMinContent),
                                     juce::roundToInt ((float) flexible * kUpperShare));

    auto upper = r.removeFromTop (upperH);
    r.removeFromTop (gap);

    macros.setBounds (r.removeFromTop (macroH));
    r.removeFromTop (gap);

    tabs.setBounds (r.removeFromTop (tabsH));
    r.removeFromTop (gap);

    // --- Pages --------------------------------------------------------------
    const auto pageArea = r;

    synthPage     .setBounds (pageArea);
    modulationPage.setBounds (pageArea);
    effectsPage   .setBounds (pageArea);
    presetBrowser .setBounds (pageArea);

    // --- Upper row: TIDE | FLUID FIELD | CURRENT ----------------------------
    const int sideW = juce::jlimit (juce::jmin (kSidePanelMin, upper.getWidth() / 3),
                                    kSidePanelMax,
                                    juce::roundToInt ((float) upper.getWidth() * kSideShare));

    oscillatorA.setBounds (upper.removeFromLeft (sideW));
    upper.removeFromLeft (gap);
    oscillatorB.setBounds (upper.removeFromRight (sideW));
    upper.removeFromRight (gap);
    fluidField.setBounds (upper);
}

} // namespace ripples
