#include "Presets/PresetSerializer.h"
#include "Parameters/ParameterIDs.h"

#include <algorithm>
#include <cmath>

namespace ripples
{

//==============================================================================
namespace
{
    constexpr const char* kAttrName        = "name";
    constexpr const char* kAttrAuthor      = "author";
    constexpr const char* kAttrCategory    = "category";
    constexpr const char* kAttrBank        = "bank";
    constexpr const char* kAttrDescription = "description";
    constexpr const char* kAttrTags        = "tags";
    constexpr const char* kAttrDryAquatic  = "dryAquatic";
    constexpr const char* kAttrVersion     = "parameterVersion";
    constexpr const char* kAttrPluginVer   = "pluginVersion";
    constexpr const char* kAttrId          = "id";
    constexpr const char* kAttrValue       = "value";
    constexpr const char* kAttrKey         = "key";

    bool isFinite (double v) noexcept
    {
        return ! (std::isnan (v) || std::isinf (v));
    }
}

//==============================================================================
juce::File PresetSerializer::getUserPresetDirectory()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("RIPPLES")
                   .getChildFile ("Presets");

    if (! dir.isDirectory())
        dir.createDirectory();   // failure here is tolerated; callers re-check

    return dir;
}

juce::String PresetSerializer::sanitiseName (const juce::String& name)
{
    auto cleaned = name.trim().removeCharacters ("\\/:*?\"<>|\r\n\t");

    // Collapse runs of whitespace so "A   B" and "A B" cannot fight over a file.
    while (cleaned.contains ("  "))
        cleaned = cleaned.replace ("  ", " ");

    if (cleaned.isEmpty())
        cleaned = "Untitled";

    return cleaned.substring (0, 96);
}

juce::File PresetSerializer::fileForPreset (const juce::String& presetName)
{
    return getUserPresetDirectory().getChildFile (sanitiseName (presetName) + kFileExtension);
}

bool PresetSerializer::isVersionUsable (int fileVersion) noexcept
{
    // Anything that could plausibly be a version number. Values outside this
    // range mean the attribute was garbage, not that the preset is too new.
    return fileVersion > 0 && fileVersion < 100000;
}

//==============================================================================
std::unique_ptr<juce::XmlElement> PresetSerializer::toXml (const Preset& preset)
{
    auto root = std::make_unique<juce::XmlElement> (kRootTag);

    root->setAttribute (kAttrVersion, pid::kParameterVersion);

   #if defined (JucePlugin_VersionString)
    root->setAttribute (kAttrPluginVer, JucePlugin_VersionString);
   #endif

    auto* info = root->createNewChildElement (kInfoTag);
    info->setAttribute (kAttrName, preset.info.name);
    info->setAttribute (kAttrAuthor, preset.info.author);
    info->setAttribute (kAttrCategory, preset.info.category);
    info->setAttribute (kAttrBank, preset.info.bank);
    info->setAttribute (kAttrDescription, preset.info.description);
    info->setAttribute (kAttrTags, preset.info.tags.joinIntoString (","));
    info->setAttribute (kAttrDryAquatic, preset.info.dryAquatic ? 1 : 0);

    auto* params = root->createNewChildElement (kParametersTag);

    for (const auto& [id, value] : preset.values)
    {
        if (id.isEmpty() || ! isFinite ((double) value))
            continue;

        auto* p = params->createNewChildElement (kParamTag);
        p->setAttribute (kAttrId, id);
        p->setAttribute (kAttrValue, (double) juce::jlimit (0.0f, 1.0f, value));
    }

    return root;
}

bool PresetSerializer::fromXml (const juce::XmlElement& xml, Preset& result)
{
    if (! xml.hasTagName (kRootTag))
        return false;

    Preset loaded;

    const int fileVersion = xml.getIntAttribute (kAttrVersion, pid::kParameterVersion);
    loaded.info.parameterVersion = isVersionUsable (fileVersion) ? fileVersion
                                                                 : pid::kParameterVersion;

    if (auto* info = xml.getChildByName (kInfoTag))
    {
        loaded.info.name        = info->getStringAttribute (kAttrName).trim();
        loaded.info.author      = info->getStringAttribute (kAttrAuthor, "Unknown").trim();
        loaded.info.category    = info->getStringAttribute (kAttrCategory, "Pad").trim();
        loaded.info.bank        = info->getStringAttribute (kAttrBank, "USER").trim();
        loaded.info.description = info->getStringAttribute (kAttrDescription).trim();
        loaded.info.dryAquatic  = info->getIntAttribute (kAttrDryAquatic, 0) != 0;

        loaded.info.tags.addTokens (info->getStringAttribute (kAttrTags), ",", "");
        loaded.info.tags.trim();
        loaded.info.tags.removeEmptyStrings();
        loaded.info.tags.removeDuplicates (true);
    }

    if (loaded.info.name.isEmpty())
        loaded.info.name = "Untitled";
    if (loaded.info.category.isEmpty())
        loaded.info.category = "Pad";
    if (loaded.info.bank.isEmpty())
        loaded.info.bank = "USER";

    loaded.info.isFactory = false;

    int count = 0;

    if (auto* params = xml.getChildByName (kParametersTag))
    {
        for (auto* p : params->getChildWithTagNameIterator (kParamTag))
        {
            if (++count > kMaxParametersPerFile)
                break;

            const auto id = p->getStringAttribute (kAttrId).trim();

            if (id.isEmpty() || ! p->hasAttribute (kAttrValue))
                continue;

            const double v = p->getDoubleAttribute (kAttrValue, std::numeric_limits<double>::quiet_NaN());

            if (! isFinite (v))
                continue;

            loaded.values[id] = juce::jlimit (0.0f, 1.0f, (float) v);
        }
    }

    if (loaded.values.empty())
        return false;   // a preset with no parameters is not a preset

    result = std::move (loaded);
    return true;
}

