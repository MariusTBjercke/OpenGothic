#include "inventorymenu.h"

#include <Tempest/Log>
#include <Tempest/Painter>
#include <Tempest/SoundEffect>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "utils/string_frm.h"
#include "world/objects/npc.h"
#include "world/objects/interactive.h"
#include "world/objects/item.h"
#include "world/world.h"
#include "utils/gthfont.h"
#include "utils/keycodec.h"
#include "gothic.h"
#include "resources.h"

using namespace Tempest;

struct InventoryMenu::Page {
  Page()=default;
  Page(const Page&)=delete;
  virtual ~Page()=default;

  size_t                      size() const {
    if(is(nullptr))
      return 0;
    size_t ret = 0;
    auto it = iterator();
    while(it.isValid()) {
      ret++;
      ++it;
      }
    return ret;
    }
  Inventory::Iterator         get(size_t id) const {
    auto it = iterator();
    for(size_t i=0; i<id && it.isValid(); ++i)
      ++it;
    return it;
    }

  // fork: drag & drop resolves the item by class id, never by a stored Item*
  const Item*                 item(size_t clsId) const {
    for(auto it = iterator(); it.isValid(); ++it)
      if(it->clsId()==clsId)
        return &*it;
    return nullptr;
    }

  virtual bool                is(const Inventory* i) const { return i==nullptr; }
  virtual const Inventory*    inventory() const { return nullptr; }
  virtual Inventory::Iterator iterator() const { throw std::runtime_error("index out of range");  }
  };

struct InventoryMenu::InvPage : InventoryMenu::Page {
  InvPage(const Inventory& i, const Inventory::ItemFilter* f):inv(i),filter(f){}

  bool                is(const Inventory* i) const override { return &inv==i; }
  const Inventory*    inventory() const override { return &inv; }
  Inventory::Iterator iterator() const override {
    return inv.iterator(Inventory::T_Inventory,filter);
    }

  const Inventory&                   inv;
  const Inventory::ItemFilter*       filter = nullptr;
  };

struct InventoryMenu::TradePage : InventoryMenu::Page {
  TradePage(const Inventory& i, const Inventory::ItemFilter* f):inv(i),filter(f){}

  bool                is(const Inventory* i) const override { return &inv==i; }
  const Inventory*    inventory() const override { return &inv; }
  Inventory::Iterator iterator() const override {
    return inv.iterator(Inventory::T_Trade,filter);
    }

  const Inventory&                   inv;
  const Inventory::ItemFilter*       filter = nullptr;
  };

struct InventoryMenu::RansackPage : InventoryMenu::Page {
  RansackPage(const Inventory& i, const Inventory::ItemFilter* f):inv(i),filter(f){}

  bool                is(const Inventory* i) const override { return &inv==i; }
  const Inventory*    inventory() const override { return &inv; }
  Inventory::Iterator iterator() const override {
    return inv.iterator(Inventory::T_Ransack,filter);
    }

  const Inventory&                   inv;
  const Inventory::ItemFilter*       filter = nullptr;
  };

InventoryMenu::InventoryMenu(const KeyCodec& key)
  :keycodec(key) {
  slot = Resources::loadTexture("INV_SLOT.TGA");
  selT = Resources::loadTexture("INV_SLOT_HIGHLIGHTED.TGA");
  selU = Resources::loadTexture("INV_SLOT_EQUIPPED.TGA");
  tex  = Resources::loadTexture("INV_BACK.TGA"); // INV_TITEL.TGA

  int invMaxColumns = Gothic::settingsGetI("GAME","invMaxColumns");
  if(invMaxColumns>0)
    columsCount = size_t(invMaxColumns); else
    columsCount = 5;

  setFocusPolicy(NoFocus);
  setCursorShape(CursorShape::Hidden);
  takeTimer.timeout.bind(this,&InventoryMenu::onHoldTick);
  }

InventoryMenu::~InventoryMenu() {
  }

void InventoryMenu::close() {
  if(state!=State::Closed) {
    if(state==State::Trade)
      Gothic::inst().emitGlobalSound("TRADE_CLOSE"); else
      Gothic::inst().emitGlobalSound("INV_CLOSE");
    }
  cancelDrag();
  renderer.reset(true);
  state  = State::Closed;
  gridsCount = 0;
  tabHover = -1;
  takeAll.valid = false;
  takeAllHover  = false;
  mouseTracked  = false;
  updateCursor();
  }

void InventoryMenu::open(Npc &pl) {
  if(pl.isDown() || pl.isMonster() || pl.isInAir() || pl.isSlide() || (pl.interactive()!=nullptr))
    return;
  if(pl.bodyStateMasked()==BS_UNCONSCIOUS || pl.bodyStateMasked()==BS_LIE)
    return;
  if(pl.weaponState()!=WeaponState::NoWeapon) {
    pl.stopAnim("");
    pl.closeWeapon(false);
    }
  state  = State::Equip;
  player = &pl;
  trader = nullptr;
  chest  = nullptr;
  page   = 0;
  pagePl .reset(new InvPage  (pl.inventory(), inventoryFilter()));
  pageOth.reset();
  pageLocal[1] = tabLocal[tabs.active()];
  cancelDrag();
  adjustScroll();
  update();
  updateCursor();

  Gothic::inst().emitGlobalSound("INV_OPEN");
  }

void InventoryMenu::trade(Npc &pl, Npc &tr) {
  if(pl.isDown())
    return;
  state  = State::Trade;
  player = &pl;
  trader = &tr;
  chest  = nullptr;
  page   = 0;
  pagePl .reset(new InvPage  (pl.inventory(), inventoryFilter()));
  pageOth.reset(new TradePage(tr.inventory(), inventoryFilter()));
  pageLocal[1] = tabLocal[tabs.active()];
  cancelDrag();
  adjustScroll();
  update();
  updateCursor();
  Gothic::inst().emitGlobalSound("TRADE_OPEN");
  }

bool InventoryMenu::ransack(Npc &pl, Npc &tr) {
  if(pl.isDown())
    return false;
  auto it = tr.inventory().iterator(Inventory::T_Ransack,inventoryFilter());
  if(!it.isValid())
    return false;
  state  = State::Ransack;
  player = &pl;
  trader = &tr;
  chest  = nullptr;
  page   = 0;
  pagePl .reset(new InvPage    (pl.inventory(), inventoryFilter()));
  pageOth.reset(new RansackPage(tr.inventory(), inventoryFilter()));
  pageLocal[1] = tabLocal[tabs.active()];
  cancelDrag();
  adjustScroll();
  update();
  updateCursor();
  Gothic::inst().emitGlobalSound("INV_OPEN");
  return true;
  }

void InventoryMenu::open(Npc &pl, Interactive &ch) {
  if(pl.isDown())
    return;
  const bool needToPicklock = ch.needToLockpick(pl);
  if(!pl.setInteraction(&ch))
    return;

  if(needToPicklock && !ch.isCracked()) {
    state = State::LockPicking;
    } else {
    state = State::Chest;
    }

  player = &pl;
  trader = nullptr;
  chest  = &ch;
  page   = 0;
  pagePl .reset(new InvPage(pl.inventory(), inventoryFilter()));
  pageOth.reset(new InvPage(ch.inventory(), inventoryFilter()));
  pageLocal[1] = tabLocal[tabs.active()];
  cancelDrag();
  adjustScroll();
  update();
  updateCursor();
  }

