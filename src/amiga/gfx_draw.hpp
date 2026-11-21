// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-FileCopyrightText: 2026 Ingo Ruhnke
// SPDX-License-Identifier: GPL-3.0-or-later

// graphics.library drawing on RastPorts and BitMaps (gfx_draw.cpp): typed
// calls, used by the game's os:: calls.
#pragma once

#include "amiga/bitmap.hpp"

#include <cstdint>
#include <map>
#include <vector>

namespace amiga {

class Machine;

class Drawing {
public:
  explicit Drawing(Machine& machine) : m(machine) {}

  int32_t blt_bitmap(const BitMap& src, int sx, int sy, const BitMap& dst, int dx, int dy, int w, int h,
                     uint8_t minterm, uint8_t mask);
  // `str`: host bytes (an emulated string is ram + its address).
  int32_t text_length(uint32_t rp, const uint8_t* str, int count);
  void text(uint32_t rp, const uint8_t* str, int count);
  void set_font(uint32_t rp, uint32_t font);
  void set_rast(uint32_t rp, int pen);
  void move(uint32_t rp, int x, int y);
  void draw(uint32_t rp, int x, int y);
  void rect_fill(uint32_t rp, int x0, int y0, int x1, int y1);
  int32_t read_pixel(uint32_t rp, int x, int y);  // -1 outside the bitmap
  void set_apen(uint32_t rp, int pen);
  void set_bpen(uint32_t rp, int pen);
  void set_drmd(uint32_t rp, int mode);
  void blt_mask_bitmap_rastport(const BitMap& src, int sx, int sy, uint32_t rp, int dx, int dy, int w, int h,
                                uint8_t minterm, uint32_t mask);

  // A TextFont: read once
  // from the font file's segment into a host object, named by its address;
  // the drawing code reads glyphs from it.
  struct Font {
    int ysize = 0, xsize = 0, baseline = 0, lo = 0, hi = 0, modulo = 0;
    bool proportional = false;
    std::vector<uint8_t> data;                // ysize rows of `modulo` bytes
    std::vector<uint16_t> bit0, bits;         // per glyph (hi - lo + 2: the last one for undefined characters)
    std::vector<int16_t> space, kern;         // per glyph (empty: none)
    int glyph(int c) const { return (c < lo || c > hi) ? hi - lo + 1 : c - lo; }
    bool pixel(int row, int bit) const {
      const size_t at = size_t(row) * size_t(modulo) + size_t(bit >> 3);
      return at < data.size() && ((data[at] >> (7 - (bit & 7))) & 1);
    }
  };
  const Font& font(uint32_t addr);
  // The segment [block, block + size) was unloaded: its fonts go.
  void forget_fonts(uint32_t block, uint32_t size);

private:
  Machine& m;
  std::map<uint32_t, Font> fonts_;
  struct Bits;
  void rp_plot(uint32_t rp, Bits& b, int x, int y);
  void rp_line(uint32_t rp, int x0, int y0, int x1, int y1);
  void rp_fill(uint32_t rp, int x0, int y0, int x1, int y1);
  void rp_text(uint32_t rp, const uint8_t* str, int count);
  int text_width(uint32_t font, const uint8_t* str, int count);
  void blt_planes(const Bits& s, Bits& d, int sx, int sy, int dx, int dy, int w, int h, uint8_t minterm,
                  uint8_t mask, uint32_t bltmask, int mask_bpr);
};

}  // namespace amiga
