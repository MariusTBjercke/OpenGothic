#include "inventorytabs.h"

#include <Tempest/Painter>

#include <algorithm>
#include <cmath>

#include "game/constants.h"
#include "game/gamescript.h"
#include "game/inventory.h"
#include "world/objects/item.h"
#include "world/world.h"
#include "gothic.h"

InventoryTabs::InventoryTabs() {
  rebuild();
  }

std::string_view InventoryTabs::label(size_t tab) {
  switch(Tab(tab)) {
    case All:      return "All";
    case Melee:    return "Melee";
    case Ranged:   return "Ranged";
    case Armor:    return "Armor";
    case Potions:  return "Potions";
    case Food:     return "Food";
    case Magic:    return "Magic";
    case Writings: return "Writings";
    case Trophies: return "Trophies";
    case Keys:     return "Keys";
    case Other:    return "Other";
    case Count:    break;
    }
  return "";
  }

std::string_view InventoryTabs::instanceName(const Item& item) const {
  auto w = Gothic::inst().world();
  if(w==nullptr)
    return {};
  auto sym = w->script().findSymbol(item.clsId());
  if(sym==nullptr)
    return {};
  return sym->name();
  }

uint8_t InventoryTabs::category(const Item& item) const {
  const size_t clsId = item.clsId();
  if(const auto it = catCache.find(clsId); it!=catCache.end())
    return it->second;

  const uint32_t main = uint32_t(item.mainFlag());
  uint8_t        cat  = Other;

  // name first for the two families that flags do not describe; a trophy/keys item may carry any flag
  const auto name = instanceName(item);
  if(name.size()>=5 && name[0]=='I' && name[1]=='t') {
    if(name[2]=='A' && name[3]=='t' && name[4]=='_')
      cat = Trophies;
    if(name[2]=='K' && name[3]=='e' && name[4]=='_')
      cat = Keys;
    }

  if(cat==Other) {
    if((main & uint32_t(ITM_CAT_NF))!=0)
      cat = Melee;
    else if((main & (uint32_t(ITM_CAT_FF)|uint32_t(ITM_CAT_MUN)))!=0)
      cat = Ranged;
    else if((main & uint32_t(ITM_CAT_ARMOR))!=0)
      cat = Armor;
    else if((main & uint32_t(ITM_CAT_POTION))!=0)
      cat = Potions;
    else if((main & uint32_t(ITM_CAT_FOOD))!=0)
      cat = Food;
    else if((main & (uint32_t(ITM_CAT_RUNE)|uint32_t(ITM_CAT_MAGIC)))!=0)
      cat = Magic;
    else if((main & uint32_t(ITM_CAT_DOCS))!=0)
      cat = Writings;
    }

  catCache[clsId] = cat;
  return cat;
  }