InventoryMenu::State InventoryMenu::isOpen() const {
  return state;
  }

bool InventoryMenu::isActive() const {
  return state!=State::Closed;
  }

bool InventoryMenu::wantsCursor() const {
  return isMouseMode();
  }

void InventoryMenu::onWorldChanged() {
  close();
  player = nullptr;
  trader = nullptr;
  chest  = nullptr;
  tabs.resetCache();
  }

void InventoryMenu::tick(uint64_t /*dt*/) {
  if(player!=nullptr && (player->isDown() || player->bodyStateMasked()==BS_UNCONSCIOUS)) {
    close();
    return;
    }

  if(state==State::LockPicking) {
    if(chest->isCracked()) {
      state = State::Chest;
      return;
      }
    }

  if(state==State::Ransack) {
    if(trader==nullptr){
      close();
      return;
      }

    if(!trader->isDown()) {
      close();
      return;
      }

    auto it = trader->inventory().iterator(Inventory::T_Ransack,inventoryFilter());
    if(!it.isValid())
      close();
    }

  if(state==State::Closed) {
    if(player!=nullptr){
      if(!player->setInteraction(nullptr))
         return;
      player = nullptr;
      chest  = nullptr;
      }

    page = 0;
    renderer.reset();
    pagePl .reset();
    pageOth.reset();
    update();
    }
  }

void InventoryMenu::processMove(KeyEvent& e) {
  auto key = keycodec.tr(e);
  if(key==KeyCodec::Forward)
    moveUp();
  else if(key==KeyCodec::Back)
    moveDown();
  else if(key==KeyCodec::Left || key==KeyCodec::RotateL)
    moveLeft(true);
  else if(key==KeyCodec::Right || key==KeyCodec::RotateR)
    moveRight(true);
  }

void InventoryMenu::moveLeft(bool usePage) {
  auto& sel = activePageSel();

  if(usePage && sel.sel%columsCount==0 && page>0)
    page--;
  else if(sel.sel>0)
    sel.sel--;
  }

void InventoryMenu::moveRight(bool usePage) {
  auto&        pg     = activePage();
  auto&        sel    = activePageSel();
  const size_t pCount = pagesCount();

  if(usePage && ((sel.sel+1u)%columsCount==0 || sel.sel+1u==pg.size() || pg.size()==0) && page+1u<pCount)
    page++;
  else if(sel.sel+1<pg.size())
    sel.sel++;
  }

void InventoryMenu::moveUp() {
  auto& sel = activePageSel();

  if(sel.sel>=columsCount)
    sel.sel -= columsCount;
  else
    moveLeft(false);
  }

void InventoryMenu::moveDown() {
  auto& pg  = activePage();
  auto& sel = activePageSel();

  if(sel.sel+columsCount<pg.size())
    sel.sel += columsCount;
  else
    moveRight(false);
  }

void InventoryMenu::keyDownEvent(KeyEvent &e) {
  if(state==State::Closed || state==State::LockPicking){
    e.ignore();
    return;
    }

  processMove(e);

  if(keycodec.tr(e)==KeyCodec::Jump) {
    lootMode = LootMode::Stack;
    takeTimer.start(200);
    onTakeStuff();
    }
  else if (keycodec.tr(e)==KeyCodec::ActionGeneric || e.key==KeyEvent::K_Return) {
    onItemAction(Item::NSLOT);
    }
  else if((KeyEvent::K_3<=e.key && e.key<=KeyEvent::K_9) || e.key==KeyEvent::K_0) {
    uint8_t slot = 10;
    if((KeyEvent::K_3<=e.key && e.key<=KeyEvent::K_9))
      slot = uint8_t(e.key-KeyEvent::K_0);
    onItemAction(slot);
    }
  else if(e.key==KeyEvent::K_ESCAPE || keycodec.tr(e)==KeyCodec::Inventory){
    if(drag.armed || drag.active)
      cancelDrag(); else
      close();
    }
  else if(e.key==KeyEvent::K_Space) {
    lootMode = LootMode::Normal;
    takeTimer.start(200);
    onTakeStuff();
    }
  else if(e.key==KeyEvent::K_Z) {
    lootMode = LootMode::Ten;
    takeTimer.start(200);
    onTakeStuff();
    }
  else if(e.key==KeyEvent::K_X) {
    lootMode = LootMode::Hundred;
    takeTimer.start(200);
    onTakeStuff();
    }
  else if(isTabsMode() && (e.key==KeyEvent::K_PageUp || e.key==KeyEvent::K_PageDown)) {
    cycleTab(e.key==KeyEvent::K_PageDown ? +1 : -1);
    }
  else if(isMouseMode() && e.key==KeyEvent::K_A && (e.modifier & Event::M_Shift)!=0) {
    takeAllItems(); // fork: take-all shortcut, same as the button
    }

  adjustScroll();
  update();
  }

void InventoryMenu::keyRepeatEvent(KeyEvent& e) {
  if(state==State::LockPicking || state==State::Closed)
    return;
  processMove(e);
  adjustScroll();
  update();
  }

void InventoryMenu::keyUpEvent(KeyEvent&) {
  takeTimer.stop();
  lootMode = LootMode::Normal;
  }

// ---------------------------------------------------------------------------------------------
// fork: mouse control ([GAME] invMouse) and drag & drop, see docs/fork/plan-ui-overhaul.md

void InventoryMenu::mouseDownEvent(MouseEvent &e) {
  if(player==nullptr || state==State::Closed) {
    e.ignore();
    return;
    }

  if(state==State::LockPicking)
    return;

  if(e.button==MouseEvent::ButtonRight) {
    // during a drag the right button cancels the drag instead of closing the window
    if(drag.armed || drag.active) {
      cancelDrag();
      update();
      return;
      }
    close();
    return;
    }

  if(e.button!=MouseEvent::ButtonLeft)
    return;

  if(!isMouseMode()) {
    onItemAction(Item::NSLOT);
    adjustScroll();
    return;
    }

  mousePos     = e.pos();
  mouseTracked = true;

  // A new press supersedes any half-finished interaction. Without this a mouse-up that never reached
  // the window (released outside, alt-tab) left a drag armed, and the next click on a tab was eaten
  // by the stale click action on the old slot.
  if(drag.armed || drag.active)
    cancelDrag();

  // tabs and the take-all button come first: they are drawn in the same pass as the grids
  if(const int8_t t = tabAt(e.pos()); t>=0) {
    switchTab(size_t(t));
    update();
    return;
    }
  if(takeAll.valid && takeAll.rect.contains(e.pos())) {
    takeAllItems();
    update();
    return;
    }

  auto g = gridAt(e.pos());
  if(g==nullptr)
    return;
  auto pg = sidePage(g->side);
  if(pg==nullptr || g->wcount==0 || g->hcount==0)
    return;

  const size_t id = slotIdAt(*g,e.pos());
  if(id>=pg->size())
    return;
  auto it = pg->get(id);
  if(!it.isValid())
    return;

  if(pageOth!=nullptr)
    page = g->side;
  pageLocal[g->side].sel = id;

  // only arm the interaction: a drag may start from this slot, so the action must wait for release
  drag         = DragState();
  drag.armed   = true;
  drag.side    = g->side;
  drag.clsId   = it->clsId();
  drag.count   = it.count();
  drag.index   = id;
  drag.start   = e.pos();
  drag.pos     = e.pos();
  drag.target  = int(g->side);

  // holding the button still repeats "take one" after a delay; a quick release acts once
  takeCount = 0;
  lootMode  = LootMode::Normal;
  takeTimer.start(600);

  adjustScroll();
  update();
  }

