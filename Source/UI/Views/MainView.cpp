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
}

//==============================================================================
MainView::MainView (juce::AudioProcessorValueTreeState& apvts,
                    VisualizationState& vis,
                    PresetManager& presets)
    : presetManager (presets),
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
    // The whole backdrop -- water gradient, caustics and vignette -- is static
    // for a given size. Painting it live cost roughly 35 ms a frame at
    // 1600x950, because a radial gradient filled across 1.5 million pixels is
    // evaluated per pixel. Rendered once and blitted, it is effectively free.
    const float scale = juce::jlimit (0.5f, 4.0f,
                                      g.getInternalContext().getPhysicalPixelScaleFactor());

    const int wanted = juce::jmax (1, juce::roundToInt ((float) getWidth() * scale));

    if (backdrop.isNull() || std::abs (scale - backdropScale) > 0.01f
         || backdrop.getWidth() != wanted)
    {
        rebuildBackdrop (scale);
    }

    if (backdrop.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
        g.drawImage (backdrop, getLocalBounds().toFloat());
    }
    else
    {
        g.fillAll (RippleTheme::get().background);
    }
}

void MainView::rebuildBackdrop (float scale)
{
    const auto& t = RippleTheme::get();

    const int pw = juce::jmax (1, juce::roundToInt ((float) getWidth()  * scale));
    const int ph = juce::jmax (1, juce::roundToInt ((float) getHeight() * scale));

    if (getWidth() < 4 || getHeight() < 4)
    {
        backdrop = {};
        return;
    }

    backdrop = juce::Image (juce::Image::RGB, pw, ph, false);
    backdropScale = scale;

    juce::Graphics g (backdrop);
    g.addTransform (juce::AffineTransform::scale (scale));

    const auto bounds = getLocalBounds().toFloat();

    // Light at the surface, darkness below: the whole instrument sits in water.
    juce::ColourGradient water (t.backgroundLift, bounds.getCentreX(), bounds.getY(),
                                t.backgroundDeep, bounds.getCentreX(), bounds.getBottom(),
                                false);
    water.addColour (0.45, t.background);

    g.setGradientFill (water);
    g.fillRect (bounds);

    // Caustics: light through a moving surface lands in soft overlapping bands
    // rather than evenly. This is what stops the backdrop reading as flat paint
    // and gives the glass panels something to sit IN. Deliberately near the
    // threshold of visibility -- it should be felt, not noticed.
    {
        const juce::Graphics::ScopedSaveState state (g);

        const float w = bounds.getWidth();
        const float h = bounds.getHeight();

        for (int i = 0; i < 5; ++i)
        {
            // Irrational spacing so the bands never line up into a pattern.
            const float phase  = (float) i * 0.6180339f;
            const float cx     = bounds.getX() + w * (0.12f + 0.78f * std::fmod (phase * 1.37f, 1.0f));
            const float radius = w * (0.28f + 0.22f * std::fmod (phase * 2.11f, 1.0f));
            const float cy     = bounds.getY() + h * (0.05f + 0.35f * std::fmod (phase * 0.77f, 1.0f));

            juce::ColourGradient pool (t.causticFloor.withMultipliedAlpha (0.22f), cx, cy,
                                       t.causticFloor.withAlpha (0.0f),           cx, cy + radius, true);
            pool.isRadial = true;
            g.setGradientFill (pool);
            g.fillEllipse (cx - radius, cy - radius * 0.62f, radius * 2.0f, radius * 1.24f);
        }
    }

    // A vignette settles the edges so the eye lands on the Fluid Field.
    {
        juce::ColourGradient vig (t.backgroundDeep.withAlpha (0.0f), bounds.getCentreX(), bounds.getCentreY(),
                                  t.backgroundDeep.withAlpha (0.55f), bounds.getCentreX(), bounds.getBottom(), true);
        vig.isRadial = true;
        g.setGradientFill (vig);
        g.fillRect (bounds);
    }
}

//==============================================================================
void MainView::resized()
{
    const bool compact = getWidth() < kCompactWidth || getHeight() < kCompactHeight;

    const int margin = compact ? RippleTheme::md : RippleTheme::lg;
    const int gap    = compact ? RippleTheme::sm : RippleTheme::md;

    auto r = getLocalBounds().reduced (margin);

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
