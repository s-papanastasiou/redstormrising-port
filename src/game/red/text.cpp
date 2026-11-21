// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Red (the campaign module): text on the text page through the current
// text style: pending characters, cells, control codes, clearing.
#include "game/common/strings.hpp"
#include "game/mem/register.hpp"
#include "game/red/calls.hpp"
#include "game/red/texts_state.hpp"
#include "game/red/vars.hpp"
#include "game/screen/calls.hpp"
#include "game/ui/state.hpp"
#include "game/screen/styles.hpp"

#include <algorithm>

#include <cstring>

namespace game::red {

using screen::TextStyle;

namespace {
TextStyle& style() { return *ui::state().page_text_style; }
TextStyle& page_text_style() { return *ui::state().page_text_style; }
}  // namespace

// The cursor at text column n (and the text page's column).
void set_column(int16_t n) {
  ui::state().page_col = n;
  style().cursor_x = int16_t((n << 3) - style().x);
}

// The cursor at text row n.
void set_row(int16_t n) {
  ui::state().page_row = n;
  style().cursor_y = int16_t((n << 3) - style().y);
}

// The cursor at the start of the next line.
void new_line() {
  style().cursor_x = style().margin;
  style().cursor_y += 8;
  ui::state().page_col = int16_t(int16_t(style().x + style().margin) / 8);
  ui::state().page_row = int16_t(int16_t(style().y + style().cursor_y) / 8);
}

// $24540: text at the style's cursor; the cursor moves to its end.
void text_object(TextStyle& t, const char* str) {
  t.cursor_x = int16_t(screen::draw_text(t, t.cursor_x, t.cursor_y, str));
}
ORIGINAL_INSIDE(text_object, 0, 0, 0, 0, 0x24540);

// $24382: draw the pending characters.
void flush_text() {
  if (ui::state().pending_text[0]) text_object(page_text_style(), ui::state().pending_text);
  ui::state().pending_text[0] = 0;
  ui::state().pending_length = 0;
}
ORIGINAL_IN("red", 0x24382, flush_text);

// $23f8c: a character to the pending text (and the text page).
void record_char(int16_t ch) {
  ui::State& u = ui::state();
  u.pending_text[u.pending_length] = char(ch);
  u.pending_length = uint16_t(u.pending_length + 1);
  u.pending_text[u.pending_length] = 0;
  put_char(int16_t(uint8_t(ch)));
}
ORIGINAL_IN("red", 0x23f8c, record_char);

// $23f5c: a character drawn at once (and kept on the text page).
void text_char(int16_t ch) {
  const char text[2] = {char(ch), 0};
  text_object(page_text_style(), text);
  put_char(int16_t(uint8_t(ch)));
}
ORIGINAL_IN("red", 0x23f5c, text_char);

// $23fb0: a character centred in an 8-pixel cell (kBlankCell: an empty
// one); the cursor moves one cell on, within the style's width.
void cell_char(int16_t ch) {
  const uint8_t c = uint8_t(ch);
  const int16_t x0 = style().cursor_x;
  if (c != kBlankCell) {
    const char text[2] = {char(c), 0};
    const int16_t w = screen::char_width(int16_t(style().font), c);
    style().cursor_x += int16_t((8 - w) / 2);
    text_object(page_text_style(), text);
  }
  style().cursor_x = std::min<int16_t>(style().width, int16_t(x0 + 8));
  put_char(c);
}
ORIGINAL_IN("red", 0x23fb0, cell_char);

// $24184: a number below 100 as two cells (a blank for the tens below 10).
void two_digits(int16_t v) {
  cell_char(v < 10 ? kBlankCell : int16_t(v / 10 + '0'));
  cell_char(int16_t(v % 10 + '0'));
}
ORIGINAL_IN("red", 0x24184, two_digits);

// $24352: the width of a string in the style's font.
int16_t string_width(const char* str) {
  int16_t w = 0;
  for (const char* p = str; *p; ++p) w = int16_t(w + screen::char_width(int16_t(style().font), uint8_t(*p)));
  return w;
}
ORIGINAL_INSIDE(string_width, 0, 0, 0, 0, 0x24352);

// $2bb9c: the string after n NUL-terminated ones.
const char* skip_strings(const char* p, int16_t n) {
  for (; n > 0; --n) p += std::strlen(p) + 1;
  return p;
}
ORIGINAL_INSIDE(skip_strings, 0, 0, 0, 0, 0x2bb9c);

// $2d996: the text page style becomes current, the text page is cleared
// (spaces in the background colour); the map's controls are off.
void clear_text_page() {
  ui::state().page_text_style = page_style.addr();
  page_style->margin = 0;
  page_style->x = 0;
  page_style->width = kPageWidth;
  for (int row = 0; row < kPageRows; ++row)
    for (int i = 0; i < kPageColumns; ++i) {
      page_byte(row * kPageRow + i) = ' ';
      page_byte(row * kPageRow + kPageColours + i) = uint8_t(page_style->bg);
    }
  no_help_request() = 1;
  map_controls() = 0;
  map_active() = 0;
}
ORIGINAL_IN("red", 0x2d996, clear_text_page);
ORIGINAL_IN("red", 0x24572, clear_text_page);  // a jump to it

// $243a6: text with control codes: 0 end, 9 n column, 10/13 new line,
// 11 n row, 12 clear the page, 14 n background, 15 n foreground, 16 n
// margin (in cells), 18 n c n cells of c, 19 home; others are text.
void print_text(const char* str) {
  ui::state().pending_text[0] = 0;
  ui::state().pending_length = 0;
  const uint8_t* p = reinterpret_cast<const uint8_t*>(str);
  auto next = [&p]() -> uint8_t { return *p++; };
  for (;;) {
    const uint8_t ch = next();
    if (ch == 0) {
      flush_text();
      return;
    }
    switch (ch) {
      case 9:
        flush_text();
        set_column(next());
        break;
      case 10:
      case 13:
        flush_text();
        new_line();
        break;
      case 11:
        flush_text();
        set_row(next());
        break;
      case 12:
        flush_text();
        clear_text_page();
        break;
      case 14:
        flush_text();
        style().bg = next();
        break;
      case 15:
        flush_text();
        style().fg = next();
        break;
      case 16:
        flush_text();
        style().margin = int16_t(next() << 3);
        break;
      case 18: {
        flush_text();
        int16_t n = next();
        const uint8_t c = next();
        for (; n > 0; --n) cell_char(c);
        break;
      }
      case 19:
        flush_text();
        style().cursor_x = style().margin;
        style().cursor_y = 0;
        ui::state().page_col = int16_t(int16_t(style().x + style().margin) / 8);
        ui::state().page_row = int16_t(style().y / 8);
        break;
      default:
        record_char(ch);
        break;
    }
  }
}
ORIGINAL_INSIDE(print_text, 0, 0, 0, 0, 0x243a6);

// $2daf6: clear from the cursor to the end of its line.
void clear_line_rest() {
  const TextStyle& s = style();
  const int16_t x0 = int16_t(s.x + s.cursor_x), y0 = int16_t(s.y + s.cursor_y);
  const int16_t y1 = std::min<int16_t>(int16_t(s.y + s.height), int16_t(y0 + 7));
  screen::fill_style_bg(page_text_style(), x0, y0, int16_t(s.x + s.width), y1);
}
ORIGINAL_IN("red", 0x2daf6, clear_line_rest);

// $2dbde: clear n cells from the cursor.
void clear_cells(int16_t n) {
  const TextStyle& s = style();
  const int16_t y0 = int16_t(s.y + s.cursor_y), x0 = int16_t(s.x + s.cursor_x);
  const int16_t y1 = std::min<int16_t>(int16_t(s.y + s.height), int16_t(y0 + 7));
  const int16_t x1 = std::min<int16_t>(int16_t(s.x + s.width), int16_t((n << 3) + x0 - 1));
  screen::fill_style_bg(page_text_style(), x0, y0, x1, y1);
}
ORIGINAL_IN("red", 0x2dbde, clear_cells);

// $2dac2: clear a style's area.
void fill_style(const TextStyle& t) {
  screen::fill_style_bg(t, t.x, t.y, int16_t(t.x + t.width), int16_t(t.y + t.height));
}
ORIGINAL_IN("red", 0x2dac2, fill_style);

// $2ba74: an order's name (up to a '*'), a character at a time.
void print_name(int16_t code) {
  char text[2] = {one_char_template[0], one_char_template[1]};
  const uint8_t* name = reinterpret_cast<const uint8_t*>(skip_strings(name_list.str(), int16_t(order_code(code) + 2)));
  for (int i = 0; name[i] != 0 && name[i] != kNameEnd; ++i) {
    text[0] = char(name[i]);
    text_object(page_text_style(), text);
  }
}
ORIGINAL_IN("red", 0x2ba74, print_name);

// $24132: the low byte of v as two hex digits (in cells).
void hex_byte(int16_t v) {
  auto digit = [](int16_t d) {
    d = int16_t(d & 15);
    cell_char(int16_t((d > 9 ? d + 7 : d) + '0'));
  };
  digit(int16_t(v >> 4));
  digit(v);
}
ORIGINAL_IN("red", 0x24132, hex_byte);

// $240d8: v right-aligned in `width` cells, padded with `fill`.
void number_cells(int16_t v, int16_t width, int16_t fill) {
  common::format_int(v, number_text());
  const int16_t len = int16_t(std::strlen(number_text()));
  while (width-- > len) cell_char(int16_t(uint8_t(fill)));
  for (int16_t i = 0; i < len; ++i) cell_char(int16_t(uint8_t(number_text()[i])));
}
ORIGINAL_IN("red", 0x240d8, number_cells);

// $2d624: a time as hhmm.
void print_hhmm(const Date& d) {
  clear_cells(4);
  number_cells(d.hour, 2, '0');
  number_cells(d.minute, 2, '0');
}
ORIGINAL_INSIDE(print_hhmm, 0, 0, 0, 0, 0x2d624);

// $2d65a: a date as mm/dd/yy.
void print_date(const Date& d) {
  clear_cells(8);
  number_cells(d.month, 2, '0');
  text_char('/');
  number_cells(d.day, 2, '0');
  text_char('/');
  hex_byte(d.year);
}
ORIGINAL_INSIDE(print_date, 0, 0, 0, 0, 0x2d65a);

// $2d5a8: the status line's word (by the counts) after its prefix.
void print_status_word() {
  string_copy(number_text(), status_prefix.str());
  int16_t k;
  if (war.terrain.colour_1 > 17) k = 4;
  else if (war.terrain.colour_1 > 1) k = 3;
  else if (war.terrain.other > int16_t(2 * war.terrain.fronts + war.terrain.colour_26)) k = 0;
  else k = war.terrain.fronts > 0 ? 2 : 1;
  string_append(number_text(), skip_strings(status_words.str(), k));
  print_text(number_text());
}
ORIGINAL_IN("red", 0x2d5a8, print_status_word);

// (Red $2db4e, clear_to_end: not reached by the game.)

// $241ca: clear the rest of the line, on screen and on the text page.
void clear_row_rest() {
  const TextStyle& s = style();
  const uint16_t n = uint16_t(int16_t(int16_t(s.x + s.width) / 8) - ui::state().page_col + 1);
  clear_line_rest();
  const int32_t at = ui::state().page_row * kPageRow + ui::state().page_col;
  for (uint16_t i = 0; i < n; ++i) page_byte(at + i) = ' ';
  for (uint16_t i = 0; i < n; ++i) page_byte(at + kPageColours + i) = uint8_t(style().bg);
}
ORIGINAL_IN("red", 0x241ca, clear_row_rest);

}  // namespace game::red
