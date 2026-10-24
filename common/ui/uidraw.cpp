#include "uidraw.h"

#include <Tempest/Painter>
#include <Tempest/Texture2d>

#include <algorithm>

#include "utils/gthfont.h"

using namespace Tempest;

namespace {

const Color accentColor   = Color(0.92f,0.78f,0.42f,1.f);
const Color shadowColor   = Color(0.f,0.f,0.f,0.55f);
const Color scrimColor    = Color(0.f,0.f,0.f,1.f);
const Color panelFillCol  = Color(0.03f,0.03f,0.04f,1.f);
const Color panelBorderCol= Color(0.62f,0.55f,0.42f,0.9f);
const Color hoverCol      = Color(1.f,0.95f,0.82f,1.f);

Color withAlpha(Color c, float a) {
  c = Color(c.r(),c.g(),c.b(),c.a()*a);
  return c;
  }

}

Color UiDraw::Theme::accent() {
  return accentColor;
  }

Color UiDraw::Theme::accentSoft(float a) {
  return Color(accentColor.r(),accentColor.g(),accentColor.b(),a);
  }

Color UiDraw::Theme::hoverFill() {
  return Color(hoverCol.r(),hoverCol.g(),hoverCol.b(),0.12f);
  }

Color UiDraw::Theme::panelFill() {
  return panelFillCol;
  }

Color UiDraw::Theme::panelBorder() {
  return panelBorderCol;
  }

Color UiDraw::Theme::shadow() {
  return shadowColor;
  }

Color UiDraw::Theme::scrim() {
  return scrimColor;
  }

Color UiDraw::Theme::textOnPanel() {
  return Color(1.f);
  }

Color UiDraw::Theme::gridLine() {
  // matches the border colour of the INV_SLOT cell texture, so the outer frame of a grid looks like
  // part of the cell table instead of a separate widget
  return Color(0.74f,0.76f,0.80f,0.55f);
  }

void UiDraw::fill(Painter& p, const Rect& r, const Color& c) {
  if(r.w<=0 || r.h<=0 || c.a()<=0.f)
    return;
  p.setBrush(c);
  p.drawRect(r.x,r.y,r.w,r.h);
  }

void UiDraw::outline(Painter& p, const Rect& r, const Color& c, int width) {
  if(r.w<=0 || r.h<=0 || width<=0)
    return;
  p.setBrush(c);
  p.drawRect(r.x,           r.y,            r.w,     width);      // top
  p.drawRect(r.x,           r.y+r.h-width,  r.w,     width);      // bottom
  p.drawRect(r.x,           r.y,            width,   r.h);        // left
  p.drawRect(r.x+r.w-width, r.y,            width,   r.h);        // right
  }

void UiDraw::shadow(Painter& p, const Rect& r, int size) {
  if(size<=0)
    return;
  p.setBrush(withAlpha(shadowColor,0.35f));
  p.drawRect(r.x+size, r.y+size, r.w, r.h);
  p.setBrush(withAlpha(shadowColor,0.30f));
  p.drawRect(r.x+size/2, r.y+size/2, r.w, r.h);
  }

void UiDraw::scrim(Painter& p, const Rect& r, float alpha) {
  if(alpha<=0.f)
    return;
  fill(p, r, withAlpha(scrimColor, std::min(alpha,1.f)));
  }

void UiDraw::panel(Painter& p, const Rect& r, const Texture2d* back, float fillAlpha, bool border) {
  if(r.w<=0 || r.h<=0)
    return;

  // The stock textures are largely translucent, so the dark fill underneath is what actually makes
  // the panel readable over the world; the texture only supplies the Gothic frame/pattern on top.
  fill(p, r, withAlpha(panelFillCol, fillAlpha));
  if(back!=nullptr && !back->isEmpty()) {
    p.setBrush(Brush(*back,Color(1.f,1.f,1.f,0.92f)));
    p.drawRect(r.x,r.y,r.w,r.h, 0,0,back->w(),back->h());
    }
  if(border) {
    outline(p,r,withAlpha(panelBorderCol,0.85f),2);
    // inner hairline: gives the frame a bevelled, less flat look
    outline(p,Rect(r.x+2,r.y+2,r.w-4,r.h-4),withAlpha(accentColor,0.20f),1);
    }
  }

void UiDraw::selection(Painter& p, const Rect& r, bool strong, int width) {
  if(r.w<=0 || r.h<=0)
    return;
  fill(p, r, withAlpha(accentColor, strong ? 0.18f : 0.10f));
  outline(p, r, withAlpha(accentColor, strong ? 0.95f : 0.55f), std::max(1,width));
  }

void UiDraw::hover(Painter& p, const Rect& r) {
  if(r.w<=0 || r.h<=0)
    return;
  fill(p, r, Theme::hoverFill());
  }

void UiDraw::tooltipPlane(Painter& p, const Rect& r, const Texture2d* back, float fillAlpha) {
  if(r.w<=0 || r.h<=0)
    return;
  shadow(p,r,int(std::max(3.f,float(r.h)*0.04f)));
  panel(p,r,back,fillAlpha,true);
  // accent hairline on top edge: reads as "this window belongs to the active UI"
  fill(p,Rect(r.x,r.y,r.w,std::max(1,r.h/120)),withAlpha(accentColor,0.55f));
  }

void UiDraw::scrollBar(Painter& p, const Rect& track, size_t first, size_t visible, size_t total) {
  if(track.w<=0 || track.h<=0 || total<=visible || visible==0)
    return;

  fill(p, track, Color(0.f,0.f,0.f,0.35f));
  const float k     = float(visible)/float(total);
  const int   thumb = std::max(int(float(track.h)*k), std::max(8,track.w*3));
  const int   maxY  = std::max(1,track.h-thumb);
  const size_t maxFirst = total>visible ? total-visible : 0;
  const int   y     = track.y + int(float(maxY)*float(std::min(first,maxFirst))/float(std::max<size_t>(maxFirst,1)));

  fill(p, Rect(track.x,y,track.w,thumb), withAlpha(accentColor,0.62f));
  }

void UiDraw::textPlate(Painter& p, const Rect& r, float alpha) {
  fill(p,r,withAlpha(shadowColor,alpha));
  }

void UiDraw::HitList::add(const Rect& r, uint32_t id) {
  if(r.w<=0 || r.h<=0)
    return;
  items.push_back(Entry{r,id});
  }

void UiDraw::HitList::add(int x, int y, int w, int h, uint32_t id) {
  add(Rect(x,y,w,h),id);
  }

int UiDraw::HitList::at(const Point& pos) const {
  for(auto& i:items)
    if(i.rect.contains(pos))
      return int(i.id);
  return -1;
  }

void UiDraw::textCenter(Painter& p, const GthFont& fnt, const Rect& r, std::string_view txt) {
  if(txt.empty())
    return;
  const auto sz = fnt.textSize(txt);
  fnt.drawText(p, r.x+(r.w-sz.w)/2, r.y+(r.h+sz.h)/2, txt);
  }

void UiDraw::textLeft(Painter& p, const GthFont& fnt, const Rect& r, std::string_view txt, int inset) {
  if(txt.empty())
    return;
  const auto sz = fnt.textSize(txt);
  fnt.drawText(p, r.x+inset, r.y+(r.h+sz.h)/2, txt);
  }

void UiDraw::textRight(Painter& p, const GthFont& fnt, const Rect& r, std::string_view txt, int inset) {
  if(txt.empty())
    return;
  const auto sz = fnt.textSize(txt);
  fnt.drawText(p, r.x+r.w-sz.w-inset, r.y+(r.h+sz.h)/2, txt);
  }
