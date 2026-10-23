#include "inventorymenu.h"

#include <Tempest/Painter>
#include <Tempest/SoundEffect>

#include <algorithm>
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
  takeTimer.timeout.bind(this,&InventoryMenu::onTakeStuff);
  }

InventoryMenu::~InventoryMenu() {
  }

void InventoryMenu::close() {
  if(state!=State::Closed) {
    if(state==State::Trade)
      Gothic::inst().emitGlobalSound("TRADE_CLOSE"); else
      Gothic::inst().emitGlobalSound("INV_CLOSE");
    }
  renderer.reset(true);
  takeTimer.stop();
  state  = State::Closed;
  gridsCount = 0;
  tabAreasCount = 0;
  tabHover = -1;
  takeAll.valid = false;
  takeAllHover  = false;
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
  adjustScroll();
  update();
  updateCursor();

  Gothic::inst().emitGlobalSound("INV_OPEN");
  //Gothic::inst().emitGlobalSound("INV_CHANGE");
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
  pageOth.reset(new TradePage(tr.inventory(), nullptr));
  pageLocal[1] = tabLocal[tabs.active()];
  adjustScroll();
  update();
  updateCursor();
  Gothic::inst().emitGlobalSound("TRADE_OPEN");
  }

bool InventoryMenu::ransack(Npc &pl, Npc &tr) {
  if(pl.isDown())
    return false;
  auto it = tr.inventory().iterator(Inventory::T_Ransack);
  if(!it.isValid())
    return false;
  state  = State::Ransack;
  player = &pl;
  trader = &tr;
  chest  = nullptr;
  page   = 0;
  pagePl .reset(new InvPage    (pl.inventory(), inventoryFilter()));
  pageOth.reset(new RansackPage(tr.inventory(), nullptr));
  pageLocal[1] = tabLocal[tabs.active()];
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
  pageOth.reset(new InvPage(ch.inventory(), nullptr));
  pageLocal[1] = tabLocal[tabs.active()];
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

void InventoryMenu::onWorldChanged() {
  close();
  player = nullptr;
  trader = nullptr;
  chest  = nullptr;
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

    auto it = trader->inventory().iterator(Inventory::T_Ransack);
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

void InventoryMenu::mouseDownEvent(MouseEvent &e) {
  if(player==nullptr || state==State::Closed) {
    e.ignore();
    return;
    }

  if(state==State::LockPicking)
    return;

  if(isMouseMode() && e.button==MouseEvent::ButtonLeft) {
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
    // act on the slot under the cursor, also when hover already selected it
    if(selectAt(e.pos()))
      onItemAction(Item::NSLOT);
    adjustScroll();
    update();
    return;
    }

  if (e.button==MouseEvent::ButtonLeft)
    onItemAction(Item::NSLOT);
  else if (e.button==MouseEvent::ButtonRight)
    close();

  adjustScroll();
  }

void InventoryMenu::mouseUpEvent(MouseEvent&) {
  takeTimer.stop();
  takeCount=0;
  }

void InventoryMenu::mouseWheelEvent(MouseEvent &e) {
  if(state==State::Closed) {
    e.ignore();
    return;
    }

  if(state==State::LockPicking)
    return;

  if(isMouseMode()) {
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

size_t InventoryMenu::rowsCount() const {
  const int iy = gridTop();
  return size_t(std::max(1,(h()-iy-infoHeight()-int(20*Gothic::interfaceScale(this)))/slotSize().h));
  }

// fork: fixed offsets used to be raw pixels, which breaks the layout on a high-DPI window
// (the header overlapped the top row). Scale them like the cells and the font.
int InventoryMenu::gridTop() const {
  return int(134*Gothic::interfaceScale(this));
  }

int InventoryMenu::padd() const {
  return int(43*Gothic::interfaceScale(this));
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
  auto& page = activePage();
  auto& sel  = activePageSel();
  sel.sel = std::min(sel.sel, std::max<size_t>(page.size(),1)-1);
  while(sel.sel<sel.scroll*columsCount) {
    if(sel.scroll<=1){
      sel.scroll=0;
      return;
      }
    sel.scroll-=1;
    }

  const size_t hcount=rowsCount();
  while(sel.sel>=(sel.scroll+hcount)*columsCount) {
    sel.scroll+=1;
    }
  }

void InventoryMenu::drawAll(Painter &p, Npc &player, DrawPass pass) {
  const int padd = this->padd();
  const int iy   = gridTop();
  const int hy   = int(70*Gothic::interfaceScale(this));

  if(pass==DrawPass::Back) {
    gridsCount    = 0;
    tabAreasCount = 0;
    takeAll.valid = false;
    }

  if(state==State::LockPicking)
    return;

  const int wcount = int(columsCount);
  const int hcount = int(rowsCount());

  if(chest!=nullptr){
    if(pass==DrawPass::Back) {
      drawHeader(p,chest->displayName(),padd,hy);
      drawTakeAll(p,padd,hy,wcount*slotSize().w);
      }
    drawItems(p,pass,*pageOth,pageLocal[0],padd,iy,wcount,hcount);
    }

  if(trader!=nullptr) {
    if(pass==DrawPass::Back) {
      drawHeader(p,trader->displayName(),padd,hy);
      if(state==State::Ransack)
        drawTakeAll(p,padd,hy,wcount*slotSize().w);
      }
    drawItems(p,pass,*pageOth,pageLocal[0],padd,iy,wcount,hcount);
    }

  if(state!=State::Ransack) {
    const int plX = w()-padd-wcount*slotSize().w;
    if(pass==DrawPass::Back) {
      drawGold (p,player,w()-padd-2*slotSize().w,hy);
      drawTabs (p,*pagePl,plX,wcount*slotSize().w);
      }
    drawItems(p,pass,*pagePl,pageLocal[1],plX,iy,wcount,hcount);
    }

  if(pass==DrawPass::Back)
    drawInfo(p);
  }

void InventoryMenu::drawItems(Painter &p, DrawPass pass,
                              const Page &inv, const PageLocal& sel, int x0, int y, int wcount, int hcount) {
  if(state==State::LockPicking)
    return;

  if(tex!=nullptr && pass==DrawPass::Back) {
    p.setBrush(*tex);
    p.drawRect(x0,y,slotSize().w*wcount,slotSize().h*hcount,
               0,0,tex->w(),tex->h());
    }

  if(pass==DrawPass::Back && gridsCount<std::size(grids)) {
    auto& g  = grids[gridsCount++];
    g.rect   = Rect(x0,y,slotSize().w*wcount,slotSize().h*hcount);
    g.side   = uint8_t(&sel==&pageLocal[0] ? 0 : 1);
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
      const int x = x0 + r*slotSize().w;
      if(pass==DrawPass::Back) {
        p.setBrush(*slot);
        p.drawRect(x,y,slotSize().w,slotSize().h,
                   0,0,slot->w(),slot->h());
        }
      if(it.isValid()) {
        drawSlot(p,pass, it,inv,sel, x,y, id);
        ++it;
        ++id;
        }
      }
    y += slotSize().h;
    }
  }

void InventoryMenu::drawSlot(Painter &p, DrawPass pass, const Inventory::Iterator &it,
                             const Page& page, const PageLocal &sel,
                             int x, int y, size_t id) {
  if(!slot)
    return;

  auto& active = activePage();
  const float scale = Gothic::interfaceScale(this);

  if(pass==DrawPass::Back) {
    if((!it.isValid() && id==0) || (id==sel.sel && &page==&active)){
      p.setBrush(*selT);
      p.drawRect(x,y,slotSize().w,slotSize().h,
                 0,0,selT->w(),selT->h());
      }

    if(it.isEquipped() && selU!=nullptr) {
      p.setBrush(*selU);
      p.drawRect(x,y,slotSize().w,slotSize().h,
                 0,0,selU->w(),selU->h());
      }

    const int dsz = (id==sel.sel ? 5 : 0);
    renderer.drawItem(x-dsz, y-dsz, slotSize().w+2*dsz, slotSize().h+2*dsz, *it);
    } else {
    auto fnt = Resources::font(scale);

    if(it.count()>1) {
      string_frm vint(int(it.count()));
      auto sz = fnt.textSize(vint);
      fnt.drawText(p,x+slotSize().w-sz.w-10,
                   y+slotSize().h-10,
                   vint);
      }

    if(it.slot()!=Item::NSLOT) {
      fnt = Resources::font(Resources::FontType::Red, scale);

      string_frm vint(int(it.slot()));
      auto sz = fnt.textSize(vint);
      fnt.drawText(p,x+10,
                   y+slotSize().h/2+sz.h/2,
                   vint);
      }
    }
  }

void InventoryMenu::drawGold(Painter &p, Npc &player, int x, int y) {
  if(!slot)
    return;
  auto           w    = Gothic::inst().world();
  auto           txt  = w ? w->script().currencyName() : "";
  const size_t   gold = player.inventory().goldCount();
  if(txt.empty())
    txt="Gold";

  string_frm vint(txt," : ",int(gold));
  drawHeader(p,vint,x,y);
  }

void InventoryMenu::drawHeader(Painter &p, std::string_view title, int x, int y) {
  const float scale = Gothic::interfaceScale(this);
  auto&       fnt   = Resources::font(scale);

  const int   tw    = fnt.textSize(title).w;
  const int   th    = fnt.textSize(title).h;
  const int   padd  = int(8*scale);
  const int   dw    = std::max(slotSize().w*2, tw+padd*2);
  const int   dh    = int(34*scale);

  if(tex) {
    p.setBrush(*tex);
    p.drawRect(x,y,dw,dh, 0,0,tex->w(),tex->h());
    }
  if(slot) {
    p.setBrush(*slot);
    p.drawRect(x,y,dw,dh, 0,0,slot->w(),slot->h());
    }

  fnt.drawText(p,x+(dw-tw)/2,y+dh/2+th/2,title);
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
  if(tex) {
    p.setBrush(*tex);
    p.drawRect(x,y,dw,dh,
               0,0,tex->w(),tex->h());
    }

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
  if(cursorShape()!=CursorShape::Arrow)
    setCursorShape(CursorShape::Arrow);

  const int8_t tab   = tabAt(e.pos());
  const bool   take  = takeAll.valid && takeAll.rect.contains(e.pos());
  bool         dirty = tab!=tabHover || take!=takeAllHover;
  tabHover     = tab;
  takeAllHover = take;

  if(selectAt(e.pos()) || dirty)
    update();
  }

void InventoryMenu::mouseDragEvent(MouseEvent& e) {
  // no hover while a button is held: holding the button keeps taking from the clicked slot
  if(!isMouseMode())
    e.ignore();
  }

bool InventoryMenu::isMouseMode() const {
  if(player==nullptr || state==State::Closed || state==State::LockPicking)
    return false;
  return Gothic::settingsGetI("GAME","invMouse")!=0;
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

bool InventoryMenu::selectAt(const Point& pos) {
  auto g  = gridAt(pos);
  auto pg = g!=nullptr ? sidePage(g->side) : nullptr;
  if(pg==nullptr || g->wcount==0 || g->hcount==0)
    return false;

  const size_t col = std::min(size_t((pos.x-g->rect.x)/slotSize().w), g->wcount-1);
  const size_t row = std::min(size_t((pos.y-g->rect.y)/slotSize().h), g->hcount-1);
  auto&        sel = pageLocal[g->side];
  const size_t id  = (sel.scroll+row)*g->wcount + col;
  if(id>=pg->size())
    return false;

  if(pageOth!=nullptr)
    page = g->side;
  sel.sel = id;
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
  for(size_t i=0; i<tabAreasCount; ++i)
    if(tabAreas[i].rect.contains(pos))
      return int8_t(tabAreas[i].tab);
  return -1;
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

void InventoryMenu::drawTabs(Painter& p, const Page& page, int x0, int w) {
  if(!isTabsMode())
    return;

  const float  scale = Gothic::interfaceScale(this);
  const int    gap   = int(3*scale);
  const size_t cnt   = size_t(InventoryTabs::Count);
  // the free band between the header row and the grid: the bar lives there, exactly as wide as the grid
  const int    by    = int(70*scale) + int(34*scale) + int(2*scale);
  const int    bh    = gridTop() - by - int(2*scale);
  const int    tw    = int((w - int(cnt-1)*gap) / int(cnt));
  if(bh<=0 || tw<=0)
    return;

  // empty categories keep their glyph, dimmed: an empty tab reads as "nothing here", not as a hole
  bool has[InventoryTabs::Count] = {};
  if(const Inventory* inv = page.inventory(); inv!=nullptr) {
    for(auto it = inv->iterator(Inventory::T_Inventory); it.isValid(); ++it) {
      for(size_t t=0; t<cnt; ++t)
        if(InventoryTabs::matches(t,*it))
          has[t] = true;
      }
    }

  // one INV_BACK panel behind the whole bar: INV_SLOT alone is translucent in the middle, so without
  // this the world shows through the tabs (and only the empty ones, which used INV_BACK, looked solid)
  if(tex!=nullptr) {
    p.setBrush(*tex);
    p.drawRect(x0, by, tw*int(cnt) + gap*int(cnt-1), bh, 0,0,tex->w(),tex->h());
    }

  int x = x0;
  for(size_t t=0; t<cnt; ++t) {
    const Rect r(x,by,tw,bh);
    const bool active = t==tabs.active();
    const bool hover  = int8_t(t)==tabHover;
    if(slot!=nullptr && selT!=nullptr) {
      // every tab looks like a grid cell; the active one is the bordered slot, empty ones only dim the glyph
      p.setBrush(active ? *selT : *slot);
      p.drawRect(r.x,r.y,r.w,r.h, 0,0,slot->w(),slot->h());
      }

    const int m = std::max(int(3*scale), std::min(tw,bh)/10);
    const int s = std::min(tw,bh) - 2*m;
    if(s>0) {
      const Rect  ir(r.x+(r.w-s)/2, r.y+(r.h-s)/2, s, s);
      const float alpha = !has[t] ? 0.28f : (active || hover ? 1.f : 0.86f);
      InventoryTabs::paintGlyph(p, t, ir, alpha);
      }

    if(tabAreasCount<std::size(tabAreas))
      tabAreas[tabAreasCount++] = TabArea{r,uint8_t(t)};
    x += tw + gap;
    }
  }

void InventoryMenu::drawTakeAll(Painter& p, int x, int y, int w) {
  if(state!=State::Chest && state!=State::Ransack)
    return;
  if(pageOth==nullptr || pageOth->size()==0)
    return;

  const float scale = Gothic::interfaceScale(this);
  auto&       fnt   = Resources::font(scale);
  const int   pad   = int(8*scale);
  const int   bh    = int(34*scale);
  const int   bw    = fnt.textSize("Take all").w + 2*pad;
  Rect r(x+w-bw, y, bw, bh);

  if(tex!=nullptr) {
    p.setBrush(*tex);
    p.drawRect(r.x,r.y,r.w,r.h, 0,0,tex->w(),tex->h());
    }
  if(selT!=nullptr && takeAllHover) {
    p.setBrush(*selT);
    p.drawRect(r.x,r.y,r.w,r.h, 0,0,selT->w(),selT->h());
    }
  const auto sz = fnt.textSize("Take all");
  fnt.drawText(p, r.x+(r.w-sz.w)/2, r.y+r.h/2+sz.h/2, "Take all");
  takeAll.rect  = r;
  takeAll.valid = true;
  }
