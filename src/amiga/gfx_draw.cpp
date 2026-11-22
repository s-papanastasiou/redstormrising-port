// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-FileCopyrightText: 2026 Ingo Ruhnke
// SPDX-License-Identifier: GPL-3.0-or-later

// graphics.library RastPort drawing (Kickstart 1.3 structure layouts):
// pens and draw modes, Move/Draw, RectFill, SetRast, Read/WritePixel,
// text with the game's own TextFonts, BltBitMap, BltMaskBitMapRastPort.
//
// There are no layers (all of the game's windows are full-screen
// backdrops): drawing clips to the RastPort's BitMap.
#include "amiga/gfx_draw.hpp"

#include "amiga/hle.hpp"
#include "amiga/machine.hpp"

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace amiga {

namespace {
// TextFont
constexpr uint32_t TF_YSIZE = 20, TF_FLAGS = 23, TF_XSIZE = 24, TF_BASELINE = 26,
                   TF_LOCHAR = 32, TF_HICHAR = 33, TF_CHARDATA = 34, TF_MODULO = 38,
                   TF_CHARLOC = 40, TF_CHARSPACE = 44, TF_CHARKERN = 48;
constexpr uint8_t FPF_PROPORTIONAL = 0x20;
constexpr uint8_t JAM2 = 1, COMPLEMENT = 2, INVERSVID = 4;
}  // namespace

// A BitMap as seen by the drawing code.
struct Drawing::Bits {
  Machine& m;
  uint32_t bm;
  int bpr = 0, rows = 0, depth = 0;
  uint32_t plane[8] = {};
  Bits(Machine& mm, uint32_t b) : m(mm), bm(b) {
    if (bm) take(m.bitmap(bm));
  }
  Bits(Machine& mm, const BitMap& b) : m(mm), bm(0) { take(b); }
  void take(const BitMap& b) {
    bpr = b.bytes_per_row;
    rows = b.rows;
    depth = std::min<int>(b.depth, 8);
    for (int p = 0; p < depth; ++p) plane[p] = b.planes[p];
  }
  bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < bpr * 8 && y < rows; }
  int get(int x, int y) const {
    if (!inside(x, y)) return 0;
    const uint32_t off = uint32_t(y) * uint32_t(bpr) + uint32_t(x >> 3);
    const uint8_t bit = uint8_t(0x80 >> (x & 7));
    int v = 0;
    for (int p = 0; p < depth; ++p)
      if (plane[p] && (*m.bytes(plane[p] + off, 1) & bit)) v |= 1 << p;
    return v;
  }
  // Write `pen` to the planes selected by `mask`.
  void put(int x, int y, int pen, uint8_t mask) {
    if (!inside(x, y)) return;
    const uint32_t off = uint32_t(y) * uint32_t(bpr) + uint32_t(x >> 3);
    const uint8_t bit = uint8_t(0x80 >> (x & 7));
    for (int p = 0; p < depth; ++p) {
      if (!plane[p] || !(mask & (1 << p))) continue;
      uint8_t& b = *m.bytes(plane[p] + off, 1);
      b = (pen & (1 << p)) ? uint8_t(b | bit) : uint8_t(b & ~bit);
    }
  }
  void invert(int x, int y, uint8_t mask) {
    if (!inside(x, y)) return;
    const uint32_t off = uint32_t(y) * uint32_t(bpr) + uint32_t(x >> 3);
    const uint8_t bit = uint8_t(0x80 >> (x & 7));
    for (int p = 0; p < depth; ++p)
      if (plane[p] && (mask & (1 << p))) *m.bytes(plane[p] + off, 1) ^= bit;
  }
};

// One "foreground" pixel of a line/fill in the RastPort's draw mode.
void Drawing::rp_plot(uint32_t rp, Bits& b, int x, int y) {
  const uint8_t mode = m.rastport(rp).draw_mode, mask = m.rastport(rp).mask;
  if (mode & COMPLEMENT) b.invert(x, y, mask);
  else b.put(x, y, m.rastport(rp).fg_pen, mask);
}

