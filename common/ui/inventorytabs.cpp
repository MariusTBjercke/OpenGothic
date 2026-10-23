#include "inventorytabs.h"

#include "game/constants.h"
#include "world/objects/item.h"

InventoryTabs::InventoryTabs() {
  rebuild();
  }

std::string_view InventoryTabs::label(size_t tab) {
  switch(Tab(tab)) {
    case All:      return "All";
    case Weapons:  return "Weapons";
    case Armor:    return "Armor";
    case Potions:  return "Potions";
    case Food:     return "Food";
    case Writings: return "Writings";
    case Magic:    return "Magic";
    case Other:    return "Other";
    case Count:    break;
    }
  return "";
  }

bool InventoryTabs::matches(size_t tab, const Item& item) {
  const uint32_t main      = uint32_t(item.mainFlag());
  constexpr uint32_t weapons = uint32_t(ITM_CAT_NF)|uint32_t(ITM_CAT_FF)|uint32_t(ITM_CAT_MUN);
  constexpr uint32_t armor   = uint32_t(ITM_CAT_ARMOR);
  constexpr uint32_t potion  = uint32_t(ITM_CAT_POTION);
  constexpr uint32_t food    = uint32_t(ITM_CAT_FOOD);
  constexpr uint32_t docs    = uint32_t(ITM_CAT_DOCS);
  constexpr uint32_t magic   = uint32_t(ITM_CAT_RUNE)|uint32_t(ITM_CAT_MAGIC);
  constexpr uint32_t known   = weapons|armor|potion|food|docs|magic;

  switch(Tab(tab)) {
    case All:      return true;
    case Weapons:  return (main & weapons)!=0;
    case Armor:    return (main & armor)!=0;
    case Potions:  return (main & potion)!=0;
    case Food:     return (main & food)!=0;
    case Writings: return (main & docs)!=0;
    case Magic:    return (main & magic)!=0;
    case Other:    return (main & known)==0;
    case Count:    break;
    }
  return true;
  }

void InventoryTabs::setActive(size_t tab) {
  if(tab>=Count || tab==activeId)
    return;
  activeId = tab;
  rebuild();
  }

void InventoryTabs::cycle(int dir) {
  const int cnt = int(Count);
  const int id  = (int(activeId) + cnt + dir%cnt) % cnt;
  setActive(size_t(id));
  }

void InventoryTabs::rebuild() {
  const size_t id = activeId;
  filt = [id](const Item& item) {
    return matches(id, item);
    };
  }
