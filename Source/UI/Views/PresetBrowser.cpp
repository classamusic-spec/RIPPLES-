#include "UI/Views/PresetBrowser.h"

#include "UI/Theme/RippleTheme.h"

#include <cmath>

namespace ripples
{

namespace
{
    constexpr int kBrowserInset = RippleTheme::lg;
    constexpr int kMinTagWidth  = RippleTheme::grid (20);   // 80
    constexpr int kCategoryWide = RippleTheme::grid (32);   // 128
    constexpr int kRowHeight    = RippleTheme::grid (6);    // 24
    constexpr int kStarZone     = RippleTheme::grid (7);    // 28

    const juce::StringArray kCategories { "ALL", "Pad", "Key", "Pluck", "Bass",
                                          "Lead", "Arp", "Texture", "Drone", "FX" };

    const juce::StringArray kTags { "Deep", "Wet", "Dark", "Bright", "Dreamy", "Glassy",
                                    "Organic", "Chaotic", "Calm", "Cinematic",
                                    "Submerged", "Surface" };
}

//==============================================================================
/** The category column. Plain rows, one selected at a time. */
class PresetBrowser::CategoryModel final : public juce::ListBoxModel
{
public:
    explicit CategoryModel (const juce::StringArray& itemsToUse) : items (itemsToUse) {}

    int getNumRows() override { return items.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height,
                           bool rowIsSelected) override
    {
        if (! juce::isPositiveAndBelow (row, items.size()))
            return;

        const auto& t = RippleTheme::get();
        auto bounds = juce::Rectangle<int> (0, 0, width, height);

        if (rowIsSelected)
        {
            g.setColour (t.selectionHighlight);
            g.fillRoundedRectangle (bounds.reduced (RippleTheme::xs / 2).toFloat(),
                                    t.smallRadius);
        }

        g.setColour (rowIsSelected ? t.primaryText : t.secondaryText);
        g.setFont (t.labelFont());
        g.drawText (items[row].toUpperCase(),
                    bounds.reduced (RippleTheme::sm, 0),
                    juce::Justification::centredLeft, false);
    }

    void selectedRowsChanged (int lastRowSelected) override
    {
        if (onSelect != nullptr)
            onSelect (lastRowSelected);
    }