void InventoryMenu::mouseUpEvent(MouseEvent& e) {
  takeTimer.stop();
  takeCount = 0;

  const bool            wasArmed  = drag.armed;
  const bool            wasActive = drag.active;
  const bool            repeated  = drag.repeated;
  const uint8_t         srcSide   = drag.side;
  const size_t          clsId     = drag.clsId;
  const int             target    = drag.target;
  const Tempest::Point  start     = drag.start;
  drag = DragState();
  updateCursor();

  const bool mouseMode = isMouseMode();

  if(wasActive) {
    // drop: resolve by class id, the inventory may have changed under the drag
    resolveDrop(srcSide,clsId,target);
    adjustScroll();
    update();
    return;
    }

  if(wasArmed && !repeated && mouseMode && e.button==MouseEvent::ButtonLeft) {
    // release without movement = click: use/equip, or take exactly one item
    if(selectAt(start)) {
      if(state==State::Equip) {
        onItemAction(Item::NSLOT);
        } else {
        lootMode = LootMode::Normal;
        takeCount = 1;              // exactly one; must not start the hold-repeat timer
        onTakeStuff();
        }
      }
    }

  adjustScroll();
  if(mouseMode)
    update();
  }

void InventoryMenu::mouseDragEvent(MouseEvent& e) {
  if(!isMouseMode()) {
    e.ignore();
    return;
    }

  mousePos     = e.pos();
  mouseTracked = true;

  if(!drag.armed)
    return;

  if(!drag.active) {
    const int thr = int(8*Gothic::interfaceScale(this));
    const int dx  = std::abs(e.pos().x-drag.start.x);
    const int dy  = std::abs(e.pos().y-drag.start.y);
    if(dx<thr && dy<thr) {
      // A small movement is still a hold, but it postpones the repeat: a player who starts moving
      // to drag must never trigger the "hold to take one" action while doing so.
      if(dx>1 || dy>1)
        takeTimer.start(600);
      return;
      }
    drag.active   = true;
    drag.repeated = false;
    takeTimer.stop();
    setCursorShape(CursorShape::SizeAll);
    }

  drag.pos = e.pos();
  auto g   = gridAt(e.pos());
  const int target = g!=nullptr ? int(g->side) : -1;
  if(target!=drag.target)
    drag.target = target;
  update();
  }

void InventoryMenu::mouseDoubleClickEvent(MouseEvent& e) {
  if(player==nullptr || state==State::Closed) {
    e.ignore();
    return;
    }
  if(!isMouseMode() || state==State::LockPicking)
    return;
  performDoubleClick(e.pos());
  }

void InventoryMenu::mouseEnterEvent(MouseEvent& e) {
  mousePos     = e.pos();
  mouseTracked = true;
  }

void InventoryMenu::mouseLeaveEvent(MouseEvent&) {
  mouseTracked = false;
  tabHover     = -1;
  takeAllHover = false;
  // the cursor left the window mid-drag: the drop cannot be resolved, so cancel instead of leaving a
  // ghost that follows the cursor until the next click
  if(drag.armed || drag.active)
    cancelDrag();
  update();
  }

void InventoryMenu::mouseWheelEvent(MouseEvent &e) {
  if(state==State::Closed) {
    e.ignore();
    return;
    }

  if(state==State::LockPicking)
    return;

  if(isMouseMode()) {
    if(drag.armed || drag.active)
      return;
    scrollDelta += e.delta;
    const int rows = scrollDelta/120;
    scrollDelta %= 120;
    if(rows!=0) {
      if((e.modifier & Event::M_Shift)!=0 || tabAt(e.pos())>=0) {
        cycleTab(rows>0 ? -1 : +1);
        } else {
        auto g = gridAt(e.pos());
        scrollGrid(g!=nullptr ? g->side : activeSide(), -rows);
        selectAt(e.pos());
        }
      adjustScroll();
      update();
      }
    return;
    }

  scrollDelta += e.delta;
  if(scrollDelta>0) {
    for(int i=0;i<scrollDelta/120;++i)
      moveUp();
    scrollDelta %= 120;
    } else {
    for(int i=0;i<-scrollDelta/120;++i)
      moveDown();
    scrollDelta %= 120;
    }
  adjustScroll();
  }

void InventoryMenu::onHoldTick() {
  if(!drag.armed || drag.active) {
    takeTimer.stop();
    return;
    }
  if(state==State::Equip) {
    takeTimer.stop();          // no "keep using" for equipment
    return;
    }
  drag.repeated = true;
  onTakeStuff();
  adjustScroll();
  update();
  takeTimer.start(200);
  }

void InventoryMenu::cancelDrag() {
  takeTimer.stop();
  takeCount = 0;
  drag = DragState();
  updateCursor();
  }

void InventoryMenu::resolveDrop(uint8_t srcSide, size_t clsId, int dstSide) {
  if(player==nullptr || dstSide<0 || int(srcSide)==dstSide)
    return;                    // dropping back on the source grid is a cancel, not a reorder

  auto src = sidePage(srcSide);
  if(src==nullptr)
    return;
  const Item* item = src->item(clsId);
  if(item==nullptr)
    return;                    // the stack vanished under the drag

  const size_t count = item->count();
  const bool   fromPlayer = srcSide==1;

  if(state==State::Chest && chest!=nullptr) {
    if(fromPlayer) player->moveItem(clsId,*chest,count); else
                   player->addItem (clsId,*chest,count);
    Gothic::inst().emitGlobalSound("INV_CHANGE");
    }
  else if(state==State::Trade && trader!=nullptr) {
    if(fromPlayer) player->sellItem(clsId,*trader,count); else
                   player->buyItem (clsId,*trader,count);
    Gothic::inst().emitGlobalSound("INV_CHANGE");
    }
  else if(state==State::Ransack && trader!=nullptr) {
    if(!fromPlayer) {
      player->addItem(clsId,*trader,count);
      Gothic::inst().emitGlobalSound("INV_CHANGE");
      }
    }
  }

void InventoryMenu::performDoubleClick(const Point& pos) {
  auto g = gridAt(pos);
  if(g==nullptr)
    return;
  auto pg = sidePage(g->side);
  if(pg==nullptr || g->wcount==0 || g->hcount==0)
    return;

  const size_t id = slotIdAt(*g,pos);
  if(id>=pg->size())
    return;
  auto it = pg->get(id);
  if(!it.isValid())
    return;

  if(pageOth!=nullptr)
    page = g->side;
  pageLocal[g->side].sel = id;

  if(state==State::Equip) {
    // The first click of the pair already used/equipped the item (clicks act on release), so a
    // second action here would toggle it back. Double click means "the whole stack" for containers.
    return;
    }

  const size_t clsId = it->clsId();
  const size_t count = it.count();

  if(state==State::Chest && chest!=nullptr) {
    if(g->side==1) player->moveItem(clsId,*chest,count); else
                   player->addItem (clsId,*chest,count);
    Gothic::inst().emitGlobalSound("INV_CHANGE");
    }
  else if(state==State::Trade && trader!=nullptr) {
    if(g->side==1) player->sellItem(clsId,*trader,count); else
                   player->buyItem (clsId,*trader,count);
    Gothic::inst().emitGlobalSound("INV_CHANGE");
    }
  else if(state==State::Ransack && trader!=nullptr) {
    if(g->side==0) {
      player->addItem(clsId,*trader,count);
      Gothic::inst().emitGlobalSound("INV_CHANGE");
      }
    }
  adjustScroll();
  update();
  }

