// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Storm: printing with Storm's text style, and the status line under the
// displays.
#include "game/common/strings.hpp"
#include "game/mem/register.hpp"
#include "game/screen/calls.hpp"
#include "game/screen/vars.hpp"
#include "game/storm/calls.hpp"
#include "game/storm/vars.hpp"
#include "game/storm/texts.hpp"

#include <cstring>
#include <string>

namespace game::storm {

namespace {
using screen::TextStyle;
constexpr uint16_t JAM1 = 0, JAM2 = 1;

int32_t print(int16_t x, int16_t y, const char* str) { return screen::draw_text(*text_style, x, y, str); }

// Print into another bitmap than the style's.
void print_on(int16_t bitmap, int16_t x, int16_t y, const char* str) {
  TextStyle& s = *text_style;
  const uint16_t saved = s.bitmap;
  s.bitmap = uint16_t(bitmap);
  print(x, y, str);
  s.bitmap = saved;
}
}  // namespace

// $2ca48: str at (x, y) in pen `colour`.
void text(const char* str, int16_t x, int16_t y, int16_t colour) {
  TextStyle& s = *text_style;
  s.mode = JAM1;
  s.fg = uint16_t(colour);
  s.bg = 0;
  print(x, y, str);
}
ORIGINAL_INSIDE(text, 0x2ca48, 0, 0, 0, 0);

// $2ca8a: str at a character cell (8 x 8, the text 4 and 5 pixels in) in
// pen `colour` on pen 0.
void text_at_cell(const char* str, int16_t col, int16_t row, int16_t colour) {
  TextStyle& s = *text_style;
  s.mode = JAM2;
  s.fg = uint16_t(colour);
  s.bg = 0;
  print(int16_t(col * 8 + 4), int16_t(row * 8 + 5), str);
}
ORIGINAL_INSIDE(text_at_cell, 0x2ca8a, 0, 0, 0, 0);

// $2cad8: str with a shadow: the blank text cleared in pen 0, str one
// pixel to the right in the shadow pen, then str in `colour`.
void text_shadow(const char* str, int16_t x, int16_t y, int16_t colour) {
  TextStyle& s = *text_style;
  s.mode = JAM1;
  s.fg = 0;
  print(x, y, blank_text.str());
  s.mode = JAM2;
  s.bg = 0;
  s.fg = uint16_t(colour < kShadowPens ? colour + kShadowPens : colour);
  print(int16_t(x + 1), y, str);
  s.mode = JAM1;
  s.fg = uint16_t(colour);
  print(x, y, str);
}
ORIGINAL_INSIDE(text_shadow, 0x2cad8, 0, 0, 0, 0);

// $2cb74: str in pen fg on pen bg.
void text_jam2(const char* str, int16_t x, int16_t y, int16_t fg, int16_t bg) {
  TextStyle& s = *text_style;
  s.mode = JAM2;
  s.fg = uint16_t(fg);
  s.bg = uint16_t(bg);
  print(x, y, str);
  s.bg = 0;
}
ORIGINAL_INSIDE(text_jam2, 0x2cb74, 0, 0, 0, 0);

// $2cbc0: one character in pen `colour` on bitmap `bitmap`.
void draw_char(int16_t ch, int16_t x, int16_t y, int16_t colour, int16_t bitmap) {
  texts().text_buffer[0] = char(ch);
  texts().text_buffer[1] = 0;
  TextStyle& s = *text_style;
  s.mode = JAM1;
  s.fg = uint16_t(colour);
  s.bg = 0;
  print_on(bitmap, x, y, texts().text_buffer);
}
ORIGINAL_IN("storm", 0x2cbc0, draw_char);

// $2cc30: str in pen `colour` on bitmap 2.
void text_on_bitmap2(const char* str, int16_t x, int16_t y, int16_t colour) {
  TextStyle& s = *text_style;
  s.mode = JAM1;
  s.fg = uint16_t(colour);
  print_on(2, x, y, str);
}
ORIGINAL_INSIDE(text_on_bitmap2, 0x2cc30, 0, 0, 0, 0);

// $2cc7e: str in pen fg on pen bg, on bitmap 1.
void text_on_bitmap1(const char* str, int16_t x, int16_t y, int16_t fg, int16_t bg) {
  TextStyle& s = *text_style;
  s.mode = JAM2;
  s.fg = uint16_t(fg);
  s.bg = uint16_t(bg);
  print_on(1, x, y, str);
}
ORIGINAL_INSIDE(text_on_bitmap1, 0x2cc7e, 0, 0, 0, 0);

// $2ccde: str centred on the status line, shadowed, in `colour`, unless
// status messages are off. With `remember`, the message last remembered
// is not shown again and a new one is remembered.
void status_message(const char* str, int16_t colour, int16_t remember) {
  if (battle.aiming || battle.entry_mode) return;
  if (remember) {
    if (std::strcmp(str, texts().last_status) == 0) return;
    std::strcpy(texts().last_status, str);
  }
  screen::fill_rect_colour(kStatusLeft, kStatusTop, kStatusRight, kStatusBottom, kPenBlack);
  const int16_t length = int16_t(std::strlen(str));
  text_shadow(str, int16_t(kStatusCentre - kStatusHalfChar * length), kStatusTextY, colour);
}
ORIGINAL_INSIDE(status_message, 0x2ccde, 0, 0, 0, 0);

// $2cd6a: "<name> BEARING <degrees>" on the status line (remembered).
void status_bearing(const char* name, int16_t angle, int16_t colour) {
  std::string line = name;
  line += bearing_text.str();
  line += common::format_degrees(angle);
  std::strcpy(&texts().text_buffer[0], line.c_str());  // as the original, into its buffer
  status_message(texts().text_buffer, colour, 1);
}
ORIGINAL_INSIDE(status_bearing, 0x2cd6a, 0, 0, 0, 0);

// $255be: the status message text_pause_status while its flag is set.
void status_if_flag() {
  if (battle.paused) status_message(text_pause_status.str(), kPenWhite, 1);
}
ORIGINAL_IN("storm", 0x255be, status_if_flag);

// $2cdc2: clear the status line and forget the remembered message.
void clear_status() {
  screen::fill_rect_colour(kStatusLeft, kStatusTop, kStatusRight, kStatusBottom, kPenBlack);
  texts().last_status[0] = 0;
}
ORIGINAL_IN("storm", 0x2cdc2, clear_status);

}  // namespace game::storm
