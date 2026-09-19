#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Presets/PresetManager.h"
#include "UI/Components/GlassPanel.h"
#include "UI/Components/RippleButton.h"
#include "UI/Components/SectionHeader.h"

#include <memory>
#include <vector>

namespace ripples
{

//==============================================================================
/**
    The PRESETS page: search, category, tags and the list itself.

    Deliberately restrained — no cover art, no badges, no colour coding. A preset
    is a name, what it is for, and whether it is a favourite. Clicking a row
    loads it immediately so the bank can be auditioned at speed.
*/
class PresetBrowser final : public GlassPanel,
                            private juce::ListBoxModel
{
public:
    explicit PresetBrowser (PresetManager& presets);
    ~PresetBrowser() override;

    /** Re-reads the manager: call after a load, a rescan or a favourite change. */
    void presetChanged();

    void resized() override;
    void paintOverChildren (juce::Graphics&) override;

private:
    //==========================================================================
    class CategoryModel;

    // juce::ListBoxModel — the preset list.
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height,
                           bool rowIsSelected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void returnKeyPressed (int lastRowSelected) override;
    void selectedRowsChanged (int lastRowSelected) override;

    void refreshList();
    void loadRow (int row);
    void saveCurrentPreset();
    int  presetIndexForRow (int row) const;
    juce::Rectangle<int> favouriteZone (int width, int height) const;

    //==========================================================================
    PresetManager& presetManager;

    juce::TextEditor searchBox;
    RippleButton     clearButton { "CLEAR" }, saveButton { "SAVE" };

    std::vector<std::unique_ptr<RippleButton>> tagChips;
    juce::StringArray tagNames;

    std::unique_ptr<CategoryModel> categoryModel;
    juce::ListBox categoryList;

    SectionHeader listHeader { "PRESETS", "" };
    juce::ListBox presetList;

    juce::Array<int> filtered;
    juce::String     selectedCategory;   // empty means every category

    juce::Rectangle<int> detailArea, categoryLabelArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBrowser)
};

} // namespace ripples