size_t InventoryMenu::rowsCount() const {
  const float scale = Gothic::interfaceScale(this);
  const int   y0    = gridTop();
  int         bottom;
  if(isMouseMode()) {
    // the cursor tooltip replaces the info box; the hint line lives under the panels
    bottom = int(30*scale) + 2*panelPad() + int(10*scale);
    } else {
    bottom = infoHeight() + int(20*scale);
    }
  return size_t(std::max(1,(h()-y0-bottom)/slotSize().h));
  }

// fork: fixed offsets used to be raw pixels, which breaks the layout on a high-DPI window
// (the header overlapped the top row). Scale them like the cells and the font.
int InventoryMenu::gridTop() const {
  return int(134*Gothic::interfaceScale(this));
  }

int InventoryMenu::padd() const {
  return int(43*Gothic::interfaceScale(this));
  }

int InventoryMenu::panelPad() const {
  return int(9*Gothic::interfaceScale(this));
  }

int InventoryMenu::headerH() const {
  return int(36*Gothic::interfaceScale(this));
  }

void InventoryMenu::paintEvent(PaintEvent &e) {
  if(player==nullptr || state==State::Closed)
    return;
  renderer.reset();

  Painter p(e);
  drawAll(p,*player,DrawPass::Back);
  }

void InventoryMenu::paintNumOverlay(PaintEvent& e) {
  if(player==nullptr || state==State::Closed)
    return;

  Painter p(e);
  drawAll(p,*player,DrawPass::Front);
  }

Size InventoryMenu::slotSize() const {
  const float scale = Gothic::interfaceScale(this);
  const float cell  = float(Gothic::options().inventoryCellSize);
  return Size(int(cell*scale),int(cell*scale));
  }

int InventoryMenu::infoHeight() const {
  const float scale = Gothic::interfaceScale(this);
  return (Item::MAX_UI_ROWS+2)*int(float(Resources::font(scale).pixelSize()))+int(scale*10)/*padding bottom*/;
  }

size_t InventoryMenu::pagesCount() const {
  if(state==State::Chest || state==State::Trade)
    return 2;
  return 1;
  }

const InventoryMenu::Page &InventoryMenu::activePage() {
  if(pageOth!=nullptr)
    return *(page==0 ? pageOth : pagePl);
  if(pagePl)
    return *pagePl;

  static Page n;
  return n;
  }

InventoryMenu::PageLocal &InventoryMenu::activePageSel() {
  if(pageOth!=nullptr)
    return (page==0 ? pageLocal[0] : pageLocal[1]);
  return pageLocal[1];
  }

void InventoryMenu::onItemAction(uint8_t slotHint) {
  auto& page = activePage();
  auto& sel  = activePageSel();

  auto it = page.get(sel.sel);
  if(!it.isValid())
    return;

  if(state==State::Equip) {
    const size_t clsId = it->clsId();
    if(it.isEquipped() && slotHint==Item::NSLOT) {
      player->unequipItem(clsId);
      } else {
      player->useItem(clsId,slotHint,false);
      auto it2 = page.get(sel.sel);
      if((!it2.isValid() || it2->clsId()!=clsId) && sel.sel>0)
        --sel.sel;
      }
    }
  else if(state==State::Chest || state==State::Trade || state==State::Ransack) {
    lootMode = LootMode::Normal;
    takeTimer.start(200);
    onTakeStuff();
    }
  }

void InventoryMenu::onTakeStuff() {
  size_t itemCount = 0;
  auto& page = activePage();
  auto& sel  = activePageSel();
  if(sel.sel >= page.size())
    return;
  auto it = page.get(sel.sel);
  if(lootMode==LootMode::Normal) {
    ++takeCount;
    itemCount = uint32_t(std::pow(10,takeCount / 10));
    if(it.count() <= itemCount) {
      itemCount = uint32_t(it.count());
      takeCount = 0;
      }
    }
  else if(lootMode==LootMode::Stack) {
    itemCount = it.count();
    }
  else if(lootMode==LootMode::Ten) {
    itemCount = 10;
    }
  else if(lootMode==LootMode::Hundred) {
    itemCount = 100;
    }

  if(it.count() < itemCount) {
    itemCount = it.count();
    }

  if(state==State::Chest) {
    if(page.is(&player->inventory())) {
      player->moveItem(it->clsId(),*chest,itemCount);
      } else {
      player->addItem (it->clsId(),*chest,itemCount);
      }
    }
  else if(state==State::Trade) {
    if(page.is(&player->inventory())) {
      player->sellItem(it->clsId(),*trader,itemCount);
      } else {
      player->buyItem (it->clsId(),*trader,itemCount);
      }
    }
  else if(state==State::Ransack) {
    if(page.is(&trader->inventory())) {
      player->addItem(it->clsId(),*trader,itemCount);
      }
    }
  else if(state==State::Equip) {
    player->dropItem(it->clsId(),itemCount);
    }
  adjustScroll();
  }

void InventoryMenu::adjustScroll() {
  for(uint8_t s=0;s<2;++s) {
    auto pg = sidePage(s);
    if(pg==nullptr)
      continue;

    auto&        sel  = pageLocal[s];
    const size_t size = pg->size();
    sel.sel = std::min(sel.sel, std::max<size_t>(size,1)-1);

    while(sel.scroll>0 && sel.sel<sel.scroll*columsCount)
      sel.scroll-=1;

    const size_t hcount = rowsCount();
    while(sel.sel>=(sel.scroll+hcount)*columsCount)
      sel.scroll+=1;
    }
  }

