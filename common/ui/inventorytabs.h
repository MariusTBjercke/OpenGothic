#pragma once

// Fork: category tabs for the inventory, see docs/fork/plan-inventory.md.
// Holds the category list, the active tab and the item filter used by InventoryMenu.

#include <functional>
#include <string_view>

class Item;

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
