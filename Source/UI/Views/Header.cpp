#include "UI/Views/Header.h"

#include "Parameters/ParameterEnums.h"
#include "Parameters/ParameterIDs.h"
#include "UI/Components/RippleKnob.h"
#include "UI/Components/RippleSelector.h"
#include "UI/Components/SectionHeader.h"
#include "UI/Theme/RippleTheme.h"

#include <array>
#include <cmath>
#include <vector>

namespace ripples
{

namespace
{
    const juce::String kLogoText { "R I P P L E S" };
    const juce::String kTagline  { "D I V E   I N T O   S O U N D" };
    const juce::String kOutLabel { "OUT" };

    juce::String u8 (const char* utf8) { return juce::String::fromUTF8 (utf8); }

    //==========================================================================
    // Values the theme has no token for. They live here rather than in
    // RippleTheme.h because that header is shared with work in flight; every
    // colour and every value the theme DOES name is read from RippleTheme::get().
    //==========================================================================

    // Typography.
    constexpr float kWordmarkCompactHeight = 20.0f;   // title size in a narrow header
    constexpr float kPresetNameHeight      = 17.0f;   // preset name in the capsule
    constexpr float kPresetNameMinHeight   = 11.5f;   // never shrinks past this
    constexpr float kPresetNameTracking    = 0.07f;
    constexpr float kLinkTracking          = 0.22f;   // BROWSE / INIT / SETTINGS
    constexpr float kFontFitSlack          = 0.98f;   // shrink-to-fit safety margin

    // Preset capsule.
    constexpr float kCapsuleWidthShare  = 0.34f;                  // of the header width
    constexpr int   kCapsuleMinWidth    = RippleTheme::grid (56); // 224 — below this, drop a link
    constexpr int   kCapsuleFloorWidth  = RippleTheme::grid (36); // 144 — absolute floor
    constexpr int   kCapsuleMaxWidth    = RippleTheme::grid (116);// 464
    constexpr int   kCapsuleMaxHeight   = RippleTheme::grid (13); // 52
    constexpr int   kCapsuleVInset      = RippleTheme::sm;
    constexpr int   kChevronCell        = RippleTheme::grid (8);  // 32
    constexpr int   kTagMinCapsuleWidth = RippleTheme::grid (62); // 248 — below this, no tags
    constexpr int   kTagLineHeight      = RippleTheme::grid (4);  // 16

    // Glyph buttons.
    constexpr int   kGlyphCell        = RippleTheme::grid (6);    // 24
    constexpr float kGlyphInsetRatio  = 0.14f;   // of the cell, around heart / die
    constexpr float kChevronHalfWidth = 0.13f;   // of the cell
    constexpr float kChevronHalfDepth = 0.22f;
    constexpr float kDiceRadiusRatio  = 0.22f;
    constexpr float kDiceDotRatio     = 0.10f;
    constexpr float kDiceDots[]       = { 0.28f, 0.5f, 0.72f };

    // Output cluster.
    constexpr int kScopeWidth        = RippleTheme::grid (23);  // 92
    constexpr int kScopeWidthCompact = RippleTheme::grid (20);  // 80
    constexpr int kScopeHeight       = RippleTheme::grid (7);   // 28
    constexpr int kOutLabelHeight    = RippleTheme::grid (3);   // 12
    constexpr int kBarHitHeight      = RippleTheme::grid (4);   // 16 — drag target
    constexpr int kScopeHistory      = 96;                      // rolling samples

    // Metering. Levels are linear peak; the bar is drawn in decibels.
    constexpr float kMeterFloorDb    = -60.0f;
    constexpr float kMeterDecaySec   = 0.35f;
    constexpr float kPeakDecaySec    = 1.60f;
    constexpr float kMeterFrameShare = 2.0f;   // the header runs at half the animation rate

    // The width below which the interface switches to compact spacing.
    constexpr int kCompactWidth = RippleTheme::grid (280);   // 1120
    constexpr int kMaxLinkLevel = 3;

    //==========================================================================
    float toNormalisedLevel (float linearGain)
    {
        const auto db = juce::Decibels::gainToDecibels (juce::jmax (0.0f, linearGain),
                                                        kMeterFloorDb);
        return juce::jlimit (0.0f, 1.0f, (db - kMeterFloorDb) / -kMeterFloorDb);
    }

    /** A heart in the unit square, tip at the bottom. Built once per resize. */
    juce::Path makeHeartPath()
    {
        juce::Path p;
        p.startNewSubPath (0.50f, 0.96f);
        p.cubicTo (0.10f, 0.62f, 0.01f, 0.33f, 0.17f, 0.17f);
        p.cubicTo (0.32f, 0.02f, 0.45f, 0.11f, 0.50f, 0.26f);
        p.cubicTo (0.55f, 0.11f, 0.68f, 0.02f, 0.83f, 0.17f);
        p.cubicTo (0.99f, 0.33f, 0.90f, 0.62f, 0.50f, 0.96f);
        p.closeSubPath();
        return p;
    }