    std::function<void (int)> onSelect;

private:
    juce::StringArray items;
};

//==============================================================================
PresetBrowser::PresetBrowser (PresetManager& presets)
    : presetManager (presets)
{
    const auto& t = RippleTheme::get();

    setContentInset (kBrowserInset);
    setAccent (t.cyan);

    // --- Search -------------------------------------------------------------
    searchBox.setTextToShowWhenEmpty ("SEARCH", t.tertiaryText);
    searchBox.setFont (t.bodyFont());
    searchBox.setColour (juce::TextEditor::backgroundColourId, t.controlFill);
    searchBox.setColour (juce::TextEditor::outlineColourId, t.controlBorder);
    searchBox.setColour (juce::TextEditor::focusedOutlineColourId, t.controlBorderHover);
    searchBox.setColour (juce::TextEditor::textColourId, t.primaryText);
    searchBox.setColour (juce::TextEditor::highlightColourId, t.selectionHighlight);
    searchBox.setColour (juce::TextEditor::highlightedTextColourId, t.primaryText);
    searchBox.onTextChange = [this] { refreshList(); };
    addAndMakeVisible (searchBox);

    clearButton.setAccent (t.cyan);
    clearButton.onClick = [this]
    {
        searchBox.clear();

        for (const auto& chip : tagChips)
            chip->setToggleState (false, juce::dontSendNotification);

        categoryList.selectRow (0);
        selectedCategory.clear();
        refreshList();
    };
    addAndMakeVisible (clearButton);

    saveButton.setAccent (t.cyan);
    saveButton.setIsPrimary (true);
    saveButton.onClick = [this] { saveCurrentPreset(); };
    addAndMakeVisible (saveButton);

    // --- Tag filters --------------------------------------------------------
    tagNames = presets.getTags();

    if (tagNames.isEmpty())
        tagNames = kTags;

    for (const auto& tag : tagNames)
    {
        auto chip = std::make_unique<RippleButton> (tag.toUpperCase());
        chip->setAccent (t.aqua);
        chip->setClickingTogglesState (true);
        chip->setTooltip ("Show only presets tagged " + tag + ".");
        chip->onClick = [this] { refreshList(); };
        addAndMakeVisible (*chip);
        tagChips.push_back (std::move (chip));
    }

    // --- Categories ---------------------------------------------------------
    auto categories = presets.getCategories();
    categories.removeEmptyStrings();

    juce::StringArray categoryItems { "ALL" };

    if (categories.isEmpty())
        categoryItems.addArray (kCategories, 1);
    else
        categoryItems.addArray (categories);

    categoryModel = std::make_unique<CategoryModel> (categoryItems);
    categoryModel->onSelect = [this, categoryItems] (int row)
    {
        selectedCategory = (row <= 0 || row >= categoryItems.size()) ? juce::String()
                                                                    : categoryItems[row];
        refreshList();
    };

    categoryList.setModel (categoryModel.get());
    categoryList.setRowHeight (kRowHeight);
    categoryList.setColour (juce::ListBox::backgroundColourId, t.panelSunken);
    categoryList.setColour (juce::ListBox::outlineColourId, t.panelBorderSoft);
    categoryList.setOutlineThickness (juce::roundToInt (t.borderWidth));
    categoryList.getViewport()->setScrollBarThickness (t.scrollbarWidth);
    addAndMakeVisible (categoryList);
    categoryList.selectRow (0);

    // --- Preset list --------------------------------------------------------
    listHeader.setAccent (t.cyan);
    addAndMakeVisible (listHeader);

    presetList.setModel (this);
    presetList.setRowHeight (kRowHeight);
    presetList.setColour (juce::ListBox::backgroundColourId, t.panelSunken);
    presetList.setColour (juce::ListBox::outlineColourId, t.panelBorderSoft);
    presetList.setOutlineThickness (juce::roundToInt (t.borderWidth));
    presetList.getViewport()->setScrollBarThickness (t.scrollbarWidth);
    addAndMakeVisible (presetList);

    refreshList();
}

PresetBrowser::~PresetBrowser()
{
    presetList.setModel (nullptr);
    categoryList.setModel (nullptr);
}

//==============================================================================
void PresetBrowser::presetChanged()
{
    refreshList();
}

void PresetBrowser::refreshList()
{
    juce::StringArray activeTags;

    for (size_t i = 0; i < tagChips.size(); ++i)
        if (tagChips[i]->getToggleState() && (int) i < tagNames.size())
            activeTags.add (tagNames[(int) i]);

    filtered = presetManager.search (searchBox.getText(), selectedCategory, activeTags);

    listHeader.setSubtitle (juce::String (filtered.size()) + " FOUND");
    presetList.updateContent();

    // Keep the row of the loaded preset selected, without re-loading it.
    const auto current = presetManager.getCurrentPresetIndex();

    for (int row = 0; row < filtered.size(); ++row)
    {
        if (filtered[row] == current)
        {
            presetList.selectRow (row, true, true);
            break;
        }
    }

    presetList.repaint();
    repaint();
}

int PresetBrowser::presetIndexForRow (int row) const
{
    return juce::isPositiveAndBelow (row, filtered.size()) ? filtered[row] : -1;
}

//==============================================================================
int PresetBrowser::getNumRows() { return filtered.size(); }

juce::Rectangle<int> PresetBrowser::favouriteZone (int width, int height) const
{
    return { width - kStarZone, 0, kStarZone, height };
}

void PresetBrowser::paintListBoxItem (int row, juce::Graphics& g, int width, int height,
                                      bool rowIsSelected)
{
    const auto index = presetIndexForRow (row);

    if (index < 0 || ! presetManager.isValidIndex (index))
        return;

    const auto& t = RippleTheme::get();
    const auto& info = presetManager.getPresetInfo (index);

    auto bounds = juce::Rectangle<int> (0, 0, width, height);

    if (rowIsSelected)
    {
        g.setColour (t.selectionHighlight);
        g.fillRoundedRectangle (bounds.reduced (RippleTheme::xs / 2).toFloat(), t.smallRadius);
    }

    // Favourite marker: filled when kept, an outline when not.
    auto star = favouriteZone (width, height);
    const auto dot = star.withSizeKeepingCentre (RippleTheme::sm, RippleTheme::sm).toFloat();

    if (presetManager.isFavourite (index))
    {
        g.setColour (t.aqua);
        g.fillEllipse (dot);
    }
    else
    {
        g.setColour (t.disabledText);
        g.drawEllipse (dot, t.borderWidth);
    }

    bounds.removeFromRight (kStarZone);
    bounds.reduce (RippleTheme::sm, 0);

    // Category on the right, name on the left.
    auto categoryArea = bounds.removeFromRight (juce::jmin (RippleTheme::grid (22),
                                                            bounds.getWidth() / 3));

    g.setFont (t.smallFont());
    g.setColour (t.tertiaryText);
    g.drawText (info.category.toUpperCase(), categoryArea,
                juce::Justification::centredRight, false);

    g.setFont (t.bodyFont());
    g.setColour (rowIsSelected ? t.primaryText : t.secondaryText);
    g.drawText (info.name, bounds, juce::Justification::centredLeft, true);
}

void PresetBrowser::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    const auto index = presetIndexForRow (row);

