#pragma once

#include <Tempest/Widget>
#include <Tempest/Texture2d>
#include <Tempest/Timer>

#include "graphics/inventoryrenderer.h"
#include "game/inventory.h"
#include "ui/inventorytabs.h"
#include "ui/uidraw.h"

class Npc;
class Item;
class Inventory;
class Interactive;
class World;
class KeyCodec;
namespace Tempest { class Window; }

class InventoryMenu : public Tempest::Widget {
  public:
    InventoryMenu(const KeyCodec& key);
    ~InventoryMenu();

    enum class State:uint8_t {
      Closed=0,
      Equip,
      Chest,
      Trade,
      Ransack,
      LockPicking
      };

    enum class LootMode:uint8_t {
      Normal=0,
      Stack,
      Ten,
      Hundred
      };

    enum class DrawPass:uint8_t {
      Back,
      Front
      };

    void  close();
    void  open(Npc& pl);
    void  trade(Npc& pl,Npc& tr);
    bool  ransack(Npc& pl,Npc& tr);
    void  open(Npc& pl,Interactive& chest);
    State isOpen() const;
    bool  isActive() const;
    bool  wantsCursor() const; // fork: true while the window drives the mouse cursor itself
    void  onWorldChanged();

    void  tick(uint64_t dt);
    void  draw(Tempest::Encoder<Tempest::CommandBuffer>& cmd);
    void  paintNumOverlay(Tempest::PaintEvent& e);

    void  keyDownEvent  (Tempest::KeyEvent&   e) override;
    void  keyRepeatEvent(Tempest::KeyEvent&   e) override;
    void  keyUpEvent    (Tempest::KeyEvent&   e) override;

  protected:
    void  paintEvent     (Tempest::PaintEvent& e) override;

    void  mouseDownEvent (Tempest::MouseEvent& event) override;
    void  mouseUpEvent   (Tempest::MouseEvent& event) override;
    void  mouseMoveEvent (Tempest::MouseEvent& event) override;
    void  mouseDragEvent (Tempest::MouseEvent& event) override;
    void  mouseWheelEvent(Tempest::MouseEvent& event) override;
    void  mouseDoubleClickEvent(Tempest::MouseEvent& event) override;
    void  mouseEnterEvent(Tempest::MouseEvent& event) override;
    void  mouseLeaveEvent(Tempest::MouseEvent& event) override;

  private:
    struct Page;
    struct InvPage;
    struct TradePage;
    struct RansackPage;

    struct PageLocal final {
      size_t                  sel    = 0;
      size_t                  scroll = 0;
      };

    // fork: tab bar geometry. The cells are contiguous and derived from the strip rect, so a click
    // anywhere in the bar maps to a tab (per-cell hit records used to leave dead pixels in the gaps
    // and could go stale between paints).
    struct TabBar final {
      Tempest::Rect           strip;     // whole bar, including its padding
      Tempest::Rect           band;      // area covered by the tab cells
      int                     cellW = 0; // width of one cell
      int                     extra = 0; // first `extra` cells are one pixel wider
      };
    TabBar                    tabBar;

    struct ActionArea final {
      Tempest::Rect           rect;
      bool                    valid = false;
      };

    const KeyCodec&           keycodec;

    const Tempest::Texture2d* tex =nullptr;
    const Tempest::Texture2d* slot=nullptr;
    const Tempest::Texture2d* selT=nullptr;
    const Tempest::Texture2d* selU=nullptr;

    State                     state      =State::Closed;
    Npc*                      player     =nullptr;
    Npc*                      trader     =nullptr;
    Interactive*              chest      =nullptr;

    std::unique_ptr<Page>     pageOth, pagePl;
    PageLocal                 pageLocal[2];

    uint8_t                   page       =0;
    Tempest::Timer            takeTimer;
    size_t                    takeCount  =0;
    LootMode                  lootMode   =LootMode::Normal;
    InventoryRenderer         renderer;

    size_t                    columsCount = 5;
    int32_t                   scrollDelta = 0;