    std::unique_ptr<RippleKnob> makeKnob (juce::Component& parent,
                                          juce::AudioProcessorValueTreeState& apvts,
                                          const juce::String& label,
                                          const juce::String& paramID,
                                          RippleKnob::Size size,
                                          juce::Colour accent,
                                          const juce::String& tooltip = {})
    {
        auto knob = std::make_unique<RippleKnob> (label, size);
        knob->setAccent (accent);

        if (tooltip.isNotEmpty())
            knob->setTooltipText (tooltip);

        knob->attach (apvts, paramID);
        parent.addAndMakeVisible (*knob);
        return knob;
    }

    void layoutRow (juce::Rectangle<int> area,
                    const std::vector<juce::Component*>& items,
                    int gap)
    {
        const int n = (int) items.size();

        if (n <= 0 || area.isEmpty())
            return;

        const float cellW = (float) (area.getWidth() - gap * (n - 1)) / (float) n;
        float x = (float) area.getX();

        for (auto* c : items)
        {
            const juce::Rectangle<float> cell { x, (float) area.getY(),
                                                cellW, (float) area.getHeight() };
            c->setBounds (cell.toNearestInt());
            x += cellW + (float) gap;
        }
    }
}

//==============================================================================
/** A chevron, a heart or a die, drawn as a path that is rebuilt only on resize. */
class Header::GlyphButton final : public juce::Button
{
public:
    enum class Kind { ChevronLeft, ChevronRight, Heart, Dice };

    GlyphButton (const juce::String& name, Kind k)
        : juce::Button (name), kind (k)
    {
        setWantsKeyboardFocus (false);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    /** Lit state — the heart fills when the preset is a favourite. */
    void setActive (bool shouldBeActive)
    {
        if (active != shouldBeActive)
        {
            active = shouldBeActive;
            repaint();
        }
    }

    void resized() override { rebuildPaths(); }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const auto& t = RippleTheme::get();

        auto colour = active ? t.cyanBright : t.secondaryText;

        if (down)       colour = t.cyan;
        else if (over)  colour = t.cyanBright;

        if (kind == Kind::Heart && active)
        {
            juce::ColourGradient grad (colour, shapeBounds.getCentreX(), shapeBounds.getY(),
                                       t.cyan,  shapeBounds.getCentreX(), shapeBounds.getBottom(),
                                       false);
            g.setGradientFill (grad);
            g.fillPath (shape);
        }
        else if (! shape.isEmpty())
        {
            const float width = (kind == Kind::ChevronLeft || kind == Kind::ChevronRight)
                                    ? t.chevronThickness : t.glyphStrokeWidth;

            g.setColour (colour);
            g.strokePath (shape, juce::PathStrokeType (width,
                                                       juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
        }

        if (! detail.isEmpty())
        {
            g.setColour (colour);
            g.fillPath (detail);
        }
    }

private:
    void rebuildPaths()
    {
        shape.clear();
        detail.clear();

        const auto b = getLocalBounds().toFloat();

        if (b.getWidth() < 2.0f || b.getHeight() < 2.0f)
            return;

        const float side = juce::jmin (b.getWidth(), b.getHeight());
        shapeBounds = juce::Rectangle<float> (side, side).withCentre (b.getCentre());

        const auto inner = shapeBounds.reduced (side * kGlyphInsetRatio);
        const auto centre = shapeBounds.getCentre();

        switch (kind)
        {
            case Kind::ChevronLeft:
            case Kind::ChevronRight:
            {
                const float dx  = side * kChevronHalfWidth;
                const float dy  = side * kChevronHalfDepth;
                const float dir = kind == Kind::ChevronLeft ? 1.0f : -1.0f;

                shape.startNewSubPath (centre.x + dx * dir, centre.y - dy);
                shape.lineTo          (centre.x - dx * dir, centre.y);
                shape.lineTo          (centre.x + dx * dir, centre.y + dy);
                break;
            }

            case Kind::Heart:
            {
                shape = makeHeartPath();
                shape.applyTransform (shape.getTransformToScaleToFit (inner, true));
                break;
            }

            case Kind::Dice:
            {
                shape.addRoundedRectangle (inner, inner.getWidth() * kDiceRadiusRatio);

                const float dotR = juce::jmax (0.6f, inner.getWidth() * kDiceDotRatio);

                for (auto f : kDiceDots)
                    detail.addEllipse (juce::Rectangle<float> (dotR * 2.0f, dotR * 2.0f)
                                           .withCentre ({ inner.getX() + inner.getWidth()  * f,
                                                          inner.getY() + inner.getHeight() * f }));
                break;
            }
        }
    }

    Kind kind;
    bool active = false;
    juce::Path shape, detail;
    juce::Rectangle<float> shapeBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlyphButton)
};

//==============================================================================
/** A plain, dim, letter-spaced text action. No border, no fill. */
class Header::TextLink final : public juce::Button
{
public:
    explicit TextLink (const juce::String& linkText)
        : juce::Button (linkText)
    {
        setButtonText (linkText);
        setWantsKeyboardFocus (false);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    int getIdealWidth() const
    {
        return juce::roundToInt (juce::GlyphArrangement::getStringWidth (font, getButtonText()))
               + RippleTheme::sm;
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const auto& t = RippleTheme::get();

        g.setFont (font);
        g.setColour (down ? t.cyan : (over ? t.cyanBright : t.tertiaryText));
        g.drawText (getButtonText(), getLocalBounds(), juce::Justification::centred, false);

        if (over || down)
        {
            auto rule = getLocalBounds().toFloat();
            rule = rule.removeFromBottom (t.borderWidth).reduced ((float) RippleTheme::xs, 0.0f);
            g.fillRect (rule);
        }
    }

private:
    // Built once with the button, not per paint.
    const juce::Font font { RippleTheme::withTracking (RippleTheme::get().smallFont().getHeight(),
                                                       juce::Font::plain, kLinkTracking) };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TextLink)
};

//==============================================================================
/**
    The output waveform.

    There is no sample history to read, so the scope keeps its own: one peak and
    one RMS value per timer tick, in a fixed ring, drawn as a symmetric envelope.
    It costs two short paths over a 92x28 rectangle, and it stops repainting
    entirely once the whole ring holds the same value — silence is free.
*/
class Header::Scope final : public juce::Component
{
public:
    Scope()
    {
        outer.fill (0.0f);
        inner.fill (0.0f);
        setInterceptsMouseClicks (false, false);
    }