void InventoryMenu::drawAll(Painter &p, Npc &player, DrawPass pass) {
  const float  scale    = Gothic::interfaceScale(this);
  const int    padd     = this->padd();
  const int    iy       = gridTop();
  const int    pp       = panelPad();
  const int    hdrH     = headerH();
  const Size   cell     = slotSize();
  const int    wcount   = int(columsCount);
  const int    hcount   = int(rowsCount());
  const int    gridW    = wcount*cell.w;
  const int    gridH    = hcount*cell.h;
  (void)player;

  if(pass==DrawPass::Back) {
    gridsCount    = 0;
    takeAll.valid = false;
    }

  if(state==State::LockPicking)
    return;

  // 1. dim the world behind the window so frames and text stay readable
  if(pass==DrawPass::Back && isScrimMode())
    UiDraw::scrim(p,Rect(0,0,w(),h()),0.45f);

  // 2. table of sides that are on screen
  struct Side final {
    const Page*      page = nullptr;
    PageLocal*       sel  = nullptr;
    bool             isPlayer = false;
    std::string_view title;
    int              x = 0;
    };
  Side sides[2];
  size_t sidesCount = 0;
  if(pageOth!=nullptr) {
    std::string_view title;
    if(chest!=nullptr)       title = chest->displayName(); else
    if(trader!=nullptr)      title = trader->displayName(); else
                             title = "Chest";
    sides[sidesCount++] = Side{pageOth.get(),&pageLocal[0],false,title,padd};
    }
  if(state!=State::Ransack) {
    sides[sidesCount++] = Side{pagePl.get(),&pageLocal[1],true,"Inventory",w()-padd-gridW};
    }

  // 3. panels (frames + headers) first, so the slots are drawn on top
  const int tabH = isTabsMode() ? int(44*scale) : 0;
  if(pass==DrawPass::Back) {
    for(size_t i=0;i<sidesCount;++i) {
      auto& s = sides[i];
      const Rect panel(s.x-pp, iy-hdrH-pp, gridW+2*pp, hdrH+gridH+2*pp);
      const Rect header(s.x, iy-hdrH, gridW, hdrH);
      drawPanel(p,panel,header,s.isPlayer,s.title);
      }
    // Tab strip: exactly as wide as the panel(s), flush on top of them (no gap, no inset), so it
    // reads as a toolbar of the window instead of a floating element.
    if(isTabsMode() && sidesCount>0) {
      const int x0 = sides[0].x - pp;
      const int x1 = sides[sidesCount-1].x + gridW + pp;
      const Rect strip(x0, iy-hdrH-pp-tabH, x1-x0, tabH);
      drawTabs(p,strip,*pagePl);
      }
    }

  // 4. grids
  for(size_t i=0;i<sidesCount;++i) {
    auto& s = sides[i];
    const uint8_t side = s.isPlayer ? 1 : 0;
    drawItems(p,pass,*s.page,*s.sel,side,s.x,iy,wcount,hcount);
    }

  if(pass==DrawPass::Back) {
    if(!isMouseMode())
      drawInfo(p);
    drawHints(p);
    drawDragGhost(p);
    } else {
    // The tooltip goes into the front overlay pass: the 3D item models are drawn *between* the two
    // UI layers (Renderer::draw), so a tooltip in the normal UI layer had the icons floating on top
    // of its text. The plate is near-opaque because it has to hide those models.
    drawTooltip(p);
    }
  }

void InventoryMenu::drawItems(Painter &p, DrawPass pass,
                              const Page &inv, const PageLocal& sel, uint8_t side,
                              int x0, int y, int wcount, int hcount) {
  if(state==State::LockPicking)
    return;

  const Size  cell = slotSize();
  const int   y0   = y;
  const Rect  area (x0,y0,cell.w*wcount,cell.h*hcount);

  if(pass==DrawPass::Back) {
    if(tex!=nullptr) {
      p.setBrush(*tex);
      p.drawRect(area.x,area.y,area.w,area.h, 0,0,tex->w(),tex->h());
      }

    // drop target highlight while dragging
    if(drag.active && drag.target==int(side)) {
      UiDraw::fill  (p,area,UiDraw::Theme::accentSoft(0.10f));
      UiDraw::outline(p,area,UiDraw::Theme::accentSoft(0.85f),int(2*Gothic::interfaceScale(this)));
      }
    }

  if(pass==DrawPass::Back && gridsCount<std::size(grids)) {
    auto& g  = grids[gridsCount++];
    g.rect   = area;
    g.side   = side;
    g.wcount = size_t(wcount);
    g.hcount = size_t(hcount);
    }

  auto   it = inv.iterator();
  size_t id = 0;
  for(size_t i=0; it.isValid() && i<sel.scroll*size_t(wcount); ++i) {
    ++it;
    ++id;
    }
  for(int i=0;i<hcount;++i) {
    for(int r=0;r<wcount;++r) {
      const int x = x0 + r*cell.w;
      if(pass==DrawPass::Back) {
        if(slot!=nullptr) {
          p.setBrush(*slot);
          p.drawRect(x,y,cell.w,cell.h, 0,0,slot->w(),slot->h());
          }
        }
      if(it.isValid()) {
        drawSlot(p,pass, it,inv,sel, x,y, id);
        ++it;
        ++id;
        }
      }
    y += cell.h;
    }

  if(pass==DrawPass::Back) {
    // Explicit frame around the grid. The cell textures have no border on their outer left/top edge,
    // which made the table look clipped against the panel there; this closes the rectangle.
    const float scale = Gothic::interfaceScale(this);
    UiDraw::outline(p,area,UiDraw::Theme::gridLine(),std::max(1,int(2*scale)));
    }
  }

void InventoryMenu::drawSlot(Painter &p, DrawPass pass, const Inventory::Iterator &it,
                             const Page& page, const PageLocal &sel,
                             int x, int y, size_t id) {
  if(!slot)
    return;

  auto& active = activePage();
  const float scale = Gothic::interfaceScale(this);
  const Size  cell  = slotSize();

  if(pass==DrawPass::Back) {
    if((!it.isValid() && id==0) || (id==sel.sel && &page==&active)){
      p.setBrush(*selT);
      p.drawRect(x,y,cell.w,cell.h, 0,0,selT->w(),selT->h());
      }

    if(it.isEquipped() && selU!=nullptr) {
      p.setBrush(*selU);
      p.drawRect(x,y,cell.w,cell.h, 0,0,selU->w(),selU->h());
      }

    const int dsz = (id==sel.sel ? 5 : 0);
    renderer.drawItem(x-dsz, y-dsz, cell.w+2*dsz, cell.h+2*dsz, *it);
    } else {
    auto fnt = Resources::font(scale);

    if(it.count()>1) {
      string_frm vint(int(it.count()));
      auto sz = fnt.textSize(vint);
      // dark plate behind the number: item models are bright and the plain text was hard to read
      const Rect tr(x+cell.w-sz.w-int(8*scale), y+cell.h-sz.h-int(4*scale),
                    sz.w+int(10*scale), sz.h+int(2*scale));
      UiDraw::fill(p,tr,Color(0,0,0,0.45f));
      fnt.drawText(p,tr.x+int(5*scale),tr.y+tr.h-int(2*scale),vint);
      }

    if(it.slot()!=Item::NSLOT) {
      fnt = Resources::font(Resources::FontType::Red, scale);

      string_frm vint(int(it.slot()));
      auto sz = fnt.textSize(vint);
      const Rect tr(x+int(4*scale), y+cell.h/2-sz.h/2, sz.w+int(8*scale), sz.h);
      UiDraw::fill(p,tr,Color(0,0,0,0.40f));
      fnt.drawText(p,tr.x+int(4*scale),tr.y+tr.h,vint);
      }
    }
  }

