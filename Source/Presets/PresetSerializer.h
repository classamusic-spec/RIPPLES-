#pragma once

/*
    RIPPLES — user preset persistence.

    Factory presets are compiled-in C++ data (see FactoryPresets.h) and never
    touch the disk. This class exists only for USER presets and the favourites
    list, both of which live in

        <userApplicationDataDirectory>/RIPPLES/Presets

    Everything here runs on the message thread. Nothing here is called from the
    audio thread, directly or indirectly.

    Robustness rule: a missing, unreadable, truncated, or garbage file is never
    fatal. Every entry point returns false / an empty result and the caller
    simply skips that preset.
*/

#include "Presets/PresetManager.h"

#include <memory>
#include <vector>

namespace ripples
{

class PresetSerializer
{
public:
    //==========================================================================
    static constexpr const char* kFileExtension  = ".ripple";
    static constexpr const char* kRootTag        = "RIPPLES_PRESET";
    static constexpr const char* kInfoTag        = "INFO";
    static constexpr const char* kParametersTag  = "PARAMETERS";
    static constexpr const char* kParamTag       = "PARAM";
    static constexpr const char* kFavouritesTag  = "RIPPLES_FAVOURITES";
    static constexpr const char* kFavouriteTag   = "FAVOURITE";
    static constexpr const char* kFavouritesFile = "Favourites.xml";

    /** Hard ceiling on how many <PARAM> entries we will read from one file, so
        that a hostile or corrupt file cannot make us allocate unboundedly. */
    static constexpr int kMaxParametersPerFile = 4096;

    //==========================================================================
    // Locations.

    /** <userApplicationDataDirectory>/RIPPLES/Presets — created on demand. */
    static juce::File getUserPresetDirectory();

    /** Strips characters that are illegal or awkward in a file name. */
    static juce::String sanitiseName (const juce::String& name);

    /** Path a preset of this name would be written to. */
    static juce::File fileForPreset (const juce::String& presetName);

    //==========================================================================
    // Conversion.

    static std::unique_ptr<juce::XmlElement> toXml (const Preset& preset);

    /** Fills `result` from XML. Returns false if the element is not a RIPPLES
        preset or carries no usable parameters. Values are clamped to 0..1 and
        non-finite values are dropped. */
    static bool fromXml (const juce::XmlElement& xml, Preset& result);

    //==========================================================================
    // Files.

    static bool saveToFile (const Preset& preset, const juce::File& file);

    /** Returns false (without throwing, asserting or logging) for any file that
        does not exist, is empty, is not XML, or is not a RIPPLES preset. */
    static bool loadFromFile (const juce::File& file, Preset& result);

    /** Every readable *.ripple under `directory` (recursive), sorted by name.
        Unreadable files are silently skipped. */
    static std::vector<Preset> scanDirectory (const juce::File& directory);

    //==========================================================================
    // Favourites — a flat list of PresetInfo::getKey() strings.

    static juce::StringArray loadFavourites();
    static bool saveFavourites (const juce::StringArray& keys);

    //==========================================================================
    /** Version policy.

        Preset values are normalised, so they stay meaningful across parameter
        range changes; that is the whole reason we store them that way. A file
        from a different pid::kParameterVersion is therefore still loaded:
        parameters that no longer exist are ignored by PresetManager, and
        parameters that did not exist yet fall back to their defaults. The file
        version is kept in PresetInfo::parameterVersion so the UI can mention
        it. This function only rejects values that cannot be a version at all. */
    static bool isVersionUsable (int fileVersion) noexcept;

private:
    PresetSerializer() = delete;
};

} // namespace ripples