    void push (float peak, float rms)
    {
        const auto epsilon = RippleTheme::get().modRingEpsilon;
        const auto previous = (write + kScopeHistory - 1) % kScopeHistory;

        if (std::abs (peak - outer[(size_t) previous]) > epsilon
            || std::abs (rms - inner[(size_t) previous]) > epsilon)
            settled = 0;
        else
            ++settled;

        outer[(size_t) write] = peak;
        inner[(size_t) write] = rms;
        newest = peak;
        write = (write + 1) % kScopeHistory;

        if (settled < kScopeHistory)
            repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto& t = RippleTheme::get();
        const auto b = getLocalBounds().toFloat();

        if (b.getWidth() < 2.0f || b.getHeight() < 2.0f)
            return;

        const float centreY = b.getCentreY();
        const float halfH   = juce::jmax (1.0f, b.getHeight() * 0.5f - t.traceCoreWidth);
        const float step    = b.getWidth() / (float) (kScopeHistory - 1);

        // Zero line, so the scope still reads as an instrument at silence.
        g.setColour (t.panelBorderSoft);
        g.fillRect (juce::Rectangle<float> (b.getX(), centreY - t.borderWidth * 0.5f,
                                            b.getWidth(), t.borderWidth));

        buildEnvelope (outerPath, outer, b.getX(), centreY, halfH, step);
        buildEnvelope (innerPath, inner, b.getX(), centreY, halfH, step);

        // A waveform overview: the peak envelope as a glowing body, the RMS
        // core bright inside it, and a crisp rim on the peak. Two fills and one
        // hairline stroke over 92x28 pixels.
        g.setColour (t.traceGlow);
        g.fillPath (outerPath);

        g.setColour (t.traceCore);
        g.fillPath (innerPath);

        g.setColour (t.scopeTrace);
        g.strokePath (outerPath, juce::PathStrokeType (t.borderWidth));
    }

private:
    using Ring = std::array<float, (size_t) kScopeHistory>;

    void buildEnvelope (juce::Path& p, const Ring& v,
                        float x0, float centreY, float halfH, float step) const
    {
        p.clear();

        for (int i = 0; i < kScopeHistory; ++i)
        {
            const float x = x0 + step * (float) i;
            const float y = centreY - v[(size_t) ((write + i) % kScopeHistory)] * halfH;

            if (i == 0)
                p.startNewSubPath (x, y);
            else
                p.lineTo (x, y);
        }

        for (int i = kScopeHistory - 1; i >= 0; --i)
        {
            const float x = x0 + step * (float) i;
            p.lineTo (x, centreY + v[(size_t) ((write + i) % kScopeHistory)] * halfH);
        }

        p.closeSubPath();
    }

    Ring outer {}, inner {};
    int   write = 0;
    int   settled = kScopeHistory;
    float newest = 0.0f;

