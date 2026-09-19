#pragma once

#include <atomic>
#include <array>
#include <cstdint>

namespace ripples
{

/**
    Lock-free bridge from the audio thread to the editor.

    The audio thread only ever *stores* to these atomics (relaxed ordering is
    fine — a torn read one frame late is invisible at 60fps). The GUI only ever
    *loads*. Nothing here allocates, blocks, or touches a juce::Component.

    Note events use a small ring buffer so the UI can spawn one expanding ripple
    per note rather than missing fast passages.
*/
class VisualizationState
{
public:
    //==========================================================================
    // Continuous levels — written once per audio block.
    void setOutputPeak (float l, float r) noexcept
    {
        peakL.store (l, std::memory_order_relaxed);
        peakR.store (r, std::memory_order_relaxed);
    }

    void setOutputRMS (float rms) noexcept { outRMS.store (rms, std::memory_order_relaxed); }

    float getPeakL() const noexcept { return peakL.load (std::memory_order_relaxed); }
    float getPeakR() const noexcept { return peakR.load (std::memory_order_relaxed); }
    float getOutputRMS() const noexcept { return outRMS.load (std::memory_order_relaxed); }

    //==========================================================================
    // Modulator values, normalised. Drive the animated indicators.
    void setTideValue    (float v) noexcept { tide.store (v, std::memory_order_relaxed); }
    void setCurrentValue (float v) noexcept { current.store (v, std::memory_order_relaxed); }
    void setDriftValue   (float v) noexcept { drift.store (v, std::memory_order_relaxed); }
    void setRippleValue  (float v) noexcept { ripple.store (v, std::memory_order_relaxed); }
    void setAmpEnvValue  (float v) noexcept { ampEnv.store (v, std::memory_order_relaxed); }
    void setModEnvValue  (float v) noexcept { modEnv.store (v, std::memory_order_relaxed); }

    float getTideValue()    const noexcept { return tide.load (std::memory_order_relaxed); }
    float getCurrentValue() const noexcept { return current.load (std::memory_order_relaxed); }
    float getDriftValue()   const noexcept { return drift.load (std::memory_order_relaxed); }
    float getRippleValue()  const noexcept { return ripple.load (std::memory_order_relaxed); }
    float getAmpEnvValue()  const noexcept { return ampEnv.load (std::memory_order_relaxed); }
    float getModEnvValue()  const noexcept { return modEnv.load (std::memory_order_relaxed); }

    //==========================================================================
    // Voice / filter state for the displays.
    void setActiveVoices (int n) noexcept { activeVoices.store (n, std::memory_order_relaxed); }
    int  getActiveVoices() const noexcept { return activeVoices.load (std::memory_order_relaxed); }

    void setFilterCutoffHz (float hz) noexcept { cutoffHz.store (hz, std::memory_order_relaxed); }
    float getFilterCutoffHz() const noexcept { return cutoffHz.load (std::memory_order_relaxed); }

    void setFilterResonance (float r) noexcept { filterReso.store (r, std::memory_order_relaxed); }
    float getFilterResonance() const noexcept { return filterReso.load (std::memory_order_relaxed); }

    //==========================================================================
    // Note events — a tiny lock-free ring. The UI drains it each frame.
    struct NoteEvent
    {
        float velocity = 0.0f;   // 0..1
        float pitch    = 0.0f;   // normalised 0..1 across the keyboard
        bool  isNoteOn = true;
    };

    static constexpr int kEventCapacity = 32;

    void pushNoteEvent (float velocity, float normalisedPitch, bool noteOn) noexcept
    {
        const auto w = writeIndex.load (std::memory_order_relaxed);
        events[(size_t) (w & (kEventCapacity - 1))] = { velocity, normalisedPitch, noteOn };
        writeIndex.store (w + 1, std::memory_order_release);
    }

    /** Drains up to kEventCapacity pending events into dest; returns how many. */
    int drainNoteEvents (NoteEvent* dest, int maxEvents) noexcept
    {
        const auto w = writeIndex.load (std::memory_order_acquire);
        auto r = readIndex;

        // If we fell more than a buffer behind, skip ahead rather than replay stale events.
        if (w - r > (uint32_t) kEventCapacity)
            r = w - (uint32_t) kEventCapacity;

        int count = 0;
        while (r != w && count < maxEvents)
        {
            dest[count++] = events[(size_t) (r & (kEventCapacity - 1))];
            ++r;
        }
        readIndex = r;
        return count;
    }

    //==========================================================================
    // Droplet events — a simple monotonically increasing counter plus intensity.
    void pushDropletEvent (float intensity, float pan) noexcept
    {
        dropletCount.fetch_add (1, std::memory_order_relaxed);
        lastDropletIntensity.store (intensity, std::memory_order_relaxed);
        lastDropletPan.store (pan, std::memory_order_relaxed);
    }

    uint32_t getDropletCount() const noexcept { return dropletCount.load (std::memory_order_relaxed); }
    float getLastDropletIntensity() const noexcept { return lastDropletIntensity.load (std::memory_order_relaxed); }
    float getLastDropletPan() const noexcept { return lastDropletPan.load (std::memory_order_relaxed); }

    //==========================================================================
    // Macro-derived visual mood, published so the UI need not re-read the APVTS.
    void setDepth (float v)    noexcept { depth.store (v, std::memory_order_relaxed); }
    void setGlow (float v)     noexcept { glow.store (v, std::memory_order_relaxed); }
    void setMotion (float v)   noexcept { motion.store (v, std::memory_order_relaxed); }

    float getDepth()  const noexcept { return depth.load (std::memory_order_relaxed); }
    float getGlow()   const noexcept { return glow.load (std::memory_order_relaxed); }
    float getMotion() const noexcept { return motion.load (std::memory_order_relaxed); }

private:
    std::atomic<float> peakL { 0.0f }, peakR { 0.0f }, outRMS { 0.0f };
    std::atomic<float> tide { 0.0f }, current { 0.0f }, drift { 0.0f }, ripple { 0.0f };
    std::atomic<float> ampEnv { 0.0f }, modEnv { 0.0f };
    std::atomic<float> cutoffHz { 1000.0f }, filterReso { 0.0f };
    std::atomic<float> depth { 0.5f }, glow { 0.3f }, motion { 0.3f };
    std::atomic<int>   activeVoices { 0 };

    std::array<NoteEvent, kEventCapacity> events {};
    std::atomic<uint32_t> writeIndex { 0 };
    uint32_t readIndex = 0;

    std::atomic<uint32_t> dropletCount { 0 };
    std::atomic<float> lastDropletIntensity { 0.0f }, lastDropletPan { 0.0f };

    static_assert ((kEventCapacity & (kEventCapacity - 1)) == 0,
                   "kEventCapacity must be a power of two for the mask to work");
};

} // namespace ripples