//==============================================================================
bool PresetSerializer::saveToFile (const Preset& preset, const juce::File& file)
{
    if (file.getFullPathName().isEmpty() || preset.values.empty())
        return false;

    auto parent = file.getParentDirectory();

    if (! parent.isDirectory() && ! parent.createDirectory().wasOk())
        return false;

    auto xml = toXml (preset);

    if (xml == nullptr)
        return false;

    juce::TemporaryFile temp (file);

    if (! xml->writeTo (temp.getFile(), {}))
        return false;

    return temp.overwriteTargetFileWithTemporary();
}

bool PresetSerializer::loadFromFile (const juce::File& file, Preset& result)
{
    if (! file.existsAsFile() || file.getSize() <= 0)
        return false;

    // Refuse anything absurd before we hand it to the XML parser.
    if (file.getSize() > 4 * 1024 * 1024)
        return false;

    auto xml = juce::XmlDocument::parse (file);

    if (xml == nullptr)
        return false;   // missing, truncated or not XML at all

    if (! fromXml (*xml, result))
        return false;

    if (result.info.name.isEmpty() || result.info.name == "Untitled")
        result.info.name = file.getFileNameWithoutExtension();

    result.info.filePath = file.getFullPathName();

    return true;
}

std::vector<Preset> PresetSerializer::scanDirectory (const juce::File& directory)
{
    std::vector<Preset> found;

    if (! directory.isDirectory())
        return found;

    juce::Array<juce::File> files;
    directory.findChildFiles (files, juce::File::findFiles, true,
                              juce::String ("*") + kFileExtension);

    found.reserve ((size_t) files.size());

    for (const auto& f : files)
    {
        Preset p;

        if (! loadFromFile (f, p))
            continue;   // corrupt or foreign file: skip it, never crash

        // A preset filed in a sub-folder takes that folder as its bank, which
        // lets users organise the directory however they like.
        if (p.info.bank.isEmpty() || p.info.bank == "USER")
        {
            auto parent = f.getParentDirectory();

            if (parent != directory)
                p.info.bank = parent.getFileName().toUpperCase();
        }

        p.info.isFactory = false;
        found.push_back (std::move (p));
    }

    std::sort (found.begin(), found.end(),
               [] (const Preset& a, const Preset& b)
               {
                   return a.info.name.compareIgnoreCase (b.info.name) < 0;
               });

    return found;
}

//==============================================================================
juce::StringArray PresetSerializer::loadFavourites()
{
    juce::StringArray keys;

    auto file = getUserPresetDirectory().getChildFile (kFavouritesFile);

    if (! file.existsAsFile() || file.getSize() <= 0 || file.getSize() > 1024 * 1024)
        return keys;

    auto xml = juce::XmlDocument::parse (file);

    if (xml == nullptr || ! xml->hasTagName (kFavouritesTag))
        return keys;

    for (auto* e : xml->getChildWithTagNameIterator (kFavouriteTag))
    {
        const auto key = e->getStringAttribute (kAttrKey).trim();

        if (key.isNotEmpty())
            keys.addIfNotAlreadyThere (key);
    }

    return keys;
}

bool PresetSerializer::saveFavourites (const juce::StringArray& keys)
{
    auto dir = getUserPresetDirectory();

    if (! dir.isDirectory())
        return false;

    juce::XmlElement xml (kFavouritesTag);

    for (const auto& key : keys)
    {
        if (key.trim().isEmpty())
            continue;

        xml.createNewChildElement (kFavouriteTag)->setAttribute (kAttrKey, key);
    }

    return xml.writeTo (dir.getChildFile (kFavouritesFile), {});
}

} // namespace ripples