    // fork: mouse control ([GAME] invMouse), see docs/fork/plan-inventory.md
    struct GridArea final {
      Tempest::Rect           rect;
      uint8_t                 side   = 0; // index in pageLocal: 0 - chest/trader, 1 - player
      size_t                  wcount = 0;
      size_t                  hcount = 0;
      };
    GridArea                  grids[2];
    size_t                    gridsCount  = 0;

    // fork: category tabs ([GAME] invTabs) and take-all button
    InventoryTabs             tabs;
    int8_t                    tabHover      = -1;
    PageLocal                 tabLocal[InventoryTabs::Count];
    ActionArea                takeAll;
    bool                      takeAllHover  = false;

    // fork: drag & drop and cursor tooltip, see docs/fork/plan-ui-overhaul.md
    struct DragState final {
      bool            armed    = false; // pressed on a slot, not yet a drag
      bool            active   = false; // moved past the threshold with the button held
      bool            repeated = false; // the hold-repeat timer already acted on this slot
      uint8_t         side     = 1;     // source side (0 - other, 1 - player)
      size_t          clsId    = 0;
      size_t          count    = 0;
      size_t          index    = 0;
      Tempest::Point  start;
      Tempest::Point  pos;
      int             target   = -1;    // grid under the cursor: side index, -1 = none
      };

    DragState                 drag;
    uint8_t                   lastClickSide  = 2;   // 2 = no click yet
    size_t                    lastClickIndex = 0;
    Tempest::Point            mousePos;
    bool                      mouseTracked = false;

    // layout helpers, all scale aware
    size_t                    rowsCount() const;
    int                       gridTop() const;
    int                       padd() const;
    int                       panelPad() const;
    int                       headerH() const;

    Tempest::Size             slotSize() const;
    int                       infoHeight() const;
    size_t                    pagesCount() const;

    const Page&               activePage();
    PageLocal&                activePageSel();

    void          processMove(Tempest::KeyEvent& e);
    void          moveLeft(bool usePage);
    void          moveRight(bool usePage);
    void          moveUp();
    void          moveDown();

    void          onItemAction(uint8_t slotHint);
    void          onTakeStuff();
    void          onHoldTick();
    void          adjustScroll();
    void          drawAll   (Tempest::Painter& p, Npc& player, DrawPass pass);
    void          drawItems (Tempest::Painter& p, DrawPass pass, const Page &inv, const PageLocal &sel,
                             uint8_t side, int x, int y, int wcount, int hcount);
    void          drawSlot  (Tempest::Painter& p, DrawPass pass, const Inventory::Iterator& it,
                             const Page& page, const PageLocal &sel, int x, int y, size_t id);
    void          drawGold  (Tempest::Painter& p, int x, int y, int w);
    void          drawInfo  (Tempest::Painter& p);
    void          drawPanel (Tempest::Painter& p, const Tempest::Rect& panel, const Tempest::Rect& header,
                             bool playerSide, std::string_view title);
    void          drawTabs  (Tempest::Painter& p, const Tempest::Rect& strip, const Page& page);
    void          drawTakeAll(Tempest::Painter& p, const Tempest::Rect& header);
    void          drawTooltip(Tempest::Painter& p);
    void          drawHints (Tempest::Painter& p);
    void          drawDragGhost(Tempest::Painter& p);
    std::string_view hints() const;

    bool            isMouseMode() const;
    bool            isTabsMode() const;
    bool            isScrimMode() const;
    bool            isTooltipMode() const;
    auto            inventoryFilter() const -> const Inventory::ItemFilter*;
    uint8_t         activeSide() const;
    const Page*     sidePage(uint8_t side) const;
    const GridArea* gridAt(const Tempest::Point& pos) const;
    bool            selectAt(const Tempest::Point& pos);
    void            scrollGrid(uint8_t side, int rows);
    size_t          slotIdAt(const GridArea& g, const Tempest::Point& pos) const;

    int8_t          tabAt(const Tempest::Point& pos) const;
    void            switchTab(size_t tab);
    void            cycleTab(int dir);
    void            takeAllItems();

    void            performClick(const Tempest::Point& pos);
    void            cancelDrag();
    void            resolveDrop(uint8_t srcSide, size_t clsId, int dstSide);
    void            performDoubleClick(const Tempest::Point& pos);

    void             updateCursor();
  };