void Drawing::rp_line(uint32_t rp, int x0, int y0, int x1, int y1) {
  Bits b(m, m.rastport(rp).bitmap);
  const int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  const int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    rp_plot(rp, b, x0, y0);
    if (x0 == x1 && y0 == y1) break;
    const int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void Drawing::rp_fill(uint32_t rp, int x0, int y0, int x1, int y1) {
  Bits b(m, m.rastport(rp).bitmap);
  const uint8_t mode = m.rastport(rp).draw_mode, mask = m.rastport(rp).mask;
  const uint32_t ptrn = m.rastport(rp).area_ptrn;
  const int ptrn_rows = ptrn ? 1 << std::min<int>(m.rastport(rp).area_ptsz, 8) : 0;
  for (int y = std::max(0, y0); y <= y1 && y < b.rows; ++y) {
    const uint16_t pat = ptrn ? m.r16(ptrn + uint32_t((y % ptrn_rows) * 2)) : 0xFFFF;
    for (int x = std::max(0, x0); x <= x1 && x < b.bpr * 8; ++x) {
      const bool fg = (pat >> (15 - (x & 15))) & 1;
      if (mode & COMPLEMENT) { if (fg) b.invert(x, y, mask); continue; }
      if (fg) b.put(x, y, m.rastport(rp).fg_pen, mask);
      else if (mode & JAM2) b.put(x, y, m.rastport(rp).bg_pen, mask);
    }
  }
}

const Drawing::Font& Drawing::font(uint32_t addr) {
  auto it = fonts_.find(addr);
  if (it != fonts_.end()) return it->second;
  Font f;
  f.ysize = m.r16(addr + TF_YSIZE);
  f.xsize = m.r16(addr + TF_XSIZE);
  f.baseline = m.r16(addr + TF_BASELINE);
  f.lo = m.r8(addr + TF_LOCHAR);
  f.hi = m.r8(addr + TF_HICHAR);
  f.modulo = m.r16(addr + TF_MODULO);
  f.proportional = m.r8(addr + TF_FLAGS) & FPF_PROPORTIONAL;
  const uint32_t data = m.r32(addr + TF_CHARDATA), loc = m.r32(addr + TF_CHARLOC);
  const uint32_t space = m.r32(addr + TF_CHARSPACE), kern = m.r32(addr + TF_CHARKERN);
  const int glyphs = f.hi - f.lo + 2;
  f.data.resize(size_t(f.ysize) * size_t(f.modulo));
  for (size_t i = 0; i < f.data.size(); ++i) f.data[i] = m.r8(data + uint32_t(i));
  for (int g = 0; g < glyphs; ++g) {
    f.bit0.push_back(m.r16(loc + uint32_t(g) * 4));
    f.bits.push_back(m.r16(loc + uint32_t(g) * 4 + 2));
    if (space) f.space.push_back(int16_t(m.r16(space + uint32_t(g) * 2)));
    if (kern) f.kern.push_back(int16_t(m.r16(kern + uint32_t(g) * 2)));
  }
  return fonts_.emplace(addr, std::move(f)).first->second;
}

void Drawing::forget_fonts(uint32_t block, uint32_t size) {
  std::erase_if(fonts_, [&](const auto& f) { return f.first >= block && f.first < block + size; });
}

int Drawing::text_width(uint32_t addr, const uint8_t* str, int count) {
  if (!addr) return 0;
  const Font& f = font(addr);
  int w = 0;
  for (int i = 0; i < count; ++i) {
    const int g = f.glyph(str[i]);  // undefined -> default glyph
    if (f.proportional && !f.space.empty()) w += f.space[size_t(g)];
    else w += f.xsize;
    if (f.proportional && !f.kern.empty()) w += f.kern[size_t(g)];
  }
  return w;
}

// Text(rp, string, count): glyphs from the font's CharData/CharLoc, top at
// cp_y - baseline; advances cp_x.
void Drawing::rp_text(uint32_t rp, const uint8_t* str, int count) {
  const uint32_t addr = m.rastport(rp).font;
  if (!addr) return;
  const Font& f = font(addr);
  Bits b(m, m.rastport(rp).bitmap);
  const uint8_t mode = m.rastport(rp).draw_mode, mask = m.rastport(rp).mask;
  int fg = m.rastport(rp).fg_pen, bg = m.rastport(rp).bg_pen;
  if (mode & INVERSVID) std::swap(fg, bg);
  const int spacing = m.rastport(rp).tx_spacing;
  const int top = m.rastport(rp).cp_y - f.baseline;
  // As graphics.library's Text: the string is a template of character
  // cells, each `kern + space` pixels wide (its glyph `kern` pixels in, a
  // glyph wider than its cell running into the next); JAM2 paints the
  // background over the cells wherever no glyph pixel is set.
  struct Cell {
    int x, kx, adv, bit0, bits;
  };
  std::vector<Cell> cells;
  int x = m.rastport(rp).cp_x;
  for (int i = 0; i < count; ++i) {
    const size_t g = size_t(f.glyph(str[i]));
    const int kx = (f.proportional && !f.kern.empty()) ? f.kern[g] : 0;
    const int adv = (f.proportional && !f.space.empty()) ? f.space[g] : f.xsize;
    cells.push_back({x, kx, adv, f.bit0[g], f.bits[g]});
    x += adv + kx + spacing;
  }
  auto glyph_at = [&](int dx, int y) {
    for (const Cell& c : cells) {
      const int px = dx - (c.x + c.kx);
      if (px >= 0 && px < c.bits && f.pixel(y, c.bit0 + px)) return true;
    }
    return false;
  };
  for (const Cell& c : cells) {
    const int from = std::min(c.x, c.x + c.kx);
    const int to = std::max(c.x + c.kx + c.adv, c.x + c.kx + c.bits);
    for (int y = 0; y < f.ysize; ++y)
      for (int dx = from; dx < to; ++dx) {
        const bool inside_cell = dx >= c.x && dx < c.x + c.kx + c.adv;
        const bool on = glyph_at(dx, y);
        const int dy = top + y;
        if (mode & COMPLEMENT) {
          // (each glyph pixel once: by the cell its glyph starts in)
          const int px = dx - (c.x + c.kx);
          if (px >= 0 && px < c.bits && f.pixel(y, c.bit0 + px)) b.invert(dx, dy, mask);
          continue;
        }
        if (on) b.put(dx, dy, fg, mask);
        else if ((mode & JAM2) && inside_cell) b.put(dx, dy, bg, mask);
      }
  }
  m.rastport(rp).cp_x = int16_t(x);
}

// Minterm over A (mask), B (source), C (destination) bits.
static inline bool minterm(uint8_t lf, bool a, bool b, bool c) {
  return (lf >> ((a ? 4 : 0) | (b ? 2 : 0) | (c ? 1 : 0))) & 1;
}

// BltBitMap / BltMaskBitMapRastPort core: copy a rectangle plane by plane
// with minterm `lf` (A = mask plane bit or 1, B = source, C = destination),
// only in the planes selected by `planes`. Reads the source first, so
// overlapping rectangles behave like the blitter's ascending/descending
// modes.
void Drawing::blt_planes(const Bits& s, Bits& d, int sx, int sy, int dx, int dy,
                     int w, int h, uint8_t lf, uint8_t planes, uint32_t mask_plane,
                     int mask_bpr) {
  if (w <= 0 || h <= 0) return;
  std::vector<uint8_t> src(size_t(w) * size_t(h));
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) src[size_t(y) * size_t(w) + size_t(x)] = uint8_t(s.get(sx + x, sy + y));
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      const int tx = dx + x, ty = dy + y;
      if (!d.inside(tx, ty)) continue;
      bool a = true;
      if (mask_plane) {
        const int mx = sx + x, my = sy + y;
        a = (*m.bytes(mask_plane + uint32_t(my * mask_bpr + (mx >> 3)), 1) >> (7 - (mx & 7))) & 1;
        // BltMaskBitMapRastPort (the ROM's XOR, mask, XOR blits): the
        // destination is untouched where the mask is clear; where it is
        // set, (ABC|ABNC|ANBC) = $E0 copies the source.
        if (!a) continue;
      }
      const int sp = src[size_t(y) * size_t(w) + size_t(x)];
      const int dp = d.get(tx, ty);
      int out = dp;
      for (int p = 0; p < d.depth; ++p) {
        if (!(planes & (1 << p))) continue;
        const bool r = mask_plane && lf == 0xE0 ? ((sp >> p) & 1) : minterm(lf, a, (sp >> p) & 1, (dp >> p) & 1);
        out = r ? (out | (1 << p)) : (out & ~(1 << p));
      }
      if (out != dp) d.put(tx, ty, out, 0xFF);
    }
}