    // Reused across paints; Path::clear() keeps the storage it already has.
    mutable juce::Path outerPath, innerPath;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Scope)
};

//==============================================================================
/**
    The output level, shown as a horizontal bar that is also the output gain
    control: dragging along it sets the parameter, exactly as the old rotary did.

    A LinearBar slider maps the whole width onto the parameter range, so the
    drag position and the drawn bar agree pixel for pixel. Painting is entirely
    ours; the LookAndFeel never draws.
*/
class Header::OutputBar final : public juce::Slider
{
public:
    OutputBar()
    {
        setSliderStyle (juce::Slider::LinearBar);
        setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        setSliderSnapsToMousePosition (true);

        // The rotary this replaced was focusable and named; keep both.
        setWantsKeyboardFocus (true);
        setTitle ("OUTPUT");
        setDescription ("Output level");
    }

    void attach (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID)
    {
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                         (apvts, paramID, *this);
    }

    /** Called from the header's timer with the linear output peak. */
    void setLevel (float linearPeak)
    {
        const auto& t = RippleTheme::get();

        const float dt       = kMeterFrameShare / (float) juce::jmax (1, t.targetFrameRate);
        const float fall     = std::exp (-dt / kMeterDecaySec);
        const float peakFall = std::exp (-dt / kPeakDecaySec);

        const float target  = toNormalisedLevel (linearPeak);
        const float level   = juce::jmax (target, displayLevel * fall);
        const float hold    = juce::jmax (level,  peakHold * peakFall);

        if (std::abs (level - displayLevel) > t.modRingEpsilon
            || std::abs (hold - peakHold) > t.modRingEpsilon)
        {
            displayLevel = level;
            peakHold = hold;
            repaint();
        }
    }

    std::function<void()> onValueChange;

    void paint (juce::Graphics& g) override
    {
        const auto& t = RippleTheme::get();

        auto area = getLocalBounds().toFloat().reduced (t.borderWidth, 0.0f);

        if (area.getWidth() < 4.0f)
            return;

        const auto track = area.withSizeKeepingCentre (area.getWidth(),
                                                       juce::jmin (area.getHeight(),
                                                                   t.meterBarHeight));

        g.setColour (t.meterTrack);
        g.fillRoundedRectangle (track, t.meterBarRadius);

        if (displayLevel > 0.0f)
        {
            // The gradient spans the whole track, so a given level always
            // arrives at the same colour rather than sliding through the ramp.
            juce::ColourGradient grad (t.meterFillLow,  track.getX(),     track.getCentreY(),
                                       t.meterFillHigh, track.getRight(), track.getCentreY(),
                                       false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (track.withWidth (juce::jmax (t.meterBarHeight,
                                                                 track.getWidth() * displayLevel)),
                                    t.meterBarRadius);
        }

        if (peakHold > 0.0f)
        {
            const float x = track.getX() + track.getWidth() * peakHold;
            g.setColour (peakHold >= 1.0f ? t.coral : t.cyanBright);
            g.fillRect (juce::Rectangle<float> (x - t.borderWidth, track.getY(),
                                                t.borderWidth * 2.0f, track.getHeight()));
        }

        // The gain handle: the bar is a control as well as a readout.
        const float gx = track.getX()
                           + track.getWidth() * (float) valueToProportionOfLength (getValue());

        g.setColour (isMouseOverOrDragging() ? t.cyanBright : t.knobIndicator);
        g.fillRoundedRectangle (juce::Rectangle<float> (t.borderWidth * 2.0f,
                                                        track.getHeight() + (float) RippleTheme::xs)
                                    .withCentre ({ gx, track.getCentreY() }),
                                t.borderWidth);

        g.setColour (t.panelBorderSoft);
        g.drawRoundedRectangle (track, t.meterBarRadius, t.borderWidth);

        if (hasKeyboardFocus (false))
        {
            const auto ring = track.expanded (t.focusRingPadding);
            g.setColour (t.focusRing);
            g.drawRoundedRectangle (ring, t.meterBarRadius + t.focusRingPadding, t.focusRingWidth);
        }
    }

    // Slider already repaints on mouse activity; focus is ours to notice.
    void focusGained (juce::Component::FocusChangeType type) override { juce::Slider::focusGained (type); repaint(); }
    void focusLost   (juce::Component::FocusChangeType type) override { juce::Slider::focusLost (type);   repaint(); }

private:
    void valueChanged() override
    {
        repaint();

        if (onValueChange != nullptr)
            onValueChange();
    }

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    float displayLevel = 0.0f, peakHold = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutputBar)
};

//==============================================================================
/** The call-out behind SETTINGS: voice behaviour and the master shaper. */
class Header::SettingsPanel final : public juce::Component
{
public:
    explicit SettingsPanel (juce::AudioProcessorValueTreeState& apvts)
        : voiceHeader ("VOICE", "GLOBAL"),
          masterHeader ("MASTER", "OUTPUT SHAPING")
    {
        const auto& t = RippleTheme::get();

        voiceHeader.setAccent (t.cyan);
        masterHeader.setAccent (t.violet);
        addAndMakeVisible (voiceHeader);
        addAndMakeVisible (masterHeader);

        glideMode.setAccent (t.cyan);
        glideMode.setItems (toStringArray (glideModeNames));
        glideMode.attach (apvts, pid::glideMode);
        addAndMakeVisible (glideMode);

        voices  = makeKnob (*this, apvts, "VOICES", pid::voiceCount, RippleKnob::Size::Small, t.cyan,
                            "Maximum simultaneous voices.");
        bend    = makeKnob (*this, apvts, "BEND",   pid::bendRange,  RippleKnob::Size::Small, t.cyan,
                            "Pitch bend range in semitones.");
        tune    = makeKnob (*this, apvts, "TUNE",   pid::masterTune, RippleKnob::Size::Small, t.cyan,
                            "Master tuning in cents.");
        glide   = makeKnob (*this, apvts, "GLIDE",  pid::glideTime,  RippleKnob::Size::Small, t.cyan,
                            "Portamento time between notes.");

        low     = makeKnob (*this, apvts, "LOW",     pid::mastLow,     RippleKnob::Size::Small, t.violet);
        mid     = makeKnob (*this, apvts, "MID",     pid::mastMid,     RippleKnob::Size::Small, t.violet);
        high    = makeKnob (*this, apvts, "HIGH",    pid::mastHigh,    RippleKnob::Size::Small, t.violet);
        drive   = makeKnob (*this, apvts, "DRIVE",   pid::mastDrive,   RippleKnob::Size::Small, t.violet,
                            "Gentle saturation before the safety limiter.");
        ceiling = makeKnob (*this, apvts, "CEILING", pid::mastCeiling, RippleKnob::Size::Small, t.violet,
                            "Output ceiling the limiter never lets the sound past.");

        setSize (RippleTheme::grid (124), RippleTheme::grid (58));
    }

