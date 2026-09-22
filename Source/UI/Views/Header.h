#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Presets/PresetManager.h"
#include "UI/Theme/RippleTheme.h"
#include "Utilities/VisualizationState.h"

#include <functional>
#include <memory>

namespace ripples
{

//==============================================================================
/**
    The top band.

        WORDMARK        R I P P L E S  /  D I V E   I N T O   S O U N D
        CAPSULE         ‹  preset name  /  CATEGORY · TAG · TAG  ›
        ACTIONS         heart, die, then BROWSE / INIT / SETTINGS as plain links
        OUTPUT          a live scope over a horizontal level bar

    The capsule, the wordmark metrics and every glyph path are built in
    resized(); paint() only fills already-measured rectangles and already-built
    paths. The one moving part is the output cluster, whose two small children
    repaint themselves so a level change never invalidates the whole band, and
    whose timer stops the moment the header is off screen.
*/
class Header final : public juce::Component,
                     private juce::Timer
{
public:
    Header (juce::AudioProcessorValueTreeState& apvts,
            VisualizationState& vis,
            PresetManager& presets);
    ~Header() override;

    /** Raised when the player asks for the browser (BROWSE, or the preset name). */
    std::function<void()> onBrowseRequested;

    /** Re-reads the current preset from the manager and repaints. */
    void presetChanged();

    //==========================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void visibilityChanged() override;
    void parentHierarchyChanged() override;

private:
    //==========================================================================
    class GlyphButton;     // heart / die / chevron, drawn as a cached path
    class TextLink;        // dim, letter-spaced text button
    class Scope;           // rolling output waveform
    class OutputBar;       // level readout that doubles as the output gain
    class SettingsPanel;   // voice + master controls, shown in a call-out

    void timerCallback() override;
    void updateTimerState();
    void showSettings();

    /** Width the action row needs with `linkLevel` text links visible (0..3). */
    int measureActions (int linkLevel) const;
    void placeActions (juce::Rectangle<int> area, int linkLevel);

    /** Re-fits the wordmark and the preset name to the space they were given. */
    void updateWordmarkFont (int maxWidth, bool compact);
    void updatePresetFont();

    //==========================================================================
    juce::AudioProcessorValueTreeState& state;
    VisualizationState& visuals;
    PresetManager& presetManager;

    std::unique_ptr<GlyphButton> prevButton, nextButton, favouriteButton, randomButton;
    std::unique_ptr<TextLink>    browseLink, initLink, settingsLink;
    std::unique_ptr<Scope>       scope;
    std::unique_ptr<OutputBar>   outputBar;

    // Painted areas, all filled in by resized().
    juce::Rectangle<int> logoArea, taglineArea, capsuleArea;
    juce::Rectangle<int> presetTextArea, presetNameArea, presetTagArea;
    juce::Rectangle<int> outLabelArea;

    juce::Font wordmarkFont { RippleTheme::get().titleFont() };
    juce::Font presetFont   { RippleTheme::get().sectionFont() };

    juce::String presetName, presetDetail, outputValueText;
    bool presetIsModified = false;
    bool showTagLine = true;
    bool presetHovered = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Header)
};

} // namespace ripples
