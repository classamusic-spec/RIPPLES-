#include "Presets/PresetManager.h"
#include "Presets/PresetSerializer.h"
#include "Presets/FactoryPresets.h"
#include "Parameters/ParameterIDs.h"
#include "Parameters/ParameterEnums.h"

#include <algorithm>
#include <cmath>

namespace ripples
{

//==============================================================================
namespace
{
    /** Message-thread guard. Preset loading touches the APVTS, allocates and
        talks to the host; none of it belongs on the audio thread. */
    inline void assertMessageThread()
    {
       #if JUCE_DEBUG
        if (auto* mm = juce::MessageManager::getInstanceWithoutCreating())
            jassert (mm->isThisTheMessageThread());
       #endif
    }

    inline bool sameValue (float a, float b) noexcept
    {
        return std::abs (a - b) < 1.0e-6f;
    }
}

//==============================================================================
PresetManager::PresetManager (juce::AudioProcessorValueTreeState& apvtsToUse)
    : apvts (apvtsToUse)
{
    favouriteKeys = PresetSerializer::loadFavourites();

    rebuildPresetList();

    currentIndex = getDefaultPresetIndex();

    // The plug-in opens on SUBMERGED DREAMS. This happens without host change
    // gestures: at construction time no host is listening yet, and a fresh
    // instance should simply come up sounding like the product. If the host
    // then restores a session, setStateInformation overwrites this, which is
    // exactly right.
    if (isValidIndex (currentIndex))
        applyValues (presets[(size_t) currentIndex].values, false);
}

PresetManager::~PresetManager() = default;

//==============================================================================
void PresetManager::rebuildPresetList()
{
    const auto& fac = factory::getPresets();

    presets.clear();
    presets.reserve (fac.size() + 16);
    presets.insert (presets.end(), fac.begin(), fac.end());

    numFactory = (int) presets.size();

    auto user = PresetSerializer::scanDirectory (PresetSerializer::getUserPresetDirectory());

    for (auto& p : user)
        presets.push_back (std::move (p));
}

void PresetManager::notifyChanged()
{
    if (onPresetChanged != nullptr)
        onPresetChanged();
}

//==============================================================================
bool PresetManager::isGlobalPreference (const juce::String& paramID)
{
    // Master tuning is a studio-wide preference, not a sound-design choice: a
    // preset must never drag the whole instrument out of tune with the session.
    return paramID == pid::masterTune;
}

void PresetManager::applyValues (const std::map<juce::String, float>& values, bool withGestures)
{
    assertMessageThread();

    for (auto* raw : apvts.processor.getParameters())
    {
        auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*> (raw);

        if (withID == nullptr)
            continue;

        const juce::String& id = withID->paramID;

        if (isGlobalPreference (id))
            continue;

        // A parameter the preset does not mention goes back to its default, so
        // that loading preset B can never leave a stray setting from preset A
        // behind. Parameters the preset mentions but this build does not have
        // (a file from a newer version) are simply never visited.
        float target = raw->getDefaultValue();

        if (auto it = values.find (id); it != values.end())
            target = juce::jlimit (0.0f, 1.0f, it->second);

        if (sameValue (raw->getValue(), target))
            continue;

        if (withGestures)
            raw->beginChangeGesture();

        raw->setValueNotifyingHost (target);

        if (withGestures)
            raw->endChangeGesture();
    }
}

void PresetManager::loadPreset (const Preset& preset)
{
    assertMessageThread();

    applyValues (preset.values, true);

    modified = false;
    notifyChanged();
}

void PresetManager::loadPresetAtIndex (int index)
{
    if (! isValidIndex (index))
        return;

    currentIndex = index;
    loadPreset (presets[(size_t) index]);
}

void PresetManager::loadFactoryPreset (int index)
{
    // Factory presets occupy [0, numFactory) of the flat list, so the factory
    // index and the flat index are the same number.
    if (index < 0 || index >= numFactory)
        return;

    loadPresetAtIndex (index);
}

void PresetManager::loadNext()
{
    if (presets.empty())
        return;

    loadPresetAtIndex ((currentIndex + 1) % (int) presets.size());
}

void PresetManager::loadPrevious()
{
    if (presets.empty())
        return;

    const int n = (int) presets.size();
    loadPresetAtIndex ((currentIndex - 1 + n) % n);
}

void PresetManager::loadInit()
{
    loadPresetAtIndex (factory::getInitPresetIndex());
}

void PresetManager::loadDefaultPreset()
{
    loadPresetAtIndex (getDefaultPresetIndex());
}

//==============================================================================
/*  Musical randomisation.

    Randomising every parameter independently gives you white noise with an
    envelope on it. Instead we take a curated factory preset as the structural
    seed — so the voice architecture, the envelope shapes and the effect balance
    are already coherent — and then perturb it.

    Three kinds of move:
      * continuous drift: bounded jitter on the parameters that carry character
        (shape, detune, cutoff, resonance, modulator rates and depths, droplet
        and resonator controls, effect mixes),
      * discrete swaps: waveform / noise colour / filter mode / tide shape are
        re-rolled from aquatic-friendly pools, never from the full enum,
      * hands off: master section, voice count, bend range, glide and the mod
        matrix routing stay as the seed left them, because those are the things
        that make a random patch unusable or dangerously loud.
*/
void PresetManager::randomise()
{
    assertMessageThread();

    if (presets.empty())
        return;

    auto& rnd = juce::Random::getSystemRandom();

    // Seed from a factory preset other than the init patch.
    const int initIndex = factory::getInitPresetIndex();
    int seedIndex = 0;

    if (numFactory > 1)
    {
        for (int attempt = 0; attempt < 8; ++attempt)
        {
            seedIndex = rnd.nextInt (numFactory);

            if (seedIndex != initIndex)
                break;
        }
    }

    Preset p = presets[(size_t) juce::jlimit (0, (int) presets.size() - 1, seedIndex)];

    auto jitter = [&rnd, &p] (const char* id, float amount)
    {
        if (auto it = p.values.find (id); it != p.values.end())
            it->second = juce::jlimit (0.0f, 1.0f,
                                       it->second + (rnd.nextFloat() * 2.0f - 1.0f) * amount);
    };

    auto maybeChoice = [&rnd, &p] (const char* id, float probability,
                                   const std::vector<int>& pool, int numChoices)
    {
        if (pool.empty() || rnd.nextFloat() > probability)
            return;

        const int pick = pool[(size_t) rnd.nextInt ((int) pool.size())];
        p.values[id] = factory::choiceToNormalised (pick, numChoices);
    };

    // --- discrete character -------------------------------------------------
    const std::vector<int> aquaticWaves
    {
        (int) OscWave::Sine, (int) OscWave::Triangle, (int) OscWave::Saw,
        (int) OscWave::Hollow, (int) OscWave::Glass, (int) OscWave::Water
    };

    const std::vector<int> aquaticNoise
    {
        (int) NoiseType::Pink, (int) NoiseType::Deep,
        (int) NoiseType::Surf, (int) NoiseType::Bubble, (int) NoiseType::Air
    };

    const std::vector<int> usableFilters
    {
        (int) FilterMode::LP12, (int) FilterMode::LP24,
        (int) FilterMode::BP12, (int) FilterMode::Morph
    };

    const std::vector<int> tideShapes
    {
        (int) TideShape::Sine, (int) TideShape::Triangle,
        (int) TideShape::Swell, (int) TideShape::Flow
    };

    maybeChoice (pid::oscAWave, 0.55f, aquaticWaves, (int) OscWave::NumWaves);
    maybeChoice (pid::oscBWave, 0.55f, aquaticWaves, (int) OscWave::NumWaves);
    maybeChoice (pid::noiseType, 0.35f, aquaticNoise, (int) NoiseType::NumTypes);
    maybeChoice (pid::filtMode, 0.30f, usableFilters, (int) FilterMode::NumModes);
    maybeChoice (pid::tideShape, 0.45f, tideShapes, (int) TideShape::NumShapes);

    // --- oscillators --------------------------------------------------------
    jitter (pid::oscAShape,   0.30f);
    jitter (pid::oscADetune,  0.20f);
    jitter (pid::oscAStereo,  0.20f);
    jitter (pid::oscALevel,   0.12f);
    jitter (pid::oscBShape,   0.30f);
    jitter (pid::oscBDetune,  0.20f);
    jitter (pid::oscBStereo,  0.20f);
    jitter (pid::oscBLevel,   0.18f);
    jitter (pid::oscBInterAmt, 0.18f);
    jitter (pid::subLevel,    0.15f);
    jitter (pid::noiseLevel,  0.12f);
    jitter (pid::noiseTone,   0.25f);

    // --- filter and envelopes ----------------------------------------------
    jitter (pid::filtCutoff,   0.14f);
    jitter (pid::filtReso,     0.15f);
    jitter (pid::filtDrive,    0.12f);
    jitter (pid::filtMovement, 0.25f);
    jitter (pid::filtPressure, 0.20f);
    jitter (pid::filtEnvAmt,   0.12f);

    jitter (pid::fenvAttack,  0.10f);
    jitter (pid::fenvDecay,   0.14f);
    jitter (pid::fenvSustain, 0.18f);
    jitter (pid::fenvRelease, 0.12f);

    jitter (pid::aenvAttack,  0.08f);
    jitter (pid::aenvDecay,   0.12f);
    jitter (pid::aenvSustain, 0.12f);
    jitter (pid::aenvRelease, 0.12f);

    // --- motion -------------------------------------------------------------
    jitter (pid::tideRate,    0.18f);
    jitter (pid::tideDepth,   0.20f);
    jitter (pid::tideStereo,  0.25f);
    jitter (pid::currRate,    0.20f);
    jitter (pid::currAmount,  0.20f);
    jitter (pid::currSmooth,  0.20f);
    jitter (pid::currDrift,   0.20f);
    jitter (pid::driftRate,   0.20f);
    jitter (pid::driftAmount, 0.20f);
    jitter (pid::rippleRate,  0.20f);
    jitter (pid::rippleDecay, 0.20f);
    jitter (pid::rippleDepth, 0.20f);

    // --- water ---------------------------------------------------------------
    jitter (pid::dropAmount,  0.22f);
    jitter (pid::dropDensity, 0.25f);
    jitter (pid::dropSize,    0.25f);
    jitter (pid::dropTone,    0.25f);
    jitter (pid::dropSplash,  0.25f);
    jitter (pid::dropGravity, 0.25f);
    jitter (pid::dropBounce,  0.25f);
    jitter (pid::resoAmount,  0.22f);
    jitter (pid::resoSize,    0.25f);
    jitter (pid::resoDecay,   0.20f);
    jitter (pid::resoDamping, 0.25f);
    jitter (pid::resoScatter, 0.25f);
    jitter (pid::resoMotion,  0.20f);

    // --- macros and field ----------------------------------------------------
    jitter (pid::macroDepth,    0.20f);
    jitter (pid::macroWet,      0.20f);
    jitter (pid::macroRipple,   0.20f);
    jitter (pid::macroCurrent,  0.20f);
    jitter (pid::macroDrops,    0.20f);
    jitter (pid::macroPressure, 0.20f);
    jitter (pid::macroSpace,    0.20f);
    jitter (pid::macroGlow,     0.20f);
    jitter (pid::fluidX,        0.25f);
    jitter (pid::fluidY,        0.25f);

    // --- effect balance only, never the enables ------------------------------
    jitter (pid::chorMix,  0.12f);
    jitter (pid::dlyMix,   0.12f);
    jitter (pid::dlyFeedback, 0.10f);
    jitter (pid::diffMix,  0.12f);
    jitter (pid::verbMix,  0.12f);
    jitter (pid::verbSize, 0.15f);

    p.info.name       = "RANDOM";
    p.info.bank       = "USER";
    p.info.isFactory  = false;
    p.info.filePath   = {};

    loadPreset (p);

    // A randomised patch is not any stored preset any more.
    currentIndex = -1;
    modified = true;
    notifyChanged();
}

//==============================================================================
int PresetManager::getNumPresets() const          { return (int) presets.size(); }
int PresetManager::getNumFactoryPresets() const   { return numFactory; }
int PresetManager::getNumUserPresets() const      { return (int) presets.size() - numFactory; }

bool PresetManager::isValidIndex (int index) const
{
    return index >= 0 && index < (int) presets.size();
}

bool PresetManager::isUserPreset (int index) const
{
    return index >= numFactory && index < (int) presets.size();
}

const PresetInfo& PresetManager::getPresetInfo (int index) const
{
    static const PresetInfo empty {};
    return isValidIndex (index) ? presets[(size_t) index].info : empty;
}

const Preset& PresetManager::getPreset (int index) const
{
    static const Preset empty {};
    return isValidIndex (index) ? presets[(size_t) index] : empty;
}

int PresetManager::getCurrentPresetIndex() const { return currentIndex; }

juce::String PresetManager::getCurrentPresetName() const
{
    if (! isValidIndex (currentIndex))
        return "RANDOM";

    return presets[(size_t) currentIndex].info.name;
}

bool PresetManager::isCurrentPresetModified() const { return modified; }
void PresetManager::markModified()                  { modified = true; }

int PresetManager::getDefaultPresetIndex() const
{
    const int i = factory::getDefaultPresetIndex();
    return isValidIndex (i) ? i : 0;
}

int PresetManager::indexOfPreset (const juce::String& name) const
{
    for (size_t i = 0; i < presets.size(); ++i)
        if (presets[i].info.name.equalsIgnoreCase (name))
            return (int) i;

    return -1;
}

//==============================================================================
juce::File PresetManager::getUserPresetDirectory() const
{
    return PresetSerializer::getUserPresetDirectory();
}

Preset PresetManager::captureCurrentState (const PresetInfo& info) const
{
    Preset p;
    p.info = info;
    p.info.isFactory = false;
    p.info.parameterVersion = pid::kParameterVersion;

    for (auto* raw : apvts.processor.getParameters())
        if (auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*> (raw))
            p.values[withID->paramID] = juce::jlimit (0.0f, 1.0f, raw->getValue());

    return p;
}

bool PresetManager::saveUserPreset (const juce::String& name, const PresetInfo& info)
{
    assertMessageThread();

    const auto cleanName = PresetSerializer::sanitiseName (name.isNotEmpty() ? name : info.name);

    if (cleanName.isEmpty())
        return false;

    PresetInfo toSave = info;
    toSave.name = cleanName;
    toSave.isFactory = false;

    if (toSave.bank.isEmpty() || toSave.bank.equalsIgnoreCase ("FACTORY"))
        toSave.bank = "USER";

    if (toSave.category.isEmpty())
        toSave.category = "Pad";

    if (toSave.author.isEmpty())
        toSave.author = "User";

    toSave.tags.trim();
    toSave.tags.removeEmptyStrings();
    toSave.tags.removeDuplicates (true);

    auto preset = captureCurrentState (toSave);
    auto file   = PresetSerializer::fileForPreset (cleanName);

    if (! PresetSerializer::saveToFile (preset, file))
        return false;

    rescanUserPresets();

    // Land on the preset we just wrote.
    for (int i = numFactory; i < (int) presets.size(); ++i)
    {
        if (presets[(size_t) i].info.name.equalsIgnoreCase (cleanName))
        {
            currentIndex = i;
            break;
        }
    }

    modified = false;
    notifyChanged();
    return true;
}

void PresetManager::deleteUserPreset (int index)
{
    assertMessageThread();

    if (! isUserPreset (index))
        return;   // factory presets live in the binary and cannot be deleted

    const auto info = presets[(size_t) index].info;

    if (info.filePath.isNotEmpty())
    {
        juce::File file (info.filePath);

        if (file.existsAsFile())
            file.deleteFile();
    }

    favouriteKeys.removeString (info.getKey());
    PresetSerializer::saveFavourites (favouriteKeys);

    rescanUserPresets();

    if (! isValidIndex (currentIndex))
        currentIndex = getDefaultPresetIndex();

    notifyChanged();
}

void PresetManager::rescanUserPresets()
{
    assertMessageThread();

    // Keep pointing at the same preset across the rescan where we can.
    const juce::String previousKey = isValidIndex (currentIndex)
                                       ? presets[(size_t) currentIndex].info.getKey()
                                       : juce::String();

    rebuildPresetList();

    currentIndex = -1;

    if (previousKey.isNotEmpty())
    {
        for (size_t i = 0; i < presets.size(); ++i)
        {
            if (presets[i].info.getKey() == previousKey)
            {
                currentIndex = (int) i;
                break;
            }
        }
    }

    if (currentIndex < 0)
        currentIndex = getDefaultPresetIndex();

    notifyChanged();
}

//==============================================================================
juce::StringArray PresetManager::getCategories() const
{
    juce::StringArray result;

    // Canonical display order first, but only the ones actually in use.
    for (const auto& c : factory::getCategoryNames())
        for (const auto& p : presets)
            if (p.info.category.equalsIgnoreCase (c))
            {
                result.addIfNotAlreadyThere (c);
                break;
            }

    // Then anything a user preset invented.
    for (const auto& p : presets)
        if (p.info.category.isNotEmpty() && ! result.contains (p.info.category, true))
            result.add (p.info.category);

    return result;
}

juce::StringArray PresetManager::getTags() const
{
    juce::StringArray result;

    for (const auto& t : factory::getTagVocabulary())
        for (const auto& p : presets)
            if (p.info.tags.contains (t, true))
            {
                result.addIfNotAlreadyThere (t);
                break;
            }

    for (const auto& p : presets)
        for (const auto& t : p.info.tags)
            if (t.isNotEmpty() && ! result.contains (t, true))
                result.add (t);

    return result;
}

juce::StringArray PresetManager::getBanks() const
{
    juce::StringArray result;

    for (const auto& b : factory::getBankNames())
        result.addIfNotAlreadyThere (b);

    for (const auto& p : presets)
        if (p.info.bank.isNotEmpty() && ! result.contains (p.info.bank, true))
            result.add (p.info.bank);

    return result;
}

juce::Array<int> PresetManager::search (const juce::String& query,
                                        const juce::String& category,
                                        const juce::StringArray& tags) const
{
    juce::Array<int> result;
    const auto q = query.trim();

    for (int i = 0; i < (int) presets.size(); ++i)
    {
        const auto& info = presets[(size_t) i].info;

        if (category.isNotEmpty() && ! info.category.equalsIgnoreCase (category))
            continue;

        bool hasAllTags = true;

        for (const auto& t : tags)
        {
            if (t.trim().isEmpty())
                continue;

            if (! info.tags.contains (t, true))
            {
                hasAllTags = false;
                break;
            }
        }

        if (! hasAllTags)
            continue;

        if (q.isNotEmpty())
        {
            const bool hit = info.name.containsIgnoreCase (q)
                          || info.author.containsIgnoreCase (q)
                          || info.bank.containsIgnoreCase (q)
                          || info.category.containsIgnoreCase (q)
                          || info.description.containsIgnoreCase (q)
                          || info.tags.joinIntoString (" ").containsIgnoreCase (q);

            if (! hit)
                continue;
        }

        result.add (i);
    }

    return result;
}

juce::Array<int> PresetManager::getPresetsInBank (const juce::String& bank) const
{
    juce::Array<int> result;

    for (int i = 0; i < (int) presets.size(); ++i)
        if (presets[(size_t) i].info.bank.equalsIgnoreCase (bank))
            result.add (i);

    return result;
}

//==============================================================================
void PresetManager::setFavourite (int index, bool shouldBeFavourite)
{
    if (! isValidIndex (index))
        return;

    const auto key = presets[(size_t) index].info.getKey();

    if (shouldBeFavourite)
        favouriteKeys.addIfNotAlreadyThere (key);
    else
        favouriteKeys.removeString (key);

    PresetSerializer::saveFavourites (favouriteKeys);
    notifyChanged();
}

bool PresetManager::isFavourite (int index) const
{
    if (! isValidIndex (index))
        return false;

    return favouriteKeys.contains (presets[(size_t) index].info.getKey(), true);
}

juce::Array<int> PresetManager::getFavourites() const
{
    juce::Array<int> result;

    for (int i = 0; i < (int) presets.size(); ++i)
        if (isFavourite (i))
            result.add (i);

    return result;
}

} // namespace ripples