    if (index < 0)
        return;

    const int rowWidth = presetList.getViewport() != nullptr
                             ? presetList.getViewport()->getViewWidth()
                             : presetList.getWidth();

    if (favouriteZone (rowWidth, kRowHeight).getX() <= e.x)
    {
        presetManager.setFavourite (index, ! presetManager.isFavourite (index));
        presetList.repaint();
        return;
    }

    loadRow (row);
}

void PresetBrowser::returnKeyPressed (int lastRowSelected) { loadRow (lastRowSelected); }

void PresetBrowser::selectedRowsChanged (int) { repaint (detailArea); }

void PresetBrowser::loadRow (int row)
{
    const auto index = presetIndexForRow (row);

    if (index >= 0)
        presetManager.loadPresetAtIndex (index);
}

//==============================================================================
void PresetBrowser::saveCurrentPreset()
{
    auto* window = new juce::AlertWindow ("SAVE PRESET",
                                          "Store the current sound in the user bank.",
                                          juce::MessageBoxIconType::NoIcon,
                                          this);

    window->addTextEditor ("name", presetManager.getCurrentPresetName(), "NAME");
    window->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<PresetBrowser> safeThis (this);

    window->enterModalState (true, juce::ModalCallbackFunction::create (
        [safeThis, window] (int result)
        {
            std::unique_ptr<juce::AlertWindow> owner (window);

            if (result != 1 || safeThis == nullptr)
                return;

            const auto name = owner->getTextEditorContents ("name").trim();

            if (name.isEmpty())
                return;

            const auto current = safeThis->presetManager.getCurrentPresetIndex();

            PresetInfo info;
            info.name = name;

            if (safeThis->presetManager.isValidIndex (current))
            {
                info = safeThis->presetManager.getPresetInfo (current);
                info.name = name;
                info.isFactory = false;
                info.bank = "USER";
                info.filePath.clear();
            }

            safeThis->presetManager.saveUserPreset (name, info);
            safeThis->refreshList();
        }), false);
}

