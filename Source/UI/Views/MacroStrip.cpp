#include "UI/Views/MacroStrip.h"

#include "Parameters/ParameterIDs.h"
#include "UI/Theme/RippleTheme.h"

namespace ripples
{

namespace
{
    struct MacroSpec
    {
        const char* label;
        const char* paramID;
        int         accentRole;   // fed to RippleTheme::accentFor
        const char* tooltip;
    };

    const std::array<MacroSpec, 8> kMacros
    {{
        { "DEPTH",    pid::macroDepth,    0,
          "Moves the sound from surface brightness toward submerged darkness." },
        { "WET",      pid::macroWet,      1,
          "Blends the dry instrument into the water around it." },
        { "RIPPLE",   pid::macroRipple,   0,
          "Adds decaying wave-like modulation." },
        { "CURRENT",  pid::macroCurrent,  1,
          "Adds smooth, organic movement." },
        { "DROPS",    pid::macroDrops,    0,
          "Scatters droplet impacts across the stereo field." },
        { "PRESSURE", pid::macroPressure, 2,
          "Adds density, drive and low-mid weight." },
        { "SPACE",    pid::macroSpace,    1,
          "Opens the space around the sound — diffusion, delay and reverb." },
        { "GLOW",     pid::macroGlow,     2,
          "Lifts the bioluminescent shimmer in the upper harmonics." }
    }};

    /** Below this content width the caption is dropped and the knobs take the room. */
    constexpr int kCaptionMinStripWidth = RippleTheme::grid (210);   // 840
    constexpr int kCaptionWidth         = RippleTheme::grid (34);    // 136
}

//==============================================================================
MacroStrip::MacroStrip (juce::AudioProcessorValueTreeState& apvts)
{
    const auto& t = RippleTheme::get();

    setContentInset (RippleTheme::lg);

    for (size_t i = 0; i < kMacros.size(); ++i)
    {
        const auto& spec = kMacros[i];

        auto knob = std::make_unique<RippleKnob> (juce::String (spec.label),
                                                  RippleKnob::Size::Large);
        knob->setAccent (t.accentFor (spec.accentRole));
        knob->setTooltipText (juce::String (spec.tooltip));
        knob->attach (apvts, juce::String (spec.paramID));

        addAndMakeVisible (*knob);
        knobs[i] = std::move (knob);
    }
}

MacroStrip::~MacroStrip() = default;

//==============================================================================
void MacroStrip::resized()
{
    auto content = getContentBounds();

    if (content.isEmpty())
        content = getLocalBounds().reduced (RippleTheme::lg);

    captionArea = {};

    if (content.getWidth() >= kCaptionMinStripWidth)
    {
        captionArea = content.removeFromRight (kCaptionWidth);
        content.removeFromRight (RippleTheme::lg);
    }

    // Equal cells, laid out in floating point so rounding never accumulates.
    const int   gap   = content.getWidth() >= RippleTheme::grid (240) ? RippleTheme::md
                                                                     : RippleTheme::sm;
    const float cellW = (float) (content.getWidth() - gap * (kNumMacros - 1)) / (float) kNumMacros;

    float x = (float) content.getX();

    for (auto& knob : knobs)
    {
        const juce::Rectangle<float> cell { x, (float) content.getY(),
                                            cellW, (float) content.getHeight() };
        knob->setBounds (cell.toNearestInt());
        x += cellW + (float) gap;
    }
}

void MacroStrip::paintOverChildren (juce::Graphics& g)
{
    if (captionArea.isEmpty())
        return;

    const auto& t = RippleTheme::get();

    auto area = captionArea;

    // Hairline that separates the caption from the last macro.
    auto rule = area.removeFromLeft (RippleTheme::sm).toFloat();
    g.setColour (t.panelBorderSoft);
    g.fillRect (rule.withWidth (t.borderWidth).reduced (0.0f, (float) RippleTheme::sm));

    area.removeFromLeft (RippleTheme::sm);

    const juce::StringArray lines { "FLOW", "TRANSFORMS", "SOUND" };

    g.setFont (t.smallFont());

    const int lineHeight = RippleTheme::grid (4);
    auto text = area.withSizeKeepingCentre (area.getWidth(), lineHeight * lines.size());

    for (int i = 0; i < lines.size(); ++i)
    {
        g.setColour (i == 1 ? t.secondaryText : t.tertiaryText);
        g.drawText (lines[i], text.removeFromTop (lineHeight),
                    juce::Justification::centredRight, false);
    }
}

} // namespace ripples
