#pragma once

/*
    RIPPLES — Preset model and preset manager.

    A preset is nothing more than a bag of NORMALISED (0..1) parameter values
    keyed by the exact IDs in Parameters/ParameterIDs.h, plus browsing metadata.
    Storing normalised values means a preset survives any later change to a
    parameter's natural range or skew: the position of the knob is preserved
    even if the Hz behind it moves.

    THREADING / REALTIME
    --------------------
    PresetManager is a MESSAGE-THREAD object. Loading a preset walks the APVTS
    and calls beginChangeGesture / setValueNotifyingHost / endChangeGesture on
    every parameter so the host sees the change and can record it. None of this
    is realtime safe and none of it is ever called from processBlock — the audio
    thread only ever reads the atomic parameter values it already reads, and
    there is no preset lookup anywhere on the audio path.
*/

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <map>
#include <vector>

namespace ripples
{

//==============================================================================
/** Browsing metadata for a preset. */
struct PresetInfo
{
    juce::String name;
    juce::String author { "Classa Music" };

    /** One of: Pad, Key, Pluck, Bass, Lead, Arp, Texture, Drone, FX. */
    juce::String category { "Pad" };

    /** Factory bank: THE SURFACE, SHALLOW WATER, DEEP BLUE, THE ABYSS,
        DROPLETS, CURRENTS, BIOLUMINESCENCE, STORMS. User presets use "USER". */
    juce::String bank { "USER" };

    /** Short sound-design note shown in the browser. */
    juce::String description;

    /** Drawn from: Deep, Wet, Dark, Bright, Dreamy, Glassy, Organic, Chaotic,
        Calm, Cinematic, Submerged, Surface. */
    juce::StringArray tags;

    bool isFactory = false;

    /** True when this preset passes the "dry aquatic test": it still reads as
        water with chorus / delay / diffusion / reverb bypassed, because its
        character comes from the oscillators, noise, the Depth filter, the
        Current / Drift / Ripple modulators, the droplets and the resonator. */
    bool dryAquatic = false;

    /** pid::kParameterVersion the preset was written with. */
    int parameterVersion = 1;

    /** Absolute path of the file a user preset came from. Empty for factory
        presets, which live in the binary and have no file. */
    juce::String filePath;

    /** Stable identity used for favourites and for re-finding a preset after a
        rescan. Bank and name together are unique. */
    juce::String getKey() const { return bank + "/" + name; }
};

//==============================================================================
/** A preset: metadata plus parameter ID -> normalised (0..1) value. */
struct Preset
{
    PresetInfo info;
    std::map<juce::String, float> values;

    bool isEmpty() const noexcept { return values.empty(); }
};

//==============================================================================
/**
    Owns the factory bank, the user bank on disk, browsing/search and the act of
    pushing a preset into the APVTS.

    The manager keeps one flat list: factory presets first (indices
    [0, getNumFactoryPresets())), then user presets. Every index-taking method
    below uses that flat list unless it says otherwise.
*/
class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& apvtsToUse);
    ~PresetManager();

    //==========================================================================
    // Loading. Message thread only.

    /** Pushes every value in the preset into the APVTS with host gestures.
        Parameters the preset does not mention are reset to their default so
        that presets can never bleed into one another. */
    void loadPreset (const Preset& preset);

    /** Loads factory preset `index` (an index into the factory range, which is
        also its index in the flat list). */
    void loadFactoryPreset (int index);

    /** Loads any preset in the flat list, factory or user. */
    void loadPresetAtIndex (int index);

    void loadNext();
    void loadPrevious();

    /** Loads INIT DEEP SAW — the honest starting point. */
    void loadInit();

    /** Loads SUBMERGED DREAMS, the preset the plug-in opens with. */
    void loadDefaultPreset();

    /** Musical randomisation: takes a curated factory preset as the structural
        seed and perturbs it within musically defensible bounds. It never
        scrambles every parameter independently — that produces noise, not
        patches. See the implementation for the exact mutation policy. */
    void randomise();

    //==========================================================================
    // Browsing.

    int getNumPresets() const;
    int getNumFactoryPresets() const;
    int getNumUserPresets() const;

    bool isUserPreset (int index) const;
    bool isValidIndex (int index) const;

    const PresetInfo& getPresetInfo (int index) const;
    const Preset& getPreset (int index) const;

    int getCurrentPresetIndex() const;
    juce::String getCurrentPresetName() const;

    /** True when a parameter has been touched since the last preset load. */
    bool isCurrentPresetModified() const;
    void markModified();

    int getDefaultPresetIndex() const;
    int indexOfPreset (const juce::String& name) const;

    //==========================================================================
    // User presets on disk.

    /** Captures the current APVTS state and writes it to the user preset
        directory. Returns false if the name is empty or the write failed. */
    bool saveUserPreset (const juce::String& name, const PresetInfo& info);

    /** Deletes the user preset at a flat-list index. Factory indices are
        ignored. */
    void deleteUserPreset (int index);

    void rescanUserPresets();

    juce::File getUserPresetDirectory() const;

    /** Snapshots every APVTS parameter into a Preset, normalised. */
    Preset captureCurrentState (const PresetInfo& info) const;

    //==========================================================================
    // Filtering.

    juce::StringArray getCategories() const;
    juce::StringArray getTags() const;
    juce::StringArray getBanks() const;

    /** Flat-list indices matching all of: a free-text query (name, author,
        bank, category, tags, description), an exact category, and every tag in
        `tags`. Empty arguments do not filter. */
    juce::Array<int> search (const juce::String& query,
                             const juce::String& category,
                             const juce::StringArray& tags) const;

    juce::Array<int> getPresetsInBank (const juce::String& bank) const;

    //==========================================================================
    // Favourites (persisted alongside the user presets).

    void setFavourite (int index, bool shouldBeFavourite);
    bool isFavourite (int index) const;
    juce::Array<int> getFavourites() const;

    //==========================================================================
    /** Called on the message thread after any load, rescan or favourite
        change, so the editor can refresh its browser. */
    std::function<void()> onPresetChanged;

private:
    void rebuildPresetList();
    void notifyChanged();
    void applyValues (const std::map<juce::String, float>& values, bool withGestures);
    static bool isGlobalPreference (const juce::String& paramID);

    juce::AudioProcessorValueTreeState& apvts;

    std::vector<Preset> presets;     // factory first, then user
    int numFactory = 0;
    int currentIndex = 0;
    bool modified = false;

    juce::StringArray favouriteKeys;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};

} // namespace ripples