void InventoryTabs::categories(const Inventory& inv, bool out[Count]) const {
  for(size_t i=0;i<size_t(Count);++i)
    out[i] = (i==size_t(All));
  for(auto it = inv.iterator(Inventory::T_Inventory); it.isValid(); ++it)
    out[category(*it)] = true;
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

void InventoryTabs::resetCache() {
  catCache.clear();
  }

void InventoryTabs::rebuild() {
  const size_t id = activeId;
  filt = [this,id](const Item& item) {
    if(id==size_t(All))
      return true;
    return category(item)==id;
    };
  }

namespace {
struct Pt {
  float x = 0;
  float y = 0;
  };

// Solid glyphs are built from the painter's triangle primitive: a fan around the centroid is
// exact for the convex and star-shaped outlines below and needs no texture. The fan is emitted
// with a consistent winding: the painter culls back-facing triangles, so a polygon authored
// clockwise would silently vanish (that is how the right page of the book went missing once).
void fillFan(Tempest::Painter& p, const Pt* pts, size_t n) {
  float area = 0;
  float cx   = 0;
  float cy   = 0;
  for(size_t i=0;i<n;++i) {
    const Pt& a = pts[i];
    const Pt& b = pts[(i+1)%n];
    area += a.x*b.y - b.x*a.y;
    cx   += a.x;
    cy   += a.y;
    }
  cx /= float(n);
  cy /= float(n);
  for(size_t i=0;i<n;++i) {
    const Pt& a = pts[i];
    const Pt& b = pts[(i+1)%n];
    if(area>=0)
      p.drawTriangle(cx,cy, 0,0, a.x,a.y, 0,0, b.x,b.y, 0,0); else
      p.drawTriangle(cx,cy, 0,0, b.x,b.y, 0,0, a.x,a.y, 0,0);
    }
  }

void fillCircle(Tempest::Painter& p, float cx, float cy, float r) {
  constexpr int N = 24;
  Pt pts[N];
  for(int i=0;i<N;++i) {
    const float a = float(i)*2.f*3.14159265f/float(N);
    pts[i] = Pt{cx + r*std::cos(a), cy + r*std::sin(a)};
    }
  fillFan(p, pts, N);
  }

// Ring: built from small radial quads, so the hole stays open (a centroid fan over an
// alternating-radius polygon would just fill the disc).
void fillRing(Tempest::Painter& p, float cx, float cy, float rOut, float rIn) {
  constexpr int N = 20;
  for(int i=0;i<N;++i) {
    const float a0 = float(i  )*2.f*3.14159265f/float(N);
    const float a1 = float(i+1)*2.f*3.14159265f/float(N);
    Pt quad[4] = {
      Pt{cx + rOut*std::cos(a0), cy + rOut*std::sin(a0)},
      Pt{cx + rOut*std::cos(a1), cy + rOut*std::sin(a1)},
      Pt{cx + rIn *std::cos(a1), cy + rIn *std::sin(a1)},
      Pt{cx + rIn *std::cos(a0), cy + rIn *std::sin(a0)},
      };
    fillFan(p, quad, 4);
    }
  }

constexpr size_t polyMax = 24;

struct Points {
  Pt     v[polyMax];
  size_t n = 0;
  void add(float x, float y) { if(n<polyMax) v[n++] = Pt{x,y}; }
  };

void fillPoints(Tempest::Painter& p, const Points& q) {
  fillFan(p, q.v, q.n);
  }
}

void InventoryTabs::paintGlyph(Tempest::Painter& p, size_t tab, const Tempest::Rect& r, float alpha) {
  // glyphs are authored in a 64x64 box and scaled to the square tab icon
  const float sx = float(r.w)/64.f;
  const float sy = float(r.h)/64.f;
  auto X = [&](float x) { return int(float(r.x) + x*sx); };
  auto Y = [&](float y) { return int(float(r.y) + y*sy); };
  auto W = [&](float w) { return std::max(1, int(w*sx)); };
  auto H = [&](float h) { return std::max(1, int(h*sy)); };
  auto F = [&](float v) { return v*sx; };

  p.setBrush(Tempest::Color(0.93f,0.93f,0.93f,alpha));
  switch(Tab(tab)) {
    case All: {
      const float q = 22, g = 8;
      for(int i=0;i<2;++i)
        for(int j=0;j<2;++j)
          p.drawRect(X(float(j)*(q+g)+2), Y(float(i)*(q+g)+2), W(q), H(q));
      break;
      }
    case Melee: {
      Points blade;
      blade.add(X(32),Y(2)); blade.add(X(39),Y(16)); blade.add(X(39),Y(38));
      blade.add(X(32),Y(45)); blade.add(X(25),Y(38)); blade.add(X(25),Y(16));
      fillPoints(p,blade);
      p.drawRect(X(8), Y(38), W(48), H(6));   // guard
      p.drawRect(X(28),Y(44), W(8),  H(9));   // grip
      p.drawRect(X(22),Y(53), W(20), H(5));   // pommel
      break;
      }
    case Ranged: {
      // bow: a thick crescent with the string on its right and an arrow resting on it
      Points bow;
      for(int i=0;i<=8;++i) {
        const float a = -1.20f + 2.40f*float(i)/8.f;
        bow.add(X(36.f - 24.f*std::cos(a)), Y(32.f + 30.f*std::sin(a)));
        }
      for(int i=8;i>=0;--i) {
        const float a = -1.20f + 2.40f*float(i)/8.f;
        bow.add(X(36.f - 12.f*std::cos(a)), Y(32.f + 30.f*std::sin(a)));
        }
      fillPoints(p,bow);
      p.drawRect(X(26),Y(3), W(3), H(58));    // string
      p.drawRect(X(12),Y(30), W(36), H(4));   // arrow shaft
      Points tip;
      tip.add(X(46),Y(24)); tip.add(X(58),Y(32)); tip.add(X(46),Y(40));
      fillPoints(p,tip);
      break;
      }
    case Armor: {
      Points shield;
      shield.add(X(6), Y(6));  shield.add(X(58),Y(6));
      shield.add(X(58),Y(32)); shield.add(X(32),Y(58)); shield.add(X(6),Y(32));
      fillPoints(p,shield);
      break;
      }
    case Potions: {
      p.drawRect(X(24),Y(2), W(16), H(7));    // cork
      Points flask;
      flask.add(X(27),Y(9));  flask.add(X(37),Y(9));  flask.add(X(37),Y(22));
      flask.add(X(53),Y(48)); flask.add(X(53),Y(56)); flask.add(X(11),Y(56));
      flask.add(X(11),Y(48)); flask.add(X(27),Y(22));
      fillPoints(p,flask);
      break;
      }
    case Food: {
      fillCircle(p, float(X(32)), float(Y(38)), float(W(24)));
      p.drawRect(X(29),Y(6), W(5), H(12));    // stem
      Points leaf;
      leaf.add(X(34),Y(10)); leaf.add(X(54),Y(4)); leaf.add(X(56),Y(12)); leaf.add(X(38),Y(18));
      fillPoints(p,leaf);
      break;
      }
    case Writings: {
      Points left;
      left.add(X(3),Y(12)); left.add(X(30),Y(5)); left.add(X(30),Y(50)); left.add(X(3),Y(57));
      fillPoints(p,left);
      Points right;
      right.add(X(34),Y(5)); right.add(X(61),Y(12)); right.add(X(61),Y(57)); right.add(X(34),Y(50));
      fillPoints(p,right);
      break;
      }
    case Magic: {
      Points star;
      for(int i=0;i<12;++i) {
        const float rad = (i%2==0) ? 30.f : 13.f;
        const float ang = -1.5707963f + float(i)*3.14159265f/6.f;
        star.add(X(32.f + rad*std::cos(ang)), Y(32.f + rad*std::sin(ang)));
        }
      fillPoints(p,star);
      break;
      }
    case Trophies: {
      // fang with two roots: reads as "animal part" at icon size
      p.drawRect(X(14),Y(8), W(36), H(11));
      Points left;
      left.add(X(17),Y(19)); left.add(X(30),Y(19)); left.add(X(27),Y(57));
      fillPoints(p,left);
      Points right;
      right.add(X(34),Y(19)); right.add(X(47),Y(19)); right.add(X(38),Y(57));
      fillPoints(p,right);
      break;
      }
    case Keys: {
      fillRing(p, float(X(20)), float(Y(20)), F(16.f), F(8.f));
      p.drawRect(X(28),Y(17), W(28), H(6));   // shaft
      p.drawRect(X(46),Y(20), W(5),  H(14));  // tooth
      p.drawRect(X(37),Y(20), W(5),  H(10));  // tooth
      break;
      }
    case Other: {
      for(int i=0;i<3;++i)
        fillCircle(p, X(12.f + float(i)*20.f), Y(32), 7.f*sx);
      break;
      }
    case Count:
      break;
    }
  }
