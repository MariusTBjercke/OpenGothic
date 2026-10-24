#pragma once

// Fork: immediate-mode drawing helpers for the modernised UI (see docs/fork/plan-ui-overhaul.md).
//
// Every window in the fork paints itself and hit-tests what it painted. This file holds the parts
// that were copied between windows before: a dim backdrop, framed panels, hover/selection accents,
// floating tooltips, scroll indicators and a hit list. It draws with the game's own textures plus
// painter primitives, so no art assets are needed.

#include <Tempest/Rect>
#include <Tempest/Color>
#include <Tempest/Point>

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <vector>

class GthFont;
namespace Tempest {
class Painter;
class Texture2d;
class Widget;
}

namespace UiDraw {

// ---------------------------------------------------------------- theme

struct Theme final {
  // one accent for "this is active / hovered", warm gold so it fits the Gothic palette
  static Tempest::Color accent();
  static Tempest::Color accentSoft(float a);
  static Tempest::Color hoverFill();
  static Tempest::Color panelFill();
  static Tempest::Color panelBorder();
  static Tempest::Color shadow();
  static Tempest::Color scrim();       // full-window dim behind a modal window
  static Tempest::Color textOnPanel(); // glyph/text tint on top of a panel
  };

// ---------------------------------------------------------------- metrics

// Scale-aware metrics shared by the windows: one place to tune spacing.
struct Metrics final {
  float scale = 1.f;

  explicit Metrics(float s) : scale(s) {}

  int  gap()     const { return int(6.f*scale);  }
  int  pad()     const { return int(8.f*scale);  }
  int  padL()    const { return int(12.f*scale); }
  int  headerH() const { return int(34.f*scale); }
  int  rowH()    const { return int(30.f*scale); }
  int  border()  const { return std::max(1, int(2.f*scale)); }
  int  radius()  const { return std::max(2, int(4.f*scale)); }
  };

// ---------------------------------------------------------------- primitives

// Fills a rect with a flat colour (alpha blended). The everyday building block.
void fill(Tempest::Painter& p, const Tempest::Rect& r, const Tempest::Color& c);

// Draws a 1px/2px outline inside `r`.
void outline(Tempest::Painter& p, const Tempest::Rect& r, const Tempest::Color& c, int width = 1);

// Drop shadow under a floating element (offset, no blur: two nested translucent rects).
void shadow(Tempest::Painter& p, const Tempest::Rect& r, int size = 4);

// Full-window dim backdrop for modal windows. `alpha` is the strength (0 disables).
void scrim(Tempest::Painter& p, const Tempest::Rect& r, float alpha);

// A framed panel: dark fill for contrast, then the game texture on top so the Gothic material
// still shows, then an optional border. `back` may be null.
void panel(Tempest::Painter& p, const Tempest::Rect& r, const Tempest::Texture2d* back,
           float fillAlpha = 0.62f, bool border = true);

// Accent ring used for "selected" states. Drawn inside the rect.
void selection(Tempest::Painter& p, const Tempest::Rect& r, bool strong, int width = 2);

// Subtle highlight used for hover states (no border).
void hover(Tempest::Painter& p, const Tempest::Rect& r);

// A floating tooltip plate: shadow + dark panel + accent hairline at the top.
void tooltipPlane(Tempest::Painter& p, const Tempest::Rect& r, const Tempest::Texture2d* back);

// Vertical scroll indicator, drawn inside `track`. Shows the visible window of `total` rows.
void scrollBar(Tempest::Painter& p, const Tempest::Rect& track, size_t first, size_t visible, size_t total);

// Text plate: a soft dark box behind a label so it stays readable over 3D content.
void textPlate(Tempest::Painter& p, const Tempest::Rect& r, float alpha = 0.55f);

// ---------------------------------------------------------------- hit testing

// Records {rect,id} while painting and resolves a point to the id. Same idea as the inventory's
// GridArea: the mouse can only hit what was actually drawn.
class HitList final {
  public:
    struct Entry final {
      Tempest::Rect rect;
      uint32_t      id = 0;
      };

    void        reset() { items.clear(); }
    void        add(const Tempest::Rect& r, uint32_t id);
    void        add(int x, int y, int w, int h, uint32_t id);

    // -1 when nothing was hit.
    int         at(const Tempest::Point& pos) const;
    bool        contains(const Tempest::Point& pos) const { return at(pos)>=0; }

    size_t      size()  const { return items.size(); }
    bool        empty() const { return items.empty();   }
    const Entry&operator[](size_t i) const { return items[i]; }

  private:
    std::vector<Entry> items;
  };

// ---------------------------------------------------------------- text

// Centres `txt` horizontally inside `r` and draws it with the font's own colour.
void textCenter(Tempest::Painter& p, const GthFont& fnt, const Tempest::Rect& r, std::string_view txt);

// Draws `txt` left aligned, vertically centred in `r`.
void textLeft(Tempest::Painter& p, const GthFont& fnt, const Tempest::Rect& r, std::string_view txt, int inset = 0);

// Draws `txt` right aligned, vertically centred in `r`.
void textRight(Tempest::Painter& p, const GthFont& fnt, const Tempest::Rect& r, std::string_view txt, int inset = 0);

}
