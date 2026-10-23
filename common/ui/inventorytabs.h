#pragma once

// Fork: category tabs for the inventory, see docs/fork/plan-inventory.md.
// Holds the category list, the active tab and the item filter used by InventoryMenu.

#include <Tempest/Rect>
#include <functional>
#include <string_view>

class Item;
namespace Tempest { class Painter; }

class InventoryTabs {
  public:
    enum Tab : uint8_t {
      All = 0,
      Weapons,
      Armor,
      Potions,
      Food,
      Writings,
      Magic,
      Other,
      Count
      };

    using Filter = std::function<bool(const Item&)>;

    InventoryTabs();

    static std::string_view label(size_t tab);
    static bool             matches(size_t tab, const Item& item);

    // Draws the category glyph into a square rect, using the painter's solid primitives only
    // (no art assets). alpha < 1 dims it, which is how empty categories are shown.
    static void paintGlyph(Tempest::Painter& p, size_t tab, const Tempest::Rect& rect, float alpha);

    size_t       active() const { return activeId; }
    void         setActive(size_t tab);
    void         cycle(int dir);

    // Stable address: pages keep the pointer and follow tab changes.
    const Filter* filter() const { return &filt; }

  private:
    void rebuild();

    size_t activeId = All;
    Filter filt;
  };