void InventoryMenu::drawPanel(Painter& p, const Rect& panel, const Rect& header,
                              bool playerSide, std::string_view title) {
  const float scale = Gothic::interfaceScale(this);

  UiDraw::shadow(p,panel,int(4*scale));
  UiDraw::panel (p,panel,tex,0.82f,true);

  // header band: a slightly lighter plate with the title on the left and the action on the right
  UiDraw::fill(p,header,Color(0,0,0,0.34f));
  UiDraw::fill(p,Rect(header.x,header.y+header.h-std::max(1,int(scale)),header.w,std::max(1,int(scale))),
               Color(1,1,1,0.10f));

  auto& fntH = Resources::font(Resources::FontType::Hi, scale);
  UiDraw::textLeft(p,fntH,header,title,int(10*scale));

  if(playerSide) {
    drawGold(p,header.x,header.y,header.w);
    } else
  if((state==State::Chest || state==State::Ransack) && pageOth!=nullptr && pageOth->size()>0) {
    drawTakeAll(p,header);
    } else
  if(pageOth!=nullptr) {
    // trade: the trader has no take-all, show how much is on offer instead
    auto&     fntD = Resources::font(Resources::FontType::Disabled, scale);
    string_frm cnt(int(pageOth->size())," items");
    UiDraw::textRight(p,fntD,header,cnt,int(10*scale));
    }
  }

std::string_view InventoryMenu::hints() const {
  switch(state) {
    case State::Equip:   return "Click: use / equip      Right click: close";
    case State::Chest:   return "Drag: move      Double click: whole stack      Right click: close";
    case State::Trade:   return "Drag: sell / buy      Double click: whole stack      Right click: close";
    case State::Ransack: return "Drag: take      Double click: whole stack      Right click: close";
    default:             return "";
    }
  }

void InventoryMenu::drawHints(Painter& p) {
  if(!isMouseMode())
    return;
  const float scale = Gothic::interfaceScale(this);
  auto&       fnt   = Resources::font(Resources::FontType::Disabled, scale);
  const auto  hint  = hints();
  const auto  sz    = fnt.textSize(hint);
  const Rect  r((w()-sz.w)/2-int(14*scale), h()-int(12*scale)-sz.h-int(6*scale),
                sz.w+int(28*scale), sz.h+int(10*scale));
  UiDraw::textPlate(p,r,0.40f);
  UiDraw::textCenter(p,fnt,r,hint);
  }

void InventoryMenu::drawGold(Painter& p, int x, int y, int w) {
  if(!slot)
    return;
  auto           wrld = Gothic::inst().world();
  auto           txt  = wrld ? wrld->script().currencyName() : "";
  const size_t   gold = player==nullptr ? 0 : player->inventory().goldCount();
  if(txt.empty())
    txt="Gold";

  const float scale = Gothic::interfaceScale(this);
  auto&       fntH  = Resources::font(Resources::FontType::Hi, scale);
  string_frm  vint(txt," : ",int(gold));
  const Rect  r(x,y,w,headerH());
  UiDraw::textRight(p,fntH,r,vint,int(10*scale));
  }

void InventoryMenu::drawInfo(Painter &p) {
  const float scale = Gothic::interfaceScale(this);
  const int   dw    = std::min(w(), int(720*scale));
  const int   dh    = infoHeight();
  const int   x     = (w()-dw)/2;
  const int   y     = h()-dh-20;

  auto& pg  = activePage();
  auto& sel = activePageSel();

  auto it = pg.get(sel.sel);
  if(!it.isValid())
    return;

  auto& r = *pg.get(sel.sel);
  UiDraw::panel(p,Rect(x,y,dw,dh),tex,0.82f,true);

  auto& fnt = Resources::font(scale);
  auto  desc = r.description();
  int   tw   = fnt.textSize(desc).w;

  fnt.drawText(p,x+(dw-tw)/2,y+int(fnt.pixelSize()),desc);

  for(size_t i=0;i<Item::MAX_UI_ROWS;++i){
    auto    txt = r.uiText(i);
    int32_t val = r.uiValue(i);

    if(txt.empty())
      continue;

    if(i+1==Item::MAX_UI_ROWS && state==State::Trade && player!=nullptr && pg.is(&player->inventory())){
      val = r.sellCost();
      }

    string_frm vint(val);
    int tw = fnt.textSize(vint).w;

    fnt.drawText(p, x+20,  y+int(i+2)*fnt.pixelSize(), txt);
    if(val!=0)
      fnt.drawText(p,x+dw-tw-20,y+int(i+2)*fnt.pixelSize(),vint);
    }

  const int sz = dh;
  renderer.drawItem(x+dw-sz-sz/2,y,sz,sz,r);
  }

void InventoryMenu::drawTooltip(Painter& p) {
  if(!isMouseMode() || !isTooltipMode() || !mouseTracked)
    return;
  if(drag.active || drag.armed)
    return;

  auto g = gridAt(mousePos);
  if(g==nullptr)
    return;
  auto pg = sidePage(g->side);
  if(pg==nullptr || g->wcount==0 || g->hcount==0)
    return;

  const size_t id = slotIdAt(*g,mousePos);
  if(id>=pg->size())
    return;
  auto it = pg->get(id);
  if(!it.isValid())
    return;

  const Item&  item  = *it;
  const float  scale = Gothic::interfaceScale(this);
  auto&        fnt   = Resources::font(scale);
  auto&        fntH  = Resources::font(Resources::FontType::Hi, scale);
  const int    pad   = int(10*scale);
  const int    lh    = fnt.pixelSize();

  std::vector<std::string> lines;
  std::vector<std::string> values;
  for(size_t i=0;i<Item::MAX_UI_ROWS;++i) {
    auto txt = item.uiText(i);
    if(txt.empty())
      continue;
    int32_t val = item.uiValue(i);
    if(i+1==Item::MAX_UI_ROWS && state==State::Trade && player!=nullptr && pg->is(&player->inventory()))
      val = item.sellCost();
    lines.emplace_back(txt);
    if(val!=0)
      values.emplace_back(string_frm(val).c_str()); else
      values.emplace_back();
    }

  const auto  name = item.displayName();
  std::string cntS;
  if(item.count()>1)
    cntS = string_frm("x",int(item.count())).c_str();

  const auto  nameSz  = fntH.textSize(name);
  const auto  cntSz   = fnt.textSize(cntS);

  int  tipW = nameSz.w + pad*2;
  if(item.count()>1)
    tipW += cntSz.w + pad;
  const int gapCol = int(24*scale);
  for(size_t i=0;i<lines.size();++i) {
    const int lw = fnt.textSize(lines[i]).w + gapCol + fnt.textSize(values[i]).w + pad*2;
    tipW = std::max(tipW,lw);
    }
  tipW = std::max(tipW,int(200*scale));
  const int hdrH = lh + int(7*scale);
  const int tipH = pad*2 + hdrH + int(lines.size())*lh;

  int x = mousePos.x + int(22*scale);
  int y = mousePos.y + int(18*scale);
  if(x+tipW>w()-8)
    x = mousePos.x - tipW - int(22*scale);
  if(y+tipH>h()-8)
    y = mousePos.y - tipH - int(18*scale);
  x = std::clamp(x,8,std::max(8,w()-tipW-8));
  y = std::clamp(y,8,std::max(8,h()-tipH-8));

  const Rect r(x,y,tipW,tipH);
  // near-opaque: this is drawn in the front overlay pass, over the 3D item models
  UiDraw::tooltipPlane(p,r,tex,0.98f);

  int ty = r.y + pad;
  fntH.drawText(p,r.x+pad,ty+lh,name);
  if(item.count()>1)
    fnt.drawText(p,r.x+r.w-pad-cntSz.w,ty+lh,cntS);
  ty += hdrH;
  UiDraw::fill(p,Rect(r.x+pad,ty-std::max(1,int(3*scale)),r.w-2*pad,std::max(1,int(scale))),
               Color(1,1,1,0.14f));

  for(size_t i=0;i<lines.size();++i) {
    fnt.drawText(p,r.x+pad,ty+lh,lines[i]);
    if(!values[i].empty()) {
      const int vw = fnt.textSize(values[i]).w;
      fnt.drawText(p,r.x+r.w-pad-vw,ty+lh,values[i]);
      }
    ty += lh;
    }
  }