    void resized() override
    {
        const auto& t = RippleTheme::get();

        auto r = getLocalBounds().reduced (RippleTheme::md);
        const int knobRowH = t.knobSmallDiameter + t.knobLabelHeight + t.knobValueHeight;

        voiceHeader.setBounds (r.removeFromTop (t.sectionHeaderHeight));
        r.removeFromTop (RippleTheme::sm);

        auto voiceRow = r.removeFromTop (knobRowH);
        layoutRow (voiceRow, { voices.get(), bend.get(), tune.get(), glide.get(), &glideMode },
                   RippleTheme::sm);

        // The selector is a short control: centre it in the cell the row gave it.
        glideMode.setBounds (glideMode.getBounds()
                                 .withSizeKeepingCentre (glideMode.getWidth(), t.selectorHeight));

        r.removeFromTop (RippleTheme::md);

        masterHeader.setBounds (r.removeFromTop (t.sectionHeaderHeight));
        r.removeFromTop (RippleTheme::sm);

        layoutRow (r.removeFromTop (knobRowH),
                   { low.get(), mid.get(), high.get(), drive.get(), ceiling.get() },
                   RippleTheme::sm);
    }

private:
    SectionHeader  voiceHeader, masterHeader;
    RippleSelector glideMode;
    std::unique_ptr<RippleKnob> voices, bend, tune, glide;
    std::unique_ptr<RippleKnob> low, mid, high, drive, ceiling;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsPanel)
};

//==============================================================================
Header::Header (juce::AudioProcessorValueTreeState& apvts,
                VisualizationState& vis,
                PresetManager& presets)
    : state (apvts),
      visuals (vis),
      presetManager (presets)
{
    prevButton      = std::make_unique<GlyphButton> ("PREVIOUS PRESET", GlyphButton::Kind::ChevronLeft);
    nextButton      = std::make_unique<GlyphButton> ("NEXT PRESET",     GlyphButton::Kind::ChevronRight);
    favouriteButton = std::make_unique<GlyphButton> ("FAVOURITE",       GlyphButton::Kind::Heart);
    randomButton    = std::make_unique<GlyphButton> ("RANDOMISE",       GlyphButton::Kind::Dice);

    browseLink   = std::make_unique<TextLink> ("BROWSE");
    initLink     = std::make_unique<TextLink> ("INIT");
    settingsLink = std::make_unique<TextLink> ("SETTINGS");

    for (auto* b : { prevButton.get(), nextButton.get(),
                     favouriteButton.get(), randomButton.get() })
        addAndMakeVisible (*b);

    for (auto* b : { browseLink.get(), initLink.get(), settingsLink.get() })
        addAndMakeVisible (*b);

    prevButton->setTooltip ("Previous preset");
    nextButton->setTooltip ("Next preset");
    favouriteButton->setTooltip ("Mark this preset as a favourite");
    randomButton->setTooltip ("Randomise within musical bounds");

    prevButton->onClick = [this] { presetManager.loadPrevious(); };
    nextButton->onClick = [this] { presetManager.loadNext(); };

    favouriteButton->onClick = [this]
    {
        const auto index = presetManager.getCurrentPresetIndex();

        if (presetManager.isValidIndex (index))
            presetManager.setFavourite (index, ! presetManager.isFavourite (index));

        presetChanged();
    };

    randomButton->onClick = [this] { presetManager.randomise(); };
    browseLink->onClick   = [this] { if (onBrowseRequested != nullptr) onBrowseRequested(); };
    initLink->onClick     = [this] { presetManager.loadInit(); };
    settingsLink->onClick = [this] { showSettings(); };

    scope = std::make_unique<Scope>();
    addAndMakeVisible (*scope);

    outputBar = std::make_unique<OutputBar>();
    outputBar->setTooltip ("Final output level. Drag along the bar to set it.");
    addAndMakeVisible (*outputBar);
    outputBar->attach (apvts, pid::mastOutput);

    outputBar->onValueChange = [this]
    {
        outputValueText = outputBar->getTextFromValue (outputBar->getValue());
        repaint (outLabelArea);
    };

    outputValueText = outputBar->getTextFromValue (outputBar->getValue());

    presetChanged();
}

