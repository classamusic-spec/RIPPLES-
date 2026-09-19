#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

namespace ripples
{

//==============================================================================
/**
    The FX page: the five global water effects, one card each.

      STEREO CURRENT   slow fluid stereo motion
      LIQUID CHORUS    thickening and shimmer
      LIQUID DELAY     moving, diffused repeats
      DIFFUSION        blur and mist
      ABYSS REVERB     the space around everything

    Each card shows its bypass, the three controls that actually get played with
    and a mix, and hides the rest behind MORE. The cards are equal width and
    shrink together, so the page reads as one rack at any size.
*/
class EffectsPage final : public juce::Component
{
public:
    explicit EffectsPage (juce::AudioProcessorValueTreeState& apvts);
    ~EffectsPage() override;

    void resized() override;

private:
    class EffectCard;

    std::vector<std::unique_ptr<EffectCard>> cards;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectsPage)
};

} // namespace ripples
