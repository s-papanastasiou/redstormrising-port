// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Screen: drawing into the window's RastPort (pens, lines, rectangles,
// boxes, blits, masked shapes and images).
#include "game/mem/os.hpp"
#include "game/mem/register.hpp"
#include "game/screen/calls.hpp"
#include "game/screen/vars.hpp"
#include "game/ui/state.hpp"
#include "game/screen/styles.hpp"

#include <algorithm>

namespace game::screen {

namespace {
uint32_t rp() { return (ui::state().rastport).addr; }
}  // namespace

// $312fc: the pen for lines and rectangles.
void set_apen(int16_t pen) { os::set_apen(rp(), pen); }
ORIGINAL(set_apen, 0x312fc, 0x2140c, 0x23144, 0x2343c, 0x273a4);
ORIGINAL(set_apen, 0x328da, 0, 0, 0x250a2, 0x2904a);  // wrappers

// $31226: clear the bitmap drawn into.
void clear_screen() { os::set_rast(rp(), 0); }
ORIGINAL(clear_screen, 0x31226, 0x21336, 0x2306e, 0x23366, 0x272ce);

// $3138a: a line in the current pen.
void draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
  os::move(rp(), x0, y0);
  os::draw(rp(), x1, y1);
}
ORIGINAL(draw_line, 0x3138a, 0x2149a, 0x231d2, 0x234ca, 0x27432);
ORIGINAL(draw_line, 0x3287c, 0, 0, 0x25044, 0x28fec);  // wrappers

// $328a4: a line in pen `pen`.
void line_colour(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t pen) {
  set_apen(pen);
  draw_line(x0, y0, x1, y1);
}
ORIGINAL(line_colour, 0x328a4, 0, 0, 0x2506c, 0x29014);

// $328ec: one pixel in the current pen.
void plot(int16_t x, int16_t y) { draw_line(x, y, x, y); }
ORIGINAL(plot, 0x328ec, 0, 0, 0x250b4, 0x2905c);

// $3290c: one pixel in pen `pen`.
void plot_colour(int16_t x, int16_t y, int16_t pen) {
  set_apen(pen);
  draw_line(x, y, x, y);
}
ORIGINAL(plot_colour, 0x3290c, 0, 0, 0x250d4, 0x2907c);

// $31a8c: the pen of a pixel of the bitmap drawn into.
int32_t read_pixel(int16_t x, int16_t y) { return os::read_pixel(rp(), x, y); }
ORIGINAL(read_pixel, 0x31a8c, 0x21b9c, 0x238d4, 0x23bcc, 0x27b34);

// $313c8: fill a rectangle of `bitmap` (one of the three) with the
// current pen, clipped to the screen; the bitmap drawn into is restored.
// A bitmap not in the table is drawn as number -1, as the original does.
void fill_rect(const BitMap& bitmap, int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
  int16_t n = -1;
  for (int16_t i = 0; i < kBitmaps; ++i)
    if (&bitmap_at(i) == &bitmap) n = i;
  const int16_t previous = ui::state().drawing_to;
  draw_to(n);
  os::rect_fill(rp(), std::max<int16_t>(x0, 0), std::max<int16_t>(y0, 0),
                std::min(x1, kScreenRight), std::min(y1, kScreenBottom));
  draw_to(previous);
}
ORIGINAL(fill_rect, 0x313c8, 0x214d8, 0x23210, 0x23508, 0x27470);

// $3271c: fill a rectangle of the bitmap drawn into with pen `pen`.
void fill_rect_colour(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t pen) {
  set_apen(pen);
  fill_rect(bitmap_at(drawing_bitmap()), x0, y0, x1, y1);
}
ORIGINAL(fill_rect_colour, 0x3271c, 0, 0, 0x24ee4, 0x28e8c);

// $3275e: fill a rectangle of a text style's bitmap with its foreground
// pen; $3279c the same with its background pen.
void fill_style_fg(const TextStyle& s, int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
  set_apen(int16_t(s.fg));
  fill_rect(bitmap_at(int16_t(s.bitmap)), x0, y0, x1, y1);
}
ORIGINAL(fill_style_fg, 0x3275e, 0, 0, 0x24f26, 0x28ece);
void fill_style_bg(const TextStyle& s, int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
  set_apen(int16_t(s.bg));
  fill_rect(bitmap_at(int16_t(s.bitmap)), x0, y0, x1, y1);
}
ORIGINAL(fill_style_bg, 0x3279c, 0, 0, 0x24f64, 0x28f0c);