Header::~Header() = default;

//==============================================================================
void Header::presetChanged()
{
    const auto index = presetManager.getCurrentPresetIndex();

    presetName = presetManager.getCurrentPresetName();

    if (presetName.isEmpty())
        presetName = "INIT";

    presetDetail.clear();

    if (presetManager.isValidIndex (index))
    {
        const auto& info = presetManager.getPresetInfo (index);

        juce::StringArray parts;
        parts.add (info.category.toUpperCase());

        for (const auto& tag : info.tags)
            parts.add (tag.toUpperCase());

        presetDetail = parts.joinIntoString (u8 (" \xc2\xb7 "));

        favouriteButton->setActive (presetManager.isFavourite (index));
    }
    else
    {
        favouriteButton->setActive (false);
    }

    presetIsModified = presetManager.isCurrentPresetModified();

    updatePresetFont();
    repaint (capsuleArea);
}

void Header::showSettings()
{
    auto content = std::make_unique<SettingsPanel> (state);

    juce::CallOutBox::launchAsynchronously (std::move (content),
                                            settingsLink->getScreenBounds(),
                                            getTopLevelComponent());
}

//==============================================================================
void Header::updateWordmarkFont (int maxWidth, bool compact)
{
    const auto& t = RippleTheme::get();

    wordmarkFont = compact ? t.titleFont().withHeight (kWordmarkCompactHeight)
                           : t.titleFont();

    const float measured = juce::GlyphArrangement::getStringWidth (wordmarkFont, kLogoText);

    if (measured > (float) maxWidth && measured > 1.0f && maxWidth > 0)
        wordmarkFont = wordmarkFont.withHeight (wordmarkFont.getHeight()
                                                  * (float) maxWidth / measured * kFontFitSlack);
}

void Header::updatePresetFont()
{
    presetFont = RippleTheme::withTracking (kPresetNameHeight, juce::Font::plain,
                                            kPresetNameTracking);

    if (presetNameArea.getWidth() <= 0 || presetName.isEmpty())
        return;

    const float measured = juce::GlyphArrangement::getStringWidth (presetFont, presetName);

    if (measured > (float) presetNameArea.getWidth() && measured > 1.0f)
        presetFont = presetFont.withHeight (
            juce::jmax (kPresetNameMinHeight,
                        presetFont.getHeight() * (float) presetNameArea.getWidth()
                            / measured * kFontFitSlack));
}

//==============================================================================
int Header::measureActions (int linkLevel) const
{
    const int gap = RippleTheme::sm;

    int width = kGlyphCell * 2 + RippleTheme::xs;   // heart + die sit close together

    // The links are dropped from the least important outwards: INIT first,
    // then SETTINGS, and BROWSE last — it is the one that opens the browser.
    if (linkLevel >= 1) width += gap + browseLink->getIdealWidth();
    if (linkLevel >= 3) width += gap + initLink->getIdealWidth();
    if (linkLevel >= 2) width += gap + settingsLink->getIdealWidth();

    return width;
}

void Header::placeActions (juce::Rectangle<int> area, int linkLevel)
{
    const int gap  = RippleTheme::sm;
    const int cell = juce::jmin (kGlyphCell, area.getHeight());

    auto glyphRow = area.removeFromLeft (kGlyphCell * 2 + RippleTheme::xs);
    glyphRow = glyphRow.withSizeKeepingCentre (glyphRow.getWidth(), cell);

    favouriteButton->setBounds (glyphRow.removeFromLeft (kGlyphCell));
    glyphRow.removeFromLeft (RippleTheme::xs);
    randomButton->setBounds (glyphRow.removeFromLeft (kGlyphCell));

    const int linkH = juce::jmin (area.getHeight(), RippleTheme::grid (5));
    area = area.withSizeKeepingCentre (area.getWidth(), linkH);

    const auto place = [&] (TextLink& link, bool visible)
    {
        link.setVisible (visible);

        if (! visible)
            return;

        area.removeFromLeft (gap);
        link.setBounds (area.removeFromLeft (juce::jmin (link.getIdealWidth(),
                                                         juce::jmax (0, area.getWidth()))));
    };

    place (*browseLink,   linkLevel >= 1);
    place (*initLink,     linkLevel >= 3);
    place (*settingsLink, linkLevel >= 2);
}

