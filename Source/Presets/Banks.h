#pragma once

/*
    RIPPLES — factory bank registry.

    Each bank lives in its own translation unit under Presets/Banks/ so that the
    eight of them can be written and reviewed independently. getPresets() in
    FactoryPresets.cpp calls these in display order.
*/

#include "Presets/PresetManager.h"

#include <vector>

namespace ripples::factory
{

void addSurfaceBank (std::vector<Preset>& out);
void addShallowBank (std::vector<Preset>& out);
void addDeepBlueBank (std::vector<Preset>& out);
void addAbyssBank (std::vector<Preset>& out);
void addDropletsBank (std::vector<Preset>& out);
void addCurrentsBank (std::vector<Preset>& out);
void addBioluminescenceBank (std::vector<Preset>& out);
void addStormsBank (std::vector<Preset>& out);

} // namespace ripples::factory
