// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Screen: two routines that program the blitter directly (written in
// assembler in the original, between OwnBlitter and DisownBlitter): copy a
// rectangle between bitmaps, draw a shape through its mask.
//
// All coordinates are 16-bit as in the original; rectangles are clipped to
// the destination and widened to whole words.
#include "game/mem/hardware.hpp"
#include "game/mem/os.hpp"
#include "game/mem/register.hpp"
#include "game/screen/calls.hpp"
#include "game/screen/vars.hpp"
#include "game/ui/state.hpp"

namespace game::screen {

namespace {
int16_t width_in_pixels(const BitMap& bm) { return int16_t(bm.bytes_per_row * 8); }

// Clip [pos, pos + size) to [0, limit); false if nothing is left.
bool clip(int16_t& pos, int16_t& size, int16_t limit) {
  if (pos < 0) {
    size = int16_t(size + pos);
    if (size < 1) return false;
    pos = 0;
  }
  if (pos >= limit) return false;
  if (int16_t(pos + size) > limit) size = int16_t(limit - pos);
  return true;
}

// Byte offset of (x, y) in a bitmap, as the 16-bit sum the original uses.
uint16_t offset_of(const BitMap& bm, int16_t x, int16_t y) {
  return uint16_t(uint32_t(uint16_t(y)) * bm.bytes_per_row + uint32_t(x >> 3));
}

hw::Blitter& blitter_start(uint16_t con0, uint16_t con1, uint16_t first_mask, uint16_t last_mask) {
  os::own_blitter();
  hw::Blitter& b = hw::blitter();
  b.con0 = con0;
  b.con1 = con1;
  b.afwm = first_mask;
  b.alwm = last_mask;
  b.dat[hw::Blitter::A] = b.dat[hw::Blitter::B] = b.dat[hw::Blitter::C] = 0;
  return b;
}

void blitter_end() { os::disown_blitter(); }
}  // namespace

// $2ef18: copy the rectangle (x, y, w, h) of src to the same place in dst,
// one blit per plane of src.
void copy_region(const BitMap& s, const BitMap& d, int16_t x, int16_t y, int16_t w, int16_t h) {
  if (!clip(x, w, width_in_pixels(d))) return;
  if (!clip(y, h, int16_t(d.rows))) return;
  const int16_t words = int16_t((int16_t(w + x + 15) >> 4) - (x >> 4));
  x = int16_t(x & ~15);
  const uint32_t src_offset = offset_of(s, x, y), dst_offset = offset_of(d, x, y);
  const uint16_t size = uint16_t((h << 6) + words);

  hw::Blitter& b = blitter_start(hw::BLTCON0_USEB | hw::BLTCON0_USED | kMintermCopyB, 0, hw::kAllBits, hw::kAllBits);
  b.mod[hw::Blitter::B] = int16_t(s.bytes_per_row - words * 2);
  b.mod[hw::Blitter::D] = int16_t(d.bytes_per_row - words * 2);
  for (uint16_t p = 0; p < s.depth; ++p) {
    b.pt[hw::Blitter::B] = s.planes[p] + src_offset;
    b.pt[hw::Blitter::D] = d.planes[p] + dst_offset;
    hw::blit(size);
  }
  blitter_end();
}
ORIGINAL(copy_region, 0x2ef18, 0, 0x21520, 0x21818, 0x24fac);

// $2ec9c: draw `shape` at (x, y) of dst through its mask (plane 7):
// D = mask ? shape : D, shifted by x & 15, one blit per plane of the shape.
void draw_shape(const BitMap& s, const BitMap& d, int16_t x, int16_t y) {
  int16_t rows = int16_t(s.rows), width = width_in_pixels(s);
  const int16_t shift = int16_t(x & 15);
  uint16_t first_mask = hw::kAllBits, last_mask = shift ? 0 : hw::kAllBits;
  int16_t shape_offset = 0, dst_offset = 0;
  if (x < 0) {  // clipped on the left: start with the word that has pixels
    width = int16_t(width + x);
    if (width < 1) return;
    const int16_t extra = shift ? 1 : 0;
    dst_offset = int16_t(dst_offset - extra * 2);
    width = int16_t(width + (extra << 4));
    shape_offset = int16_t(shape_offset + (int16_t(-x & ~15) >> 3));
    first_mask = first_word_masks[shift];
    x = 0;
  }
  if (x >= width_in_pixels(d)) return;
  if (int16_t(x + width) > width_in_pixels(d)) {
    width = int16_t(width_in_pixels(d) - x);
    last_mask = last_word_masks[shift];
  }
  if (y < 0) {
    rows = int16_t(rows + y);
    if (rows < 1) return;
    shape_offset = int16_t(shape_offset - uint16_t(uint32_t(uint16_t(y)) * s.bytes_per_row));
    y = 0;
  }
  if (y >= int16_t(d.rows)) return;
  if (int16_t(y + rows) > int16_t(d.rows)) rows = int16_t(d.rows - y);
  const int16_t words = int16_t((width >> 4) + (shift ? 1 : 0));
  dst_offset = int16_t(dst_offset + offset_of(d, x, y));
  const uint16_t size = uint16_t((rows << 6) + words);

  const uint16_t con1 = uint16_t(shift << 12);
  hw::Blitter& b = blitter_start(
      uint16_t(con1 + (hw::BLTCON0_USEA | hw::BLTCON0_USEB | hw::BLTCON0_USEC | hw::BLTCON0_USED | kMintermCookieCut)),
      con1, first_mask, last_mask);
  b.mod[hw::Blitter::B] = b.mod[hw::Blitter::A] = int16_t(s.bytes_per_row - words * 2);
  b.mod[hw::Blitter::D] = b.mod[hw::Blitter::C] = int16_t(d.bytes_per_row - words * 2);
  for (uint16_t p = 0; p < s.depth; ++p) {
    b.pt[hw::Blitter::A] = s.planes[kMaskPlane] + uint32_t(int32_t(shape_offset));
    b.pt[hw::Blitter::B] = s.planes[p] + uint32_t(int32_t(shape_offset));
    b.pt[hw::Blitter::C] = d.planes[p] + uint32_t(int32_t(dst_offset));
    b.pt[hw::Blitter::D] = d.planes[p] + uint32_t(int32_t(dst_offset));
    hw::blit(size);
  }
  blitter_end();
}
ORIGINAL(draw_shape, 0x2ec9c, 0, 0x212a4, 0x2159c, 0x24d30);

}  // namespace game::screen