//==============================================================================
void Header::resized()
{
    auto r = getLocalBounds();

    if (r.isEmpty())
        return;

    const bool compact = getWidth() < kCompactWidth;
    const int  gap     = compact ? RippleTheme::sm : RippleTheme::md;
    const int  centreX = getWidth() / 2;

    // --- Output, far right --------------------------------------------------
    {
        auto outArea = r.removeFromRight (juce::jmin (compact ? kScopeWidthCompact : kScopeWidth,
                                                      r.getWidth() / 3));

        const bool showLabel = outArea.getHeight() >= kScopeHeight + kBarHitHeight
                                                       + kOutLabelHeight + RippleTheme::xs;

        int stackH = kScopeHeight + RippleTheme::xs + kBarHitHeight
                       + (showLabel ? kOutLabelHeight : 0);
        stackH = juce::jmin (stackH, outArea.getHeight());

        auto stack = outArea.withSizeKeepingCentre (outArea.getWidth(), stackH);

        outLabelArea = showLabel ? stack.removeFromTop (kOutLabelHeight)
                                 : juce::Rectangle<int>();

        outputBar->setBounds (stack.removeFromBottom (juce::jmin (kBarHitHeight,
                                                                  stack.getHeight())));
        stack.removeFromBottom (juce::jmin (RippleTheme::xs, stack.getHeight()));
        scope->setBounds (stack);

        r.removeFromRight (gap);
    }

    const int rightEdge = r.getRight();

    // --- Identity, left -----------------------------------------------------
    {
        updateWordmarkFont (juce::jmax (RippleTheme::grid (16), getWidth() / 4), compact);

        const int logoW = juce::roundToInt (juce::GlyphArrangement::getStringWidth (wordmarkFont,
                                                                                    kLogoText))
                            + RippleTheme::xs;

        logoArea = r.removeFromLeft (juce::jmin (logoW, juce::jmax (0, r.getWidth() / 3)));

        const bool showTagline = logoArea.getHeight() >= RippleTheme::grid (15);

        taglineArea = showTagline ? logoArea.removeFromBottom (RippleTheme::grid (4))
                                  : juce::Rectangle<int>();
    }

    // --- Actions and capsule ------------------------------------------------
    // Pick the richest set of text links that still leaves the capsule a
    // civilised width, then centre the capsule on the header's true centre.
    const int leftLimit = logoArea.getRight() + gap;

    const int desiredHalf = juce::jlimit (kCapsuleFloorWidth, kCapsuleMaxWidth,
                                          juce::roundToInt ((float) getWidth() * kCapsuleWidthShare))
                              / 2;

    int chosenLevel = 0, capsuleW = 0, actionsW = 0;

    for (int level = kMaxLinkLevel; level >= 0; --level)
    {
        const int width = measureActions (level);
        const int rightLimit = rightEdge - width - gap;

        const int half = juce::jmin (desiredHalf, juce::jmin (centreX - leftLimit,
                                                              rightLimit - centreX));

        chosenLevel = level;
        actionsW    = width;
        capsuleW    = juce::jmax (0, half * 2);

        if (capsuleW >= kCapsuleMinWidth)
            break;
    }

    capsuleW = juce::jlimit (juce::jmin (kCapsuleFloorWidth, getWidth()),
                             juce::jmax (kCapsuleFloorWidth, getWidth() - RippleTheme::sm),
                             capsuleW);

    const int capsuleH = juce::jmin (getHeight(),
                                     juce::jlimit (RippleTheme::grid (6), kCapsuleMaxHeight,
                                                   getHeight() - kCapsuleVInset * 2));

    capsuleArea = juce::Rectangle<int> (0, 0, capsuleW, capsuleH)
                      .withCentre ({ centreX, getHeight() / 2 });

    // The actions sit in the gap between the capsule and the output. Centring
    // them there keeps a very wide window from opening one huge hole beside the
    // capsule while the actions huddle against the meter.
    const int span    = juce::jmax (0, rightEdge - capsuleArea.getRight());
    const int actionX = juce::jmin (rightEdge - actionsW,
                                    capsuleArea.getRight()
                                        + juce::jmax (gap, (span - actionsW) / 2));

    placeActions (juce::Rectangle<int> (actionX, r.getY(), actionsW, r.getHeight()),
                  chosenLevel);

    // --- Inside the capsule -------------------------------------------------
    {
        auto inside = capsuleArea.reduced (RippleTheme::xs);

        const int chevron = juce::jmin (kChevronCell, juce::jmax (0, inside.getWidth() / 4));

        prevButton->setBounds (inside.removeFromLeft (chevron));
        nextButton->setBounds (inside.removeFromRight (chevron));

        presetTextArea = inside.reduced (RippleTheme::xs, 0);

        showTagLine = capsuleW >= kTagMinCapsuleWidth
                        && presetTextArea.getHeight() >= RippleTheme::grid (9);

        auto text = presetTextArea;
        presetTagArea  = showTagLine ? text.removeFromBottom (kTagLineHeight)
                                     : juce::Rectangle<int>();
        presetNameArea = text;
    }

    updatePresetFont();
}

