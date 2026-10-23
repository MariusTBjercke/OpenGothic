#include "inventorytabs.h"

#include <Tempest/Painter>

#include <algorithm>
#include <cmath>

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

namespace {
struct Pt {
  float x = 0;
  float y = 0;
  };

// Solid glyphs are built from the painter's triangle primitive: a fan around the centroid is
// exact for the convex and star-shaped outlines below and needs no texture.
void fillFan(Tempest::Painter& p, const Pt* pts, size_t n) {
  float cx = 0, cy = 0;
  for(size_t i=0;i<n;++i) {
    cx += pts[i].x;
    cy += pts[i].y;
    }
  cx /= float(n);
  cy /= float(n);
  for(size_t i=0;i<n;++i) {
    const Pt& a = pts[i];
    const Pt& b = pts[(i+1)%n];
    p.drawTriangle(cx,cy, 0,0, a.x,a.y, 0,0, b.x,b.y, 0,0);
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

constexpr size_t polyMax = 16;

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

  p.setBrush(Tempest::Color(0.93f,0.93f,0.93f,alpha));
  switch(Tab(tab)) {
    case All: {
      const float q = 22, g = 8;
      for(int i=0;i<2;++i)
        for(int j=0;j<2;++j)
          p.drawRect(X(float(j)*(q+g)+2), Y(float(i)*(q+g)+2), W(q), H(q));
      break;
      }
    case Weapons: {
      Points blade;
      blade.add(X(32),Y(2)); blade.add(X(39),Y(16)); blade.add(X(39),Y(38));
      blade.add(X(32),Y(45)); blade.add(X(25),Y(38)); blade.add(X(25),Y(16));
      fillPoints(p,blade);
      p.drawRect(X(8), Y(38), W(48), H(6));   // guard
      p.drawRect(X(28),Y(44), W(8),  H(9));   // grip
      p.drawRect(X(22),Y(53), W(20), H(5));   // pommel
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
      right.add(X(61),Y(12)); right.add(X(34),Y(5)); right.add(X(34),Y(50)); right.add(X(61),Y(57));
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
    case Other: {
      for(int i=0;i<3;++i)
        fillCircle(p, X(12.f + float(i)*20.f), Y(32), 7.f*sx);
      break;
      }
    case Count:
      break;
    }
  }