// The typed calls (game code calls these directly; the HLE glue below
// for 68000 callers).
int32_t Drawing::blt_bitmap(const BitMap& src, int sx, int sy, const BitMap& dst, int dx, int dy, int w, int h,
                            uint8_t minterm, uint8_t mask) {
  Bits s(m, src), d(m, dst);
  blt_planes(s, d, sx, sy, dx, dy, w, h, minterm, mask, 0, 0);
  return std::min<int32_t>(src.depth, dst.depth);
}
int32_t Drawing::text_length(uint32_t rp, const uint8_t* str, int count) {
  return text_width(m.rastport(rp).font, str, count & 0xFFFF);
}
void Drawing::text(uint32_t rp, const uint8_t* str, int count) { rp_text(rp, str, count & 0xFFFF); }
void Drawing::set_font(uint32_t rp, uint32_t addr) {
  if (!addr) return;
  const Font& f = font(addr);
  RastPort& r = m.rastport(rp);
  r.font = addr;
  r.tx_height = uint16_t(f.ysize);
  r.tx_width = uint16_t(f.xsize);
  r.tx_baseline = uint16_t(f.baseline);
}
void Drawing::set_rast(uint32_t rp, int pen) {
  Bits b(m, m.rastport(rp).bitmap);
  for (int y = 0; y < b.rows; ++y)
    for (int x = 0; x < b.bpr * 8; ++x) b.put(x, y, pen, 0xFF);
}
void Drawing::move(uint32_t rp, int x, int y) {
  m.rastport(rp).cp_x = int16_t(x);
  m.rastport(rp).cp_y = int16_t(y);
}
void Drawing::draw(uint32_t rp, int x, int y) {
  rp_line(rp, m.rastport(rp).cp_x, m.rastport(rp).cp_y, int16_t(x), int16_t(y));
  move(rp, x, y);
}
void Drawing::rect_fill(uint32_t rp, int x0, int y0, int x1, int y1) {
  rp_fill(rp, int16_t(x0), int16_t(y0), int16_t(x1), int16_t(y1));
}
int32_t Drawing::bitmap_pixel(const BitMap& bm, int x, int y) {
  Bits b(m, bm);
  return b.inside(x, y) ? b.get(x, y) : -1;
}
int32_t Drawing::read_pixel(uint32_t rp, int x, int y) {
  Bits b(m, m.rastport(rp).bitmap);
  return b.inside(x, y) ? b.get(x, y) : -1;
}
void Drawing::set_apen(uint32_t rp, int pen) { m.rastport(rp).fg_pen = uint8_t(pen); }
void Drawing::set_bpen(uint32_t rp, int pen) { m.rastport(rp).bg_pen = uint8_t(pen); }
void Drawing::set_drmd(uint32_t rp, int mode) { m.rastport(rp).draw_mode = uint8_t(mode); }
void Drawing::blt_mask_bitmap_rastport(const BitMap& src, int sx, int sy, uint32_t rp, int dx, int dy, int w, int h,
                                       uint8_t minterm, uint32_t mask) {
  Bits s(m, src), d(m, m.rastport(rp).bitmap);
  blt_planes(s, d, sx, sy, dx, dy, w, h, minterm, m.rastport(rp).mask, mask, src.bytes_per_row);
}

}  // namespace amiga
