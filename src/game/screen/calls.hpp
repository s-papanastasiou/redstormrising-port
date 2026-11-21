// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Screen: the routines of the screen library linked into every module.
#pragma once

#include <cstdint>

#include "amiga/bitmap.hpp"
#include "game/mem/types.hpp"

namespace game::screen {

struct TextStyle;
using BitMap = amiga::BitMap;

// display.cpp: bitmaps, showing, palette, pointer
void draw_to(int16_t bitmap);
BitMap& bitmap_at(int16_t bitmap);  // one of the three
int16_t drawing_bitmap();
int16_t shown_bitmap();
void show_bitmap(int16_t bitmap);
void set_palette(const uint16_t* colours);  // 32 $0RGB words
void set_palette(const mem::be16* colours);  // from constant data
void default_palette();  // RedStart, Red
void read_palette();     // RedStart, Red
int16_t colour(int16_t reg, int16_t rgb);
void wait_tof();
void front_screen();
void set_pointer(uint32_t image);
void restore_pointer();
void hide_pointer();

// draw.cpp
void set_apen(int16_t pen);
void clear_screen();
void draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
void line_colour(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t pen);
void filled_box(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t pen);  // cleared, with an outline
void plot(int16_t x, int16_t y);
void plot_colour(int16_t x, int16_t y, int16_t pen);
int32_t read_pixel(int16_t x, int16_t y);
void fill_rect(const BitMap& bitmap, int16_t x0, int16_t y0, int16_t x1, int16_t y1);
void fill_rect_colour(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t pen);
void fill_style_fg(const TextStyle& style, int16_t x0, int16_t y0, int16_t x1, int16_t y1);
void fill_style_bg(const TextStyle& style, int16_t x0, int16_t y0, int16_t x1, int16_t y1);
void box(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t pen);
int32_t blt_bitmap(const BitMap& src, int32_t sx, int32_t sy, const BitMap& dst, int32_t dx, int32_t dy, int32_t w,
                   int32_t h, uint32_t minterm, uint32_t mask, uint32_t temp);
void draw_masked(int16_t shape, int16_t x, int16_t y, int16_t cut, int16_t centre);
void draw_image(int16_t n, int16_t x, int16_t y);  // Storm: image n of the image list, through its mask
void blit_bitmaps(int16_t from, int16_t sx, int16_t sy, int16_t to, int16_t dx, int16_t dy, int16_t w,
                  int16_t h);
void blit(const BitMap& src, int16_t sx, int16_t sy, const BitMap& dst, int16_t dx, int16_t dy, int16_t w, int16_t h);

// text.cpp
int32_t draw_text(const TextStyle& style, int16_t x, int16_t y, const char* str);
int32_t text_at(const TextStyle& style, int16_t x, int16_t y, const char* str);
int32_t text_str(const TextStyle& style, const char* str, int16_t x, int16_t y);
void text_lines(const TextStyle& style, const char* str, int16_t unused, int16_t x, int16_t y, int16_t step);
int16_t char_width(int16_t font, int16_t ch);

// blitter.cpp
void copy_region(const BitMap& src, const BitMap& dst, int16_t x, int16_t y, int16_t w, int16_t h);
void draw_shape(const BitMap& shape, const BitMap& dst, int16_t x, int16_t y);

// pictures.cpp
void load_shape(int16_t shape);
void load_picture(int16_t picture, int16_t bitmap);
uint32_t read_file_part(const char* name, const char* missing, int32_t offset, int32_t length);
void build_shape(uint32_t data, int32_t length);
int16_t show_screen(int16_t n);
void opening_picture(int16_t n, int16_t bitmap);  // RedStorm, Title

// pictures.cpp: the disk checks (the data is always there)
void need_disk_a();  // RedStormA:
void need_disk();    // RedStormB: (the game disk)

// decoder.cpp: the picture decoder
int16_t unpack(uint32_t bitmap, uint32_t data, int32_t length);  // a picture into a BitMap
int16_t picture_palette(mem::be16* colours);  // the palette of the picture unpacked last

// animation.cpp
void flush_regions(int16_t n, int16_t from);
void draw_objects(int16_t to);
void animate(int16_t frame);
void reset_regions();
void cycle_colours();

// sequences.cpp: Storm, Title
void play_sequence(int16_t n, int16_t variant);

// requesters.cpp
void alert(const char* text);        // a recoverable alert
void fatal_error(const char* text);  // does not return (nullptr: no alert)
void disk_requester(const char* volume, uint32_t& lock, int16_t can_cancel);  // until lock (or cancelled)

}  // namespace game::screen