void InventoryMenu::drawDragGhost(Painter& p) {
  if(!drag.active)
    return;
  auto pg = sidePage(drag.side);
  if(pg==nullptr)
    return;
  const Item* item = pg->item(drag.clsId);
  if(item==nullptr)
    return;

  const float scale = Gothic::interfaceScale(this);
  const int   sz    = int(float(slotSize().w)*0.85f);
  const int   x     = drag.pos.x - sz/2;
  const int   y     = drag.pos.y - sz/2;

  // a soft plate under the ghost keeps the model readable over the slots
  UiDraw::fill(p,Rect(x,y,sz,sz),Color(0,0,0,0.22f));
  renderer.drawItem(x,y,sz,sz,*item);

  if(item->count()>1) {
    auto&      fnt = Resources::font(scale);
    string_frm cnt("x",int(item->count()));
    const auto cs  = fnt.textSize(cnt);
    const Rect tr(x+sz-cs.w-int(6*scale),y+sz-cs.h-int(2*scale),cs.w+int(10*scale),cs.h+int(2*scale));
    UiDraw::fill(p,tr,Color(0,0,0,0.55f));
    fnt.drawText(p,tr.x+int(5*scale),tr.y+tr.h-int(2*scale),cnt);
    }
  }

void InventoryMenu::draw(Tempest::Encoder<CommandBuffer>& cmd) {
  renderer.draw(cmd);
  }

// fork: mouse control ([GAME] invMouse), see docs/fork/plan-inventory.md
void InventoryMenu::mouseMoveEvent(MouseEvent& e) {
  // accepting the move keeps MainWindow from re-centering the cursor and orbiting the camera
  if(!isMouseMode()) {
    e.ignore();
    return;
    }
  if(cursorShape()!=CursorShape::Arrow && !drag.active)
    setCursorShape(CursorShape::Arrow);

  mousePos     = e.pos();
  mouseTracked = true;

  if(drag.active) {
    drag.pos = e.pos();
    auto g   = gridAt(e.pos());
    drag.target = g!=nullptr ? int(g->side) : -1;
    update();
    return;
    }

  const int8_t tab   = tabAt(e.pos());
  const bool   take  = takeAll.valid && takeAll.rect.contains(e.pos());
  bool         dirty = tab!=tabHover || take!=takeAllHover;
  tabHover     = tab;
  takeAllHover = take;

  if(selectAt(e.pos()) || dirty)
    update();
  }

bool InventoryMenu::isMouseMode() const {
  if(player==nullptr || state==State::Closed || state==State::LockPicking)
    return false;
  return Gothic::settingsGetI("GAME","invMouse")!=0;
  }

bool InventoryMenu::isScrimMode() const {
  return Gothic::settingsGetI("GAME","invScrim")!=0;
  }

bool InventoryMenu::isTooltipMode() const {
  return Gothic::settingsGetI("GAME","invTooltip")!=0;
  }

uint8_t InventoryMenu::activeSide() const {
  if(pageOth!=nullptr)
    return uint8_t(page==0 ? 0 : 1);
  return 1;
  }

const InventoryMenu::Page* InventoryMenu::sidePage(uint8_t side) const {
  return side==0 ? pageOth.get() : pagePl.get();
  }

const InventoryMenu::GridArea* InventoryMenu::gridAt(const Point& pos) const {
  for(size_t i=0; i<gridsCount; ++i)
    if(grids[i].rect.contains(pos))
      return &grids[i];
  return nullptr;
  }

size_t InventoryMenu::slotIdAt(const GridArea& g, const Point& pos) const {
  const Size cell = slotSize();
  const size_t col = std::min(size_t(std::max(0,pos.x-g.rect.x)/cell.w), g.wcount-1);
  const size_t row = std::min(size_t(std::max(0,pos.y-g.rect.y)/cell.h), g.hcount-1);
  return (pageLocal[g.side].scroll+row)*g.wcount + col;
  }

bool InventoryMenu::selectAt(const Point& pos) {
  auto g  = gridAt(pos);
  auto pg = g!=nullptr ? sidePage(g->side) : nullptr;
  if(pg==nullptr || g->wcount==0 || g->hcount==0)
    return false;

  const size_t id  = slotIdAt(*g,pos);
  if(id>=pg->size())
    return false;

  if(pageOth!=nullptr)
    page = g->side;
  pageLocal[g->side].sel = id;
  // true = there is an item under the cursor (not "the selection changed"): hover selects
  // first, and a click after hover must still act on that slot
  return true;
  }

void InventoryMenu::scrollGrid(uint8_t side, int rows) {
  auto pg = sidePage(side);
  if(pg==nullptr || rows==0)
    return;

  auto&        sel       = pageLocal[side];
  const size_t size      = pg->size();
  const size_t total     = (size+columsCount-1)/columsCount;
  const size_t visible   = rowsCount();
  const size_t maxScroll = total>visible ? total-visible : 0;
  const size_t prev      = sel.scroll;
  sel.scroll = size_t(std::clamp<int64_t>(int64_t(prev)+rows, 0, int64_t(maxScroll)));

  // move the selection along, otherwise adjustScroll() scrolls back to it
  if(sel.scroll>prev)
    sel.sel += (sel.scroll-prev)*columsCount;
  else if(sel.sel>=(prev-sel.scroll)*columsCount)
    sel.sel -= (prev-sel.scroll)*columsCount;
  else
    sel.sel = 0;
  sel.sel = std::min(sel.sel, std::max<size_t>(size,1)-1);
  }

// fork: category tabs ([GAME] invTabs) and take-all, see docs/fork/plan-inventory.md
bool InventoryMenu::isTabsMode() const {
  if(player==nullptr || state==State::Closed || state==State::LockPicking)
    return false;
  return Gothic::settingsGetI("GAME","invTabs")!=0;
  }

const Inventory::ItemFilter* InventoryMenu::inventoryFilter() const {
  if(Gothic::settingsGetI("GAME","invTabs")==0)
    return nullptr;
  return tabs.filter();
  }

int8_t InventoryMenu::tabAt(const Point& pos) const {
  // Geometric lookup instead of per-cell hit rects: any pixel inside the tab bar belongs to a tab, so
  // there is no dead gap between cells, the outer padding of the bar is forgiving, and the result
  // cannot go stale between two paints.
  if(tabBar.cellW<=0 || !tabBar.strip.contains(pos))
    return -1;
  const int cnt = int(InventoryTabs::Count);
  int       x   = std::clamp(pos.x, tabBar.band.x, tabBar.band.x+tabBar.band.w-1) - tabBar.band.x;
  for(int t=0;t<cnt;++t) {
    const int w = tabBar.cellW + (t<tabBar.extra ? 1 : 0);
    if(x<w)
      return int8_t(t);
    x -= w;
    }
  return int8_t(cnt-1);
  }

