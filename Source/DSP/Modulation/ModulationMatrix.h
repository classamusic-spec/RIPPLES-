#pragma once

#include "Parameters/ParameterEnums.h"
#include "Parameters/ParameterIDs.h"

#include <cstdint>

namespace ripples
{

/** Current value of every modulation source, indexed by (int) ModSource.
    Unipolar sources (Velocity, ModWheel, envelopes) sit in 0..1;
    bipolar sources (Tide, Current, Drift, Ripple) sit in -1..1. */
struct ModSourceValues
{
    float values[(size_t) ModSource::NumSources] {};

    float operator[] (ModSource s) const noexcept { return values[(size_t) s]; }
    void set (ModSource s, float v) noexcept { values[(size_t) s] = v; }
};

//==============================================================================
/** The polarity convention the matrix assumes when it is filling in a slot's
    `bipolar` remap. The four modulators are bipolar by construction; KeyTrack
    and the two FLUID FIELD axes are centred controls, so they are bipolar too.
    Everything else — the envelopes, velocity, note, wheels, aftertouch and the
    per-note random — arrives in 0..1 and is remapped only when the slot asks. */
inline constexpr bool isBipolarModSource (ModSource s) noexcept
{
    switch (s)
    {
        case ModSource::Tide:
        case ModSource::Current:
        case ModSource::Drift:
        case ModSource::Ripple:
        case ModSource::KeyTrack:
        case ModSource::FluidFieldX:
        case ModSource::FluidFieldY:
            return true;

        default:
            return false;
    }
}

//==============================================================================
/**
    The modulation matrix: twelve source -> destination slots with a bipolar
    amount each.

    process() runs once per control block per voice, so the slot list is kept
    pre-compacted: editing a slot rebuilds a small array of the ones that are
    actually doing something, and the audio path then walks that array instead
    of testing twelve slots or sweeping the destination table. Several slots
    aimed at the same destination simply sum.
*/
class ModulationMatrix
{
public:
    static constexpr int kNumSlots = pid::kNumModSlots;

    struct Slot
    {
        ModSource source = ModSource::None;
        ModDest   dest   = ModDest::None;
        float     amount = 0.0f;    // -1..1
        bool      bipolar = false;  // treat a unipolar source as -1..1
    };

    void setSlot (int index, const Slot& slot) noexcept;   // index 0..kNumModSlots-1
    Slot getSlot (int index) const noexcept;
    void clear() noexcept;

    /** Accumulates every active slot into destOffsets, which must have
        (size_t) ModDest::NumDests elements. Clears it first. */
    void process (const ModSourceValues& sources, float* destOffsets) const noexcept;

    /** How many slots currently contribute anything — for the UI and for tests. */
    int getNumActiveSlots() const noexcept { return activeCount; }

private:
    void rebuildActive() noexcept;

    struct Entry
    {
        float   amount = 0.0f;
        uint8_t source = 0;
        uint8_t dest   = 0;
        bool    remap  = false;   // unipolar source, slot asked for bipolar
    };

    Slot  slots[kNumSlots] {};
    Entry active[kNumSlots] {};
    int   activeCount = 0;
};

} // namespace ripples
