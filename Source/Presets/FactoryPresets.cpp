#include "Presets/FactoryPresets.h"
#include "Presets/Banks.h"
#include "Presets/PresetAuthoring.h"

/*
    RIPPLES — the factory bank registry.

    The presets themselves live in Presets/Banks/, one translation unit per
    bank. This file only assembles them and answers lookups.
*/

namespace ripples::factory
{

//==============================================================================
//  Registry
//==============================================================================
const std::vector<Preset>& getPresets()
{
    // Built once, on first use, on whichever thread asks first — which is the
    // message thread, because PresetManager is a message-thread object. After
    // this the table is read-only and handed out by const reference.
    static const std::vector<Preset> presets = []
    {
        std::vector<Preset> p;
        p.reserve (420);

        addSurfaceBank (p);
        addShallowBank (p);
        addDeepBlueBank (p);
        addAbyssBank (p);
        addDropletsBank (p);
        addCurrentsBank (p);
        addBioluminescenceBank (p);
        addStormsBank (p);

        return p;
    }();

    return presets;
}

int indexOfPreset (const juce::String& name)
{
    const auto& p = getPresets();

    for (size_t i = 0; i < p.size(); ++i)
        if (p[i].info.name.equalsIgnoreCase (name))
            return (int) i;

    return -1;
}

int getDefaultPresetIndex()
{
    static const int index = juce::jmax (0, indexOfPreset ("SUBMERGED DREAMS"));
    return index;
}

int getInitPresetIndex()
{
    static const int index = juce::jmax (0, indexOfPreset ("INIT DEEP SAW"));
    return index;
}

juce::StringArray getBankNames()
{
    return { kSurface, kShallow, kDeepBlue, kAbyss,
             kDroplets, kCurrents, kBiolum, kStorms };
}

juce::StringArray getCategoryNames()
{
    return { "Pad", "Key", "Pluck", "Bass", "Lead", "Arp", "Texture", "Drone", "FX" };
}

juce::StringArray getTagVocabulary()
{
    return { "Deep", "Wet", "Dark", "Bright", "Dreamy", "Glassy",
             "Organic", "Chaotic", "Calm", "Cinematic", "Submerged", "Surface" };
}

} // namespace ripples::factory