//==============================================================================
void Header::paint (juce::Graphics& g)
{
    const auto& t = RippleTheme::get();
    const auto clip = g.getClipBounds();

    // --- Wordmark -----------------------------------------------------------
    if (clip.intersects (logoArea) || clip.intersects (taglineArea))
    {
        g.setFont (wordmarkFont);
        g.setColour (t.cyanBright);
        g.drawText (kLogoText, logoArea, juce::Justification::centredLeft, false);

        if (! taglineArea.isEmpty())
        {
            g.setFont (t.smallFont());
            g.setColour (t.tertiaryText);
            g.drawText (kTagline, taglineArea, juce::Justification::topLeft, false);
        }
    }

    // --- Preset capsule -----------------------------------------------------
    if (! capsuleArea.isEmpty() && clip.intersects (capsuleArea))
    {
        const auto capsule = capsuleArea.toFloat();
        const float radius = juce::jmin (t.capsuleRadius, capsule.getHeight() * 0.5f);

        g.setColour (t.capsuleFill);
        g.fillRoundedRectangle (capsule, radius);

        // A hairline of light inside the top edge — glass, not a flat card.
        g.setColour (t.panelHighlight);
        g.drawRoundedRectangle (capsule.reduced (t.borderWidth * 1.5f),
                                juce::jmax (0.0f, radius - t.borderWidth * 1.5f),
                                t.borderWidth);

        g.setColour (t.capsuleBorder);
        g.drawRoundedRectangle (capsule.reduced (t.borderWidth * 0.5f), radius, t.borderWidth);

        const bool withTags = showTagLine && presetDetail.isNotEmpty();

        g.setFont (presetFont);
        g.setColour (t.primaryText);
        g.drawText (presetName, withTags ? presetNameArea : presetTextArea,
                    juce::Justification::centred, true);

        if (withTags)
        {
            g.setFont (t.smallFont());
            g.setColour (t.tertiaryText);
            g.drawText (presetDetail, presetTagArea, juce::Justification::centredTop, true);
        }

        if (presetIsModified)
        {
            const float dot = (float) RippleTheme::unit;
            g.setColour (t.aqua);
            g.fillEllipse ((float) presetTextArea.getRight() - dot * 2.0f,
                           (float) presetTextArea.getY() + dot,
                           dot, dot);
        }
    }

    // --- Output label -------------------------------------------------------
    if (! outLabelArea.isEmpty() && clip.intersects (outLabelArea))
    {
        g.setFont (t.smallFont());
        g.setColour (t.tertiaryText);
        g.drawText (kOutLabel, outLabelArea, juce::Justification::centredLeft, false);

        g.setColour (t.secondaryText);
        g.drawText (outputValueText, outLabelArea, juce::Justification::centredRight, false);
    }
}

//==============================================================================
void Header::mouseDown (const juce::MouseEvent& e)
{
    if (presetTextArea.contains (e.getPosition()) && onBrowseRequested != nullptr)
        onBrowseRequested();
}

void Header::mouseMove (const juce::MouseEvent& e)
{
    const bool overPreset = presetTextArea.contains (e.getPosition());

    if (overPreset != presetHovered)
    {
        presetHovered = overPreset;
        setMouseCursor (overPreset ? juce::MouseCursor::PointingHandCursor
                                   : juce::MouseCursor::NormalCursor);
    }
}

//==============================================================================
void Header::visibilityChanged()      { updateTimerState(); }
void Header::parentHierarchyChanged() { updateTimerState(); }

void Header::updateTimerState()
{
    const auto& t = RippleTheme::get();

    if (isShowing())
        startTimerHz (juce::jmax (1, juce::roundToInt ((float) t.targetFrameRate
                                                       / kMeterFrameShare)));
    else
        stopTimer();
}

void Header::timerCallback()
{
    const float peak = juce::jmax (visuals.getPeakL(), visuals.getPeakR());

    outputBar->setLevel (peak);
    scope->push (toNormalisedLevel (peak), toNormalisedLevel (visuals.getOutputRMS()));

    const auto modified = presetManager.isCurrentPresetModified();

    if (modified != presetIsModified)
    {
        presetIsModified = modified;
        repaint (capsuleArea);
    }
}

} // namespace ripples
