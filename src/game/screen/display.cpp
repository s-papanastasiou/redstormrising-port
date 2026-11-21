// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Screen: which of the three bitmaps is drawn into and which is shown, the
// palette, the screen to front, and the mouse pointer.
#include "game/mem/os.hpp"
#include "game/mem/register.hpp"
#include "game/screen/calls.hpp"
#include "game/screen/vars.hpp"
#include "game/ui/state.hpp"

namespace game::screen {

// $3123a: draw into bitmap n from now on (the window's RastPort).
void draw_to(int16_t n) {
  ui::state().drawing_to = n;
  (ui::state().rastport)->bitmap = ui::state().bitmaps[n].addr;
}
ORIGINAL(draw_to, 0x3123a, 0x2134a, 0x23082, 0x2337a, 0x272e2);
ORIGINAL(draw_to, 0x32938, 0, 0, 0, 0);             // a wrapper with the same code
ORIGINAL(draw_to, 0, 0, 0, 0x25100, 0x290a8);       // another one

// $31280: bitmap n.
BitMap& bitmap_at(int16_t n) { return *ui::state().bitmaps[n]; }
ORIGINAL(bitmap_at, 0x31280, 0x21390, 0x230c8, 0x233c0, 0x27328);
ORIGINAL(bitmap_at, 0x31268, 0x21378, 0x230b0, 0x233a8, 0x27310);  // a copy

// $31262: the number of the bitmap drawn into.
int16_t drawing_bitmap() { return ui::state().drawing_to; }
ORIGINAL(drawing_bitmap, 0x31262, 0x21372, 0x230aa, 0x233a2, 0x2730a);

// $3125c: the number of the bitmap shown.
int16_t shown_bitmap() { return ui::state().showing; }
ORIGINAL(shown_bitmap, 0x3125c, 0x2136c, 0x230a4, 0x2339c, 0x27304);

// $31298: show bitmap n; a palette changed since the last time is copied
// into the screen's colour map first.
void show_bitmap(int16_t n) {
  ui::state().showing = n;
  Screen& s = *ui::state().screen;
  s.viewport.bitmap = ui::state().bitmaps[n].addr;
  if (ui::state().palette_changed) {
    ui::state().palette_changed = 0;
    for (int i = 0; i < kColours; ++i) s.viewport.colormap.table[i] = ui::state().palette[i];
  }
  os::make_screen(s.handle);
  os::rethink_display();
}
ORIGINAL(show_bitmap, 0x31298, 0x213a8, 0x230e0, 0x233d8, 0x27340);
ORIGINAL(show_bitmap, 0x3294a, 0, 0, 0x25112, 0x290ba);  // wrappers

// $31a4e: the palette to load at the next show_bitmap (32 $0RGB words).
void set_palette(const uint16_t* colours) {
  ui::state().palette_changed = 1;
  for (int i = 0; i < kColours; ++i) ui::state().palette[i] = colours[i];
}
void set_palette(const be16* colours) {
  uint16_t c[kColours];
  for (int i = 0; i < kColours; ++i) c[i] = colours[i];
  set_palette(c);
}
ORIGINAL_INSIDE(set_palette, 0x31a4e, 0x21b5e, 0x23896, 0x23b8e, 0x27af6);

// Red $2939c: the module's own palette at the next show_bitmap.
void default_palette() { set_palette(ui::state().module_palette); }
ORIGINAL(default_palette, 0, 0, 0, 0x253f4, 0x2939c);

// $29362 (Red, RedStart): the colour registers into the module's palette.
void read_palette() {
  for (int16_t i = 0; i < kColours; ++i) ui::state().module_palette[i] = uint16_t(colour(i, -1));
}
ORIGINAL(read_palette, 0, 0, 0, 0x253ba, 0x29362);

// $319d6: set colour register `reg` to rgb ($0RGB) at once and return it;
// with rgb < 0 return the register's colour instead.
int16_t colour(int16_t reg, int16_t rgb) {
  const Screen& s = *ui::state().screen;
  if (rgb < 0) return int16_t(os::get_rgb4(s.viewport.colormap_handle, reg));
  os::set_rgb4(s.handle + amiga::kScreenViewPort, reg, (rgb >> 8) & 15, (rgb >> 4) & 15,
               rgb & 15);
  return rgb;
}
ORIGINAL(colour, 0x319d6, 0x21ae6, 0x2381e, 0x23b16, 0x27a7e);

// $31a7e: wait for the top of the next frame.
void wait_tof() { os::wait_tof(); }
ORIGINAL(wait_tof, 0x31a7e, 0x21b8e, 0x238c6, 0x23bbe, 0x27b26);

// $3219c: the game's screen to the front, its window active.
void front_screen() {
  os::screen_to_front((ui::state().screen).addr);
  os::activate_window(ui::state().window);
}
ORIGINAL(front_screen, 0x3219c, 0x222ac, 0x23fe4, 0x242dc, 0);

// $31880: show a pointer image: the 8x8 one, the 7x7 one (its hot spot
// in the middle), anything else as 1x1 (the invisible one). The last
// visible image is remembered for restore_pointer.
void set_pointer(uint32_t image) {
  int32_t size = 1, spot = 0;
  if (image == ui::state().pointer_8x8) size = 8;
  else if (image == ui::state().pointer_7x7) {
    size = 7;
    spot = -3;
  }
  os::set_pointer(ui::state().window, image, size, size, spot, spot);
  if (image != ui::state().pointer_none) ui::state().pointer_image = image;
}
ORIGINAL(set_pointer, 0x31880, 0x21990, 0x236c8, 0x239c0, 0x27928);

// $314d0: show the last visible pointer image again.
void restore_pointer() { set_pointer(ui::state().pointer_image); }
ORIGINAL(restore_pointer, 0x314d0, 0x215e0, 0x23318, 0x23610, 0x27578);

// $314dc: hide the pointer.
void hide_pointer() { set_pointer(ui::state().pointer_none); }
ORIGINAL(hide_pointer, 0x314dc, 0x215ec, 0x23324, 0x2361c, 0x27584);

}  // namespace game::screen
