#include "DSP/Modulation/ModulationMatrix.h"

#include "Utilities/MathUtils.h"

#include <cmath>

namespace ripples
{

namespace
{
    constexpr int   kNumDests     = (int) ModDest::NumDests;
    constexpr int   kNumSources   = (int) ModSource::NumSources;
    constexpr float kAmountFloor  = 1.0e-6f;

    inline bool sourceInRange (ModSource s) noexcept
    {
        return (int) s > 0 && (int) s < kNumSources;
    }

    inline bool destInRange (ModDest d) noexcept
    {
        return (int) d > 0 && (int) d < kNumDests;
    }
}

//==============================================================================
void ModulationMatrix::setSlot (int index, const Slot& slot) noexcept
{
    if (index < 0 || index >= kNumSlots)
        return;

    Slot s = slot;
    s.amount = math::clamp (math::sanitise (s.amount), -1.0f, 1.0f);

    if (! sourceInRange (s.source)) s.source = ModSource::None;
    if (! destInRange (s.dest))     s.dest   = ModDest::None;

    slots[index] = s;
    rebuildActive();
}

ModulationMatrix::Slot ModulationMatrix::getSlot (int index) const noexcept
{
    if (index < 0 || index >= kNumSlots)
        return {};

    return slots[index];
}

void ModulationMatrix::clear() noexcept
{
    for (auto& s : slots)
        s = {};

    activeCount = 0;
}

void ModulationMatrix::rebuildActive() noexcept
{
    activeCount = 0;

    for (const auto& s : slots)
    {
        if (s.source == ModSource::None || s.dest == ModDest::None)
            continue;

        if (std::fabs (s.amount) < kAmountFloor)
            continue;

        Entry& e = active[activeCount++];
        e.amount = s.amount;
        e.source = (uint8_t) s.source;
        e.dest   = (uint8_t) s.dest;
        e.remap  = s.bipolar && ! isBipolarModSource (s.source);
    }
}

//==============================================================================
void ModulationMatrix::process (const ModSourceValues& sources, float* destOffsets) const noexcept
{
    if (destOffsets == nullptr)
        return;

    for (int i = 0; i < kNumDests; ++i)
        destOffsets[i] = 0.0f;

    for (int i = 0; i < activeCount; ++i)
    {
        const Entry& e = active[i];

        float v = sources.values[e.source];

        if (e.remap)
            v = v * 2.0f - 1.0f;

        // One source misbehaving must not be able to poison the whole voice.
        v = math::clamp (math::sanitise (v), -1.0f, 1.0f);

        destOffsets[e.dest] += v * e.amount;
    }
}

} // namespace ripples