//==============================================================================
void PresetBrowser::resized()
{
    const auto& t = RippleTheme::get();

    auto content = getContentBounds();

    if (content.isEmpty())
        content = getLocalBounds().reduced (kBrowserInset);

    if (content.isEmpty())
        return;

    // --- Search row ---------------------------------------------------------
    auto searchRow = content.removeFromTop (juce::jmin (t.selectorHeight, content.getHeight()));

    const int buttonW = juce::jmin (RippleTheme::grid (14), searchRow.getWidth() / 5);
    saveButton.setBounds (searchRow.removeFromRight (buttonW));
    searchRow.removeFromRight (RippleTheme::xs);
    clearButton.setBounds (searchRow.removeFromRight (buttonW));
    searchRow.removeFromRight (RippleTheme::sm);
    searchBox.setBounds (searchRow);

    content.removeFromTop (RippleTheme::sm);

    // --- Tag chips ----------------------------------------------------------
    const int tagCount = (int) tagChips.size();

    if (tagCount > 0)
    {
        const int perRow = juce::jlimit (1, tagCount,
                                         juce::jmax (1, content.getWidth() / kMinTagWidth));
        const int rows   = (tagCount + perRow - 1) / perRow;
        const int rowH   = t.buttonHeight + RippleTheme::xs;

        auto tagArea = content.removeFromTop (juce::jmin (rows * rowH, content.getHeight()));

        for (int r = 0; r < rows; ++r)
        {
            auto rowArea = tagArea.removeFromTop (rowH);
            const int first = r * perRow;
            const int count = juce::jmin (perRow, tagCount - first);

            const float cellW = (float) (rowArea.getWidth() - RippleTheme::xs * (count - 1))
                                / (float) juce::jmax (1, count);
            float x = (float) rowArea.getX();

            for (int i = 0; i < count; ++i)
            {
                const juce::Rectangle<float> cell { x, (float) rowArea.getY(),
                                                    cellW, (float) rowArea.getHeight() };
                auto b = cell.toNearestInt();
                tagChips[(size_t) (first + i)]
                    ->setBounds (b.withSizeKeepingCentre (b.getWidth(), t.buttonHeight));
                x += cellW + (float) RippleTheme::xs;
            }
        }

        content.removeFromTop (RippleTheme::sm);
    }

    // --- Body: categories | presets ----------------------------------------
    const int categoryW = juce::jlimit (RippleTheme::grid (22), kCategoryWide,
                                        content.getWidth() / 5);

    auto categoryColumn = content.removeFromLeft (categoryW);
    categoryLabelArea = categoryColumn.removeFromTop (juce::jmin (t.sectionHeaderHeight,
                                                                  categoryColumn.getHeight()));
    categoryColumn.removeFromTop (RippleTheme::xs);
    categoryList.setBounds (categoryColumn);

    content.removeFromLeft (RippleTheme::lg);

    listHeader.setBounds (content.removeFromTop (juce::jmin (t.sectionHeaderHeight,
                                                             content.getHeight())));
    content.removeFromTop (RippleTheme::xs);

    detailArea = content.removeFromBottom (juce::jmin (RippleTheme::grid (4),
                                                       content.getHeight() / 4));
    presetList.setBounds (content);
}

void PresetBrowser::paintOverChildren (juce::Graphics& g)
{
    const auto& t = RippleTheme::get();

    if (! categoryLabelArea.isEmpty())
    {
        g.setFont (t.smallFont());
        g.setColour (t.tertiaryText);
        g.drawText ("CATEGORY", categoryLabelArea, juce::Justification::centredLeft, false);
    }

    if (detailArea.isEmpty())
        return;

    const auto index = presetIndexForRow (presetList.getSelectedRow());

    if (index < 0 || ! presetManager.isValidIndex (index))
        return;

    const auto& info = presetManager.getPresetInfo (index);

    juce::StringArray parts;

    if (info.description.isNotEmpty())
        parts.add (info.description);

    if (info.author.isNotEmpty())
        parts.add (info.author);

    g.setFont (t.smallFont());
    g.setColour (t.tertiaryText);
    g.drawText (parts.joinIntoString (juce::String::fromUTF8 ("  \xc2\xb7  ")),
                detailArea, juce::Justification::centredLeft, true);
}

} // namespace ripples
