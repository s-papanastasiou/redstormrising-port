// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Screen: text in the window's RastPort with a text style (font, pens,
// draw mode, offsets).
#include "game/mem/os.hpp"

#include <cstring>
#include <string>

#include "game/mem/register.hpp"
#include "game/screen/calls.hpp"
#include "game/screen/vars.hpp"
#include "game/ui/state.hpp"

namespace game::screen {

namespace {
uint32_t rp() { return (ui::state().rastport).addr; }
uint32_t font(int16_t n) { return ui::state().fonts[n]; }
}  // namespace

// $32426: draw str at (x, y) plus the style's offsets, in its font, pens
// and draw mode. Returns where the text ended (x without the offset), as
// a long: some callers read all of d0, whose high word the original
// leaves as Text found it (the length, so 0).
// Draw modes other than JAM1 (0) and JAM2 (1) are not used; the original
// would pass on a register of its caller for them, here they are JAM1.
int32_t draw_text(const TextStyle& s, int16_t x, int16_t y, const char* str) {
  RastPort& r = *ui::state().rastport;
  if (font(int16_t(s.font)) != r.font) os::set_font(rp(), font(int16_t(s.font)));
  os::set_bpen(rp(), int16_t(s.bg));
  os::set_drmd(rp(), s.mode == 1 ? 1 : 0);
  os::set_apen(rp(), int16_t(s.fg));
  os::move(rp(), int32_t(x) + s.x, int32_t(y) + s.y);
  os::text(rp(), str, int16_t(std::strlen(str)));
  return int32_t(uint16_t(r.cp_x - s.x));
}
ORIGINAL_INSIDE(draw_text, 0x32426, 0, 0, 0x26ffe, 0x284da);

// $33024: draw_text with the arguments in another order.
int32_t text_at(const TextStyle& style, int16_t x, int16_t y, const char* str) { return draw_text(style, x, y, str); }
ORIGINAL_INSIDE(text_at, 0x33024, 0, 0, 0x273e4, 0x29794);

// $32d4c: the same with the string first.
int32_t text_str(const TextStyle& style, const char* str, int16_t x, int16_t y) { return text_at(style, x, y, str); }
ORIGINAL_INSIDE(text_str, 0x32d4c, 0, 0, 0x2710c, 0x294bc);

// $32d76: draw str line by line (split at newlines) from (x, y) down,
// `step` pixels apart (the original copied each line to a 128-byte
// buffer first).
void text_lines(const TextStyle& style, const char* str, int16_t unused, int16_t x, int16_t y, int16_t step) {
  (void)unused;
  for (const char* at = str;; ++at) {
    const char* start = at;
    while (*at != 0 && *at != '\n') ++at;
    const std::string line(start, size_t(at - start));
    text_at(style, x, y, line.c_str());
    if (*at == 0) return;
    y = int16_t(y + step);
  }
}
ORIGINAL_INSIDE(text_lines, 0x32d76, 0, 0, 0x27136, 0x294e6);

// $31472: the width in pixels of character ch in font number `font`.
int16_t char_width(int16_t font_number, int16_t ch) {
  RastPort& r = *ui::state().rastport;
  const uint32_t previous = r.font;
  const char text[2] = {char(ch), 0};
  os::set_font(rp(), font(font_number));
  const int16_t width = int16_t(os::text_length(rp(), text, 1));
  os::set_font(rp(), previous);
  return width;
}
ORIGINAL(char_width, 0x31472, 0x21582, 0x232ba, 0x235b2, 0x2751a);
ORIGINAL(char_width, 0x32962, 0, 0, 0x2512a, 0x290d2);  // wrappers

}  // namespace game::screen