// $3281a: the outline of a rectangle in pen `pen`.
void box(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t pen) {
  line_colour(x0, y0, x1, y0, pen);
  line_colour(x1, y0, x1, y1, pen);
  line_colour(x0, y1, x1, y1, pen);
  line_colour(x0, y0, x0, y1, pen);
}
ORIGINAL(box, 0x3281a, 0, 0, 0x24fe2, 0x28f8a);

// $327da: a rectangle cleared to pen 0 with an outline in pen `pen`.
void filled_box(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t pen) {
  fill_rect_colour(x0, y0, x1, y1, 0);
  box(x0, y0, x1, y1, pen);
}
ORIGINAL(filled_box, 0x327da, 0, 0, 0x24fa2, 0x28f4a);

// $343cc: BltBitMap (the C binding).
int32_t blt_bitmap(const BitMap& src, int32_t sx, int32_t sy, const BitMap& dst, int32_t dx, int32_t dy, int32_t w,
                   int32_t h, uint32_t minterm, uint32_t mask, uint32_t temp) {
  return os::blt_bitmap(src, sx, sy, dst, dx, dy, w, h, minterm, mask, temp);
}
ORIGINAL(blt_bitmap, 0x343cc, 0x23b78, 0x256c0, 0x288d4, 0x2f0e4);

// $343a0: BltMaskBitMapRastPort (the C binding): draw_masked calls the
// graphics layer directly.
ORIGINAL(blt_mask, 0x343a0, 0x23b44, 0x2568c, 0x288a8, 0x2f0b8);

// $3131a: copy a w x h block from one bitmap to another.
void blit(const BitMap& src, int16_t sx, int16_t sy, const BitMap& dst, int16_t dx, int16_t dy, int16_t w, int16_t h) {
  blt_bitmap(src, sx, sy, dst, dx, dy, w, h, kCopy, 0xff, 0);
}
ORIGINAL(blit, 0x3131a, 0x2142a, 0x23162, 0x2345a, 0x273c2);

// $326cc: blit between bitmaps given by number.
void blit_bitmaps(int16_t from, int16_t sx, int16_t sy, int16_t to, int16_t dx, int16_t dy, int16_t w,
                  int16_t h) {
  blit(bitmap_at(from), sx, sy, bitmap_at(to), dx, dy, w, h);
}
ORIGINAL(blit_bitmaps, 0x326cc, 0, 0, 0, 0x28e3c);

// $31d62: draw shape `shape` (loaded first unless it is the current one)
// through its mask at (x, y), centred on x if `centre`, without its
// bottom `cut` rows.
void draw_masked(int16_t shape, int16_t x, int16_t y, int16_t cut, int16_t centre) {
  if (ui::state().current_shape != shape) load_shape(shape);
  const BitMap& bm = bitmap(shape_bitmap.addr());
  const int16_t width = int16_t(bm.bytes_per_row * 8);
  const int16_t height = int16_t(bm.rows - cut);
  if (centre) x = int16_t(x - width / 2);
  os::blt_mask_bitmap_rastport(bm, 0, 0, rp(), x, y, width, height, kCookieCut, shape_mask());
}
ORIGINAL(draw_masked, 0x31d62, 0x21e72, 0x23baa, 0x23ea2, 0x27e0a);

// $3178e (Storm): draw image n of the image list through its mask at
// (x, y). An image is its width in bytes and height (signed bytes), six
// bitplanes, then the mask.
void draw_image(int16_t n, int16_t x, int16_t y) {
  ConstPtr<int8_t> image = ui::state().images;  // (the planes stay where the blitter reads them)
  for (int16_t i = 0; i < n; ++i) {
    const int16_t size = int16_t(image[0] * image[1]);
    image = image.addr + uint32_t(int32_t(int16_t(size * 7 + 2)));
  }
  const int16_t width = image[0], height = image[1];
  const int16_t plane = int16_t(width * height);
  const uint32_t planes = image.addr + 2;
  BitMap bm;
  bm.bytes_per_row = uint16_t(width);
  bm.rows = uint16_t(height);
  bm.flags = 0;
  bm.depth = 6;
  for (int16_t i = 0; i < 6; ++i) bm.planes[i] = planes + uint32_t(int32_t(int16_t(i * plane)));
  const uint32_t mask = planes + uint32_t(int32_t(int16_t(plane * 6)));
  os::blt_mask_bitmap_rastport(bm, 0, 0, rp(), x, y, int16_t(width * 8), height, kCookieCut, mask);
}
ORIGINAL(draw_image, 0x3178e, 0, 0, 0, 0);

}  // namespace game::screen
