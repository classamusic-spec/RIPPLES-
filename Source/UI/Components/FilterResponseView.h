#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Parameters/ParameterEnums.h"
#include "UI/Theme/RippleTheme.h"

#include <complex>

namespace ripples
{

//==============================================================================
/**
    The DEPTH filter's magnitude response.

    The curve is genuinely computed, not sketched: the two-pole modes are
    evaluated from the analogue state-variable prototype and LP24 from the
    four-pole ladder prototype, so a 24 dB/oct slope really is twice as steep as
    a 12 dB/oct one and the resonant peak really does grow with the resonance
    parameter.

    Log frequency axis (20 Hz .. 20 kHz, the same mapping the DSP uses), dB
    magnitude axis, faint grid, a soft cyan fill beneath the curve and a quiet
    marker at the cutoff.

    There is no timer: the paths are rebuilt only when the filter settings or the
    bounds change, so an untouched display costs nothing at all.
*/
class FilterResponseView final : public juce::Component
{
public:
    FilterResponseView();
    ~FilterResponseView() override = default;

    //==========================================================================
    // Contract API.
    void setFilter (FilterMode mode, float cutoffHz, float resonance);
    void setAccent (juce::Colour accent);

    //==========================================================================
    /** Extra helper: the morph position used when the mode is FilterMode::Morph.
        0 = low pass, 0.5 = band pass, 1 = high pass. Defaults to the centre. */
    void setMorphPosition (float position01);

    //==========================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    //==========================================================================
    void rebuildPaths();

    /** Complex response of the current filter at a frequency, in Hz. */
    std::complex<float> responseAt (float freqHz) const noexcept;

    float xForFrequency (float hz) const noexcept;
    float yForDecibels (float dB) const noexcept;

    //==========================================================================
    FilterMode mode      = FilterMode::LP24;
    float      cutoffHz  = 1000.0f;
    float      resonance = 0.2f;
    float      morphPos  = 0.5f;

    juce::Colour accentColour = RippleTheme::get().cyan;

    // Cached geometry. The three stroked ribbons that make up the luminous
    // trace are built alongside the curve, so a frame is the wash plus three
    // fills of cached geometry — nothing is stroked or allocated in paint().
    juce::Path             curve, fill;
    juce::Path             coreStroke, glowStroke, bloomStroke;
    juce::Path             wellClip;
    juce::Rectangle<float> plotBounds, labelBounds;
    bool                   showAxisLabels = false;
    float                  cutoffX = 0.0f, cutoffY = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilterResponseView)
};

} // namespace ripples
