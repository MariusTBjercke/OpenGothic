#pragma once

// Fork: category tabs for the inventory, see docs/fork/plan-inventory.md and
// docs/fork/plan-ui-overhaul.md. Holds the category list, the active tab and the item filter used by
// InventoryMenu.
//
// Categories come from the item flags (ITM_CAT_*), with the instance-name prefix as a fallback for the
// families that Gothic does not encode in flags (trophies, keys). The classification is cached per
// class id, because resolving the instance name goes through the Daedalus symbol table.

#include <Tempest/Rect>
#include <functional>
#include <string_view>
#include <unordered_map>

class Item;
class Inventory;
namespace Tempest { class Painter; }

class InventoryTabs {
  public:
    enum Tab : uint8_t {
      All = 0,
      Melee,
      Ranged,
      Armor,
      Potions,
      Food,
      Magic,
      Writings,
      Trophies,
      Keys,
      Other,
      Count
      };

    using Filter = std::function<bool(const Item&)>;

    InventoryTabs();

    static std::string_view label(size_t tab);

    // Primary category of an item: a tab id, never All.
    uint8_t     category(const Item& item) const;

    // Tabs that contain at least one item of `inv` (All always does). Used to dim empty tabs.
    void        categories(const Inventory& inv, bool out[Count]) const;

    // Draws the category glyph into a square rect, using the painter's solid primitives only
    // (no art assets). alpha < 1 dims it, which is how empty categories are shown.
    static void paintGlyph(Tempest::Painter& p, size_t tab, const Tempest::Rect& rect, float alpha);

    size_t      active() const { return activeId; }
    void        setActive(size_t tab);
    void        cycle(int dir);

    // Drops the class-id cache: the Daedalus symbol table changes with the world/mod.
    void        resetCache();

    // Stable address: pages keep the pointer and follow tab changes.
    const Filter* filter() const { return &filt; }

  private:
    void        rebuild();
    std::string_view instanceName(const Item& item) const;

    size_t activeId = All;
    Filter filt;
    mutable std::unordered_map<size_t,uint8_t> catCache;
  };