void InventoryMenu::switchTab(size_t tab) {
  if(!isTabsMode() || tab>=InventoryTabs::Count || tab==tabs.active())
    return;

  tabLocal[tabs.active()] = pageLocal[1];
  tabs.setActive(tab);
  pageLocal[1] = tabLocal[tab];

  if(pageOth!=nullptr)
    page = 1; // the tab bar belongs to the player grid
  adjustScroll();
  Gothic::inst().emitGlobalSound("INV_CHANGE");
  }

void InventoryMenu::cycleTab(int dir) {
  if(!isTabsMode() || dir==0)
    return;
  const int cnt = int(InventoryTabs::Count);
  const int id  = (int(tabs.active()) + cnt + dir%cnt) % cnt;
  switchTab(size_t(id));
  }

void InventoryMenu::takeAllItems() {
  if(pageOth==nullptr || player==nullptr)
    return;
  if(state!=State::Chest && state!=State::Ransack)
    return;

  // collect first: moving items invalidates the iterator
  std::vector<std::pair<size_t,size_t>> items;
  for(auto it = pageOth->iterator(); it.isValid(); ++it)
    items.emplace_back(it->clsId(), it.count());
  for(auto& i:items) {
    if(state==State::Chest && chest!=nullptr)
      player->addItem(i.first, *chest, i.second);
    else if(state==State::Ransack && trader!=nullptr)
      player->addItem(i.first, *trader, i.second);
    }
  adjustScroll();
  }

void InventoryMenu::updateCursor() {
  // Only the hovered widget's shape reaches the OS cursor (Tempest applies it in
  // EventDispatcher::implSetMouseOver), so this is a best-effort hint: the arrow appears on the
  // first mouse move over the grid, which is exactly when the player can act.
  const bool arrow = isMouseMode();
  setCursorShape(arrow ? CursorShape::Arrow : CursorShape::Hidden);
  }

void InventoryMenu::drawTabs(Painter& p, const Rect& strip, const Page& /*page*/) {
  if(!isTabsMode())
    return;

  const float  scale = Gothic::interfaceScale(this);
  const size_t cnt   = size_t(InventoryTabs::Count);
  if(strip.w<=0 || strip.h<=0)
    return;

  UiDraw::panel(p,strip,tex,0.82f,true);

  // The cells are contiguous and fill the strip exactly: the band is inset by the panel padding (so
  // the cells line up with the grid below) and the leftover pixels of the integer division are spread
  // over the first cells. Contiguous cells also mean a click anywhere in the bar belongs to a tab.
  // In a trade window the strip spans both panels, so the band itself is capped and centred.
  const int pp     = panelPad();
  const int maxTab = int(68*scale);
  const int bandW  = std::min(strip.w - 2*pp, maxTab*int(cnt));
  if(bandW<=0 || strip.h<=0)
    return;
  const int bandX = strip.x + (strip.w - bandW)/2;
  const int ty    = strip.y + int(4*scale);
  const int th    = strip.h - int(8*scale);
  if(th<=0)
    return;

  tabBar.strip = strip;
  tabBar.band  = Rect(bandX,ty,bandW,th);
  tabBar.cellW = bandW / int(cnt);
  tabBar.extra = bandW - tabBar.cellW*int(cnt);
  if(tabBar.cellW<=0)
    return;

  // empty categories keep their glyph, dimmed: an empty tab reads as "nothing here", not as a hole
  bool has[InventoryTabs::Count] = {};
  if(player!=nullptr)
    tabs.categories(player->inventory(), has);

  int  x = bandX;
  Rect hoverRect;
  for(size_t t=0;t<cnt;++t) {
    const int  w = tabBar.cellW + (int(t)<tabBar.extra ? 1 : 0);
    const Rect r(x,ty,w,th);
    const bool active = t==tabs.active();
    const bool hover  = int8_t(t)==tabHover;
    const Texture2d* bt = (active || hover) ? selT : slot;
    if(bt!=nullptr) {
      p.setBrush(*bt);
      p.drawRect(r.x,r.y,r.w,r.h, 0,0,bt->w(),bt->h());
      }

    const int m = std::max(int(3*scale), std::min(w,th)/10);
    const int s = std::min(w,th) - 2*m;
    if(s>0) {
      const Rect  ir(r.x+(r.w-s)/2, r.y+(r.h-s)/2, s, s);
      const float alpha = !has[t] ? 0.28f : (active || hover ? 1.f : 0.86f);
      InventoryTabs::paintGlyph(p, t, ir, alpha);
      }

    if(active) {
      const int uw = int(3*scale);
      UiDraw::fill(p,Rect(r.x+int(6*scale),r.y+r.h-uw,r.w-2*int(6*scale),uw),UiDraw::Theme::accent());
      } else
    if(hover) {
      UiDraw::outline(p,r,UiDraw::Theme::accentSoft(0.7f),std::max(1,int(2*scale)));
      }

    if(hover)
      hoverRect = r;
    x += w;
    }

  // hovered label: the bar is icon-only, so the name appears above it instead of eating grid width
  if(tabHover>=0 && hoverRect.w>0) {
    const auto  label = InventoryTabs::label(size_t(tabHover));
    auto&       fnt   = Resources::font(scale);
    const auto  sz    = fnt.textSize(label);
    const int   pad   = int(8*scale);
    const int   lw    = sz.w + 2*pad;
    const int   lh    = sz.h + int(6*scale);
    int lx = hoverRect.x + (hoverRect.w-lw)/2;
    int ly = strip.y - lh - int(4*scale);
    if(ly<0)
      ly = strip.y + strip.h + int(4*scale);
    lx = std::clamp(lx,4,std::max(4,w()-lw-4));
    const Rect lr(lx,ly,lw,lh);
    UiDraw::tooltipPlane(p,lr,tex);
    UiDraw::textCenter(p,fnt,lr,label);
    }
  }

void InventoryMenu::drawTakeAll(Painter& p, const Rect& header) {
  if(state!=State::Chest && state!=State::Ransack)
    return;
  if(pageOth==nullptr || pageOth->size()==0)
    return;

  const float scale = Gothic::interfaceScale(this);
  auto&       fnt   = Resources::font(scale);
  const int   pad   = int(8*scale);
  const int   bw    = fnt.textSize("Take all").w + 2*pad;
  const int   bh    = header.h - int(8*scale);
  const Rect  r(header.x+header.w-bw-int(8*scale), header.y+int(4*scale), bw,bh);

  if(tex!=nullptr) {
    p.setBrush(*tex);
    p.drawRect(r.x,r.y,r.w,r.h, 0,0,tex->w(),tex->h());
    }
  if(takeAllHover) {
    UiDraw::fill(p,r,UiDraw::Theme::accentSoft(0.22f));
    UiDraw::outline(p,r,UiDraw::Theme::accentSoft(0.9f),std::max(1,int(2*scale)));
    } else
  if(selT!=nullptr) {
    p.setBrush(Brush(*selT,Color(1.f,1.f,1.f,0.55f)));
    p.drawRect(r.x,r.y,r.w,r.h, 0,0,selT->w(),selT->h());
    }

  UiDraw::textCenter(p,fnt,r,"Take all");
  takeAll.rect  = r;
  takeAll.valid = true;
  }
