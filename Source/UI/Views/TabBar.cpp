#include "UI/Views/TabBar.h"

#include "UI/Theme/RippleTheme.h"

#include <cmath>

namespace ripples
{

namespace
{
    /** Horizontal padding either side of a tab's text. */
    constexpr int kTabPadding = RippleTheme::xl;

    /** How far the pill is inset from its tab cell, so the pill hugs the text
        rather than filling the whole hit area. */
    constexpr int kPillInsetX = RippleTheme::md;

    /** Vertical breathing room above and below the pill, in total. */
    constexpr int kPillInsetY = RippleTheme::sm;

    /** The pill never grows past this, however tall the bar is asked to be. */
    constexpr int kPillMaxHeight = RippleTheme::grid (6);
}

//==============================================================================
TabBar::TabBar()
{
    setInterceptsMouseClicks (true, false);
    setWantsKeyboardFocus (false);
}

TabBar::~TabBar() = default;

//==============================================================================
void TabBar::setTabs (const juce::StringArray& names)
{
    tabs = names;
    selected = juce::jlimit (0, juce::jmax (0, tabs.size() - 1), selected);
    pillValid = false;
    recomputeTabWidths();
    updateTimerState();
    repaint();
}

void TabBar::setSelectedTab (int index, bool sendNotification)
{
    index = juce::jlimit (0, juce::jmax (0, tabs.size() - 1), index);

    if (index == selected)
        return;

    selected = index;
    updateTimerState();
    repaint();

    if (sendNotification && onTabChanged != nullptr)
        onTabChanged (selected);
}

int TabBar::getPreferredHeight() const
{
    return RippleTheme::grid (8);
}

//==============================================================================
void TabBar::recomputeTabWidths()
{
    const auto& t = RippleTheme::get();
    const auto font = t.sectionFont();

    widths.clearQuick();

    for (const auto& name : tabs)
        widths.add (juce::GlyphArrangement::getStringWidth (font, name)
                    + (float) (kTabPadding * 2));
}

juce::Rectangle<float> TabBar::tabBounds (int index) const
{
    if (! juce::isPositiveAndBelow (index, widths.size()))
        return {};

    float x = 0.0f;

    for (int i = 0; i < index; ++i)
        x += widths[i];

    return { x, 0.0f, widths[index], (float) getHeight() };
}

juce::Rectangle<float> TabBar::pillTargetFor (int index) const
{
    const auto cell = tabBounds (index);

    if (cell.getWidth() <= (float) (kPillInsetX * 2))
        return cell;

    return cell.reduced ((float) kPillInsetX, 0.0f);
}

juce::Rectangle<float> TabBar::pillArea (juce::Rectangle<float> strip) const
{
    const auto h = juce::jmax (1.0f, juce::jmin (strip.getHeight() - (float) kPillInsetY,
                                                 (float) kPillMaxHeight));

    return { pillX, strip.getCentreY() - h * 0.5f, pillW, h };
}

int TabBar::tabAt (juce::Point<int> p) const
{
    for (int i = 0; i < tabs.size(); ++i)
        if (tabBounds (i).contains (p.toFloat()))
            return i;

    return -1;
}

//==============================================================================
void TabBar::resized()
{
    recomputeTabWidths();
    pillValid = false;
    updateTimerState();
}

void TabBar::paint (juce::Graphics& g)
{
    const auto& t = RippleTheme::get();
    auto bounds = getLocalBounds().toFloat();

    // Hairline rule under the whole bar.
    g.setColour (t.panelBorderSoft);
    g.fillRect (bounds.removeFromBottom (t.borderWidth));

    // The selected page is a filled pill. It is a SHAPE, not a tint, so the
    // selection survives being read without colour at all.
    const auto pill = pillArea (bounds);
    const auto showPill = pillValid && pill.getWidth() > 1.0f;

    if (showPill)
    {
        const auto radius = juce::jmin (t.pillRadius, pill.getHeight() * 0.5f);

        g.setColour (t.pillFillActive);
        g.fillRoundedRectangle (pill, radius);

        g.setColour (t.pillBorder);
        g.drawRoundedRectangle (pill.reduced (t.borderWidth * 0.5f), radius, t.borderWidth);
    }

    g.setFont (t.sectionFont());

    for (int i = 0; i < tabs.size(); ++i)
    {
        const auto cell = tabBounds (i);

        // While the pill glides, each label lights up in proportion to how much
        // of the pill has actually reached it, so nothing snaps mid-flight.
        const auto covered = showPill
                               ? juce::jlimit (0.0f, 1.0f,
                                               pill.getHorizontalRange()
                                                   .getIntersectionWith (cell.getHorizontalRange())
                                                   .getLength() / juce::jmax (1.0f, pill.getWidth()))
                               : 0.0f;

        // Everything not under the pill is plain dim tracked text: no rule, no
        // tick, no underline.
        const auto resting = (i == hovered && i != selected) ? t.cyan : t.secondaryText;

        g.setColour (resting.interpolatedWith (t.pillText, covered));
        g.drawText (tabs[i], cell.toNearestInt(), juce::Justification::centred, false);
    }
}

//==============================================================================
void TabBar::mouseDown (const juce::MouseEvent& e)
{
    const auto index = tabAt (e.getPosition());

    if (index >= 0)
        setSelectedTab (index, true);
}

void TabBar::mouseMove (const juce::MouseEvent& e)
{
    const auto index = tabAt (e.getPosition());

    if (index != hovered)
    {
        hovered = index;
        setMouseCursor (index >= 0 ? juce::MouseCursor::PointingHandCursor
                                   : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void TabBar::mouseExit (const juce::MouseEvent&)
{
    if (hovered != -1)
    {
        hovered = -1;
        repaint();
    }
}

//==============================================================================
void TabBar::visibilityChanged()       { updateTimerState(); }
void TabBar::parentHierarchyChanged()  { updateTimerState(); }

void TabBar::updateTimerState()
{
    const auto target = pillTargetFor (selected);

    if (! isShowing() || getWidth() <= 0)
    {
        stopTimer();

        // Settle immediately so the bar is correct the moment it reappears.
        pillX = target.getX();
        pillW = target.getWidth();
        pillValid = target.getWidth() > 0.0f;
        return;
    }

    if (! pillValid)
    {
        pillX = target.getX();
        pillW = target.getWidth();
        pillValid = target.getWidth() > 0.0f;
        repaint();
        return;
    }

    if (std::abs (target.getX() - pillX) > 0.5f
        || std::abs (target.getWidth() - pillW) > 0.5f)
        startTimerHz (RippleTheme::get().targetFrameRate);
}

void TabBar::timerCallback()
{
    const auto& t = RippleTheme::get();
    const auto target = pillTargetFor (selected);

    const float dt = 1.0f / (float) juce::jmax (1, t.targetFrameRate);
    const float k  = 1.0f - std::exp (-dt / juce::jmax (dt, t.hoverFadeSeconds));

    pillX += (target.getX()     - pillX) * k;
    pillW += (target.getWidth() - pillW) * k;

    if (std::abs (target.getX() - pillX) < 0.5f
        && std::abs (target.getWidth() - pillW) < 0.5f)
    {
        pillX = target.getX();
        pillW = target.getWidth();
        stopTimer();
    }

    repaint();
}

} // namespace ripples
