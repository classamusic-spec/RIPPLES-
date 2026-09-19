#include "UI/Views/TabBar.h"

#include "UI/Theme/RippleTheme.h"

#include <cmath>

namespace ripples
{

namespace
{
    /** Horizontal padding either side of a tab's text. */
    constexpr int kTabPadding = RippleTheme::xl;

    /** Thickness of the sliding underline. */
    constexpr int kUnderline  = RippleTheme::unit / 2;
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
    underlineValid = false;
    recomputeTabWidths();
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
    underlineValid = false;
    updateTimerState();
}

void TabBar::paint (juce::Graphics& g)
{
    const auto& t = RippleTheme::get();
    auto bounds = getLocalBounds().toFloat();

    // Hairline rule under the whole bar.
    g.setColour (t.panelBorderSoft);
    g.fillRect (bounds.removeFromBottom (t.borderWidth));

    g.setFont (t.sectionFont());

    for (int i = 0; i < tabs.size(); ++i)
    {
        const auto cell = tabBounds (i);

        const auto colour = i == selected ? t.primaryText
                                          : (i == hovered ? t.cyan : t.secondaryText);

        g.setColour (colour);
        g.drawText (tabs[i], cell.toNearestInt(), juce::Justification::centred, false);
    }

    if (underlineValid && underlineW > 0.0f)
    {
        const auto line = juce::Rectangle<float> (underlineX, (float) getHeight() - (float) kUnderline,
                                                  underlineW, (float) kUnderline)
                              .reduced ((float) RippleTheme::md, 0.0f);

        g.setColour (t.cyan.withAlpha (t.glowAmount));
        g.fillRoundedRectangle (line.expanded ((float) RippleTheme::xs, 0.0f), t.smallRadius);

        g.setColour (t.cyanBright);
        g.fillRoundedRectangle (line, t.smallRadius);
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
    const auto target = tabBounds (selected);

    if (! isShowing() || getWidth() <= 0)
    {
        stopTimer();

        // Settle immediately so the bar is correct the moment it reappears.
        underlineX = target.getX();
        underlineW = target.getWidth();
        underlineValid = target.getWidth() > 0.0f;
        return;
    }

    if (! underlineValid)
    {
        underlineX = target.getX();
        underlineW = target.getWidth();
        underlineValid = target.getWidth() > 0.0f;
        repaint();
        return;
    }

    if (std::abs (target.getX() - underlineX) > 0.5f
        || std::abs (target.getWidth() - underlineW) > 0.5f)
        startTimerHz (RippleTheme::get().targetFrameRate);
}

void TabBar::timerCallback()
{
    const auto& t = RippleTheme::get();
    const auto target = tabBounds (selected);

    const float dt = 1.0f / (float) juce::jmax (1, t.targetFrameRate);
    const float k  = 1.0f - std::exp (-dt / juce::jmax (dt, t.hoverFadeSeconds));

    underlineX += (target.getX()     - underlineX) * k;
    underlineW += (target.getWidth() - underlineW) * k;

    if (std::abs (target.getX() - underlineX) < 0.5f
        && std::abs (target.getWidth() - underlineW) < 0.5f)
    {
        underlineX = target.getX();
        underlineW = target.getWidth();
        stopTimer();
    }

    repaint();
}

} // namespace ripples
