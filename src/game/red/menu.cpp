// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Red (the campaign module): choosing from the menu of a text page (two
// columns of entries, by mouse or keys), the highlight, and the medal
// icons.
#include "game/input/calls.hpp"
#include "game/input/vars.hpp"
#include "game/mem/hardware.hpp"
#include "game/mem/os.hpp"
#include "game/mem/register.hpp"
#include "game/red/calls.hpp"
#include "game/red/vars.hpp"
#include "game/screen/calls.hpp"
#include "game/ui/state.hpp"
#include "game/screen/styles.hpp"

namespace game::red {

namespace {
uint8_t& cell_colour(int row, int col) { return page_byte(row * kPageRow + kPageColours + col); }
bool hits(const MenuEntry& e, int16_t row, int16_t col) { return e.row == row && col >= e.first && col <= e.last; }
}  // namespace

// $2b1fa: light up the current entry (mode 1) or put it back (mode 3),
// on the text page and on screen.
void highlight(int16_t mode) {
  const MenuEntry& e = (ui::state().menu_list)[ui::state().menu_index];
  for (int col = e.first; col <= e.last; ++col) {
    uint8_t& c = cell_colour(e.row, col);
    if (mode == 3 && c == kEntryNormal) c = kEntryLit;
    else if (mode == 1 && c == kEntryLit) c = kEntryNormal;
  }
  if (mode != 1 && mode != 3) return;
  const int16_t x0 = int16_t(e.first << 3), y0 = int16_t(e.row << 3), x1 = int16_t((e.last << 3) + 8);
  draw_entry(*ui::state().page_text_style, x0, y0, x1, int16_t(y0 + 7), mode == 1 ? 3 : 1, mode == 1 ? 1 : 3);
}
ORIGINAL_IN("red", 0x2b1fa, highlight);

// $2af3c: choose an entry first..last of the two columns: a click on one,
// or the direction keys (left/right change columns, up/down step round,
// on over entries that are not lit), until fire. Returns its code (a
// long: callers read all of d0; the original's high bytes are the index
// times 4, zero below 64 entries).
int32_t choose(int16_t first, int16_t last, const mem::Const<MenuEntry>& left_list,
               const mem::Const<MenuEntry>& right_list) {
  const uint32_t left = left_list.addr(), right = right_list.addr();  // (the menu state keeps the list's address)
  bool fire = false;
  screen::restore_pointer();
  ui::state().menu_keys = 4;
  uint32_t list = (ui::state().menu_side & kRightSide) ? right : left;
  ui::state().menu_list = list;
  int16_t idx = ui::state().menu_index;
  highlight(1);
  const MenuEntry* l = &constant<MenuEntry>(left);
  const MenuEntry* r = &constant<MenuEntry>(right);
  while (!quit_key_pressed() && !fire) {
    ui::state().menu_flags = ui::state().menu_flags & ~kKeyRead;
    mouse_clicked() = 0;
    delay(1);
    input::frame_tick();
    fire = ui::state().menu_flags & kMenuFire;
    if (fire) continue;
    if (mouse_clicked()) {
      const int16_t row = int16_t(ui::state().mouse_pos.y >> 3), col = int16_t(ui::state().mouse_pos.x >> 3);
      for (int16_t i = first; i < last + 1; ++i) {
        if (hits(l[i], row, col)) {
          ui::state().menu_side &= uint8_t(~kRightSide);
          list = left;
          idx = i;
          break;
        }
        if (hits(r[i], row, col)) {
          ui::state().menu_side |= kRightSide;
          list = right;
          idx = i;
          break;
        }
      }
    } else {
      ui::state().menu_keys = uint16_t(menu_dirs_red() & 15);
      for (int n = 0; n < 8; ++n) screen::wait_tof();
      for (;;) {
        const uint8_t k = uint8_t(ui::state().menu_keys);
        const MenuEntry& now = (ui::state().menu_list)[ui::state().menu_index];
        if (k & 2) {
          ui::state().menu_side &= uint8_t(~kRightSide);
          list = left;
          ui::state().menu_keys = 4;
          if (left == menu_split.addr() && now.first > 20) idx = int16_t(idx - 4);
        } else if (k & 1) {
          ui::state().menu_side |= kRightSide;
          list = right;
          ui::state().menu_keys = 4;
          if (right == menu_split.addr() && now.last < 20) idx = int16_t(idx + 4);
        } else if (k & 8) {
          if (--idx < first) idx = last;
        } else if (k & 4) {
          if (++idx > last) idx = first;
        }
        const MenuEntry& e = constant<MenuEntry>(list + uint32_t(idx * int16_t(sizeof(MenuEntry))));
        if (cell_colour(e.row, e.first) == kEntryLit || ui::state().menu_keys == 0) break;
      }
    }
    if (idx != ui::state().menu_index || list != (ui::state().menu_list).addr) {
      highlight(3);
      ui::state().menu_index = idx;
      ui::state().menu_list = list;
      highlight(1);
    }
  }
  input::wait_released();
  screen::hide_pointer();
  return (ui::state().menu_list)[ui::state().menu_index].code;
}
ORIGINAL_IN("red", 0x2af3c, choose);

// $23c30: the medals held, as icons in a row from x 112.
void draw_icons() {
  int16_t x = 112;
  for (int16_t slot = 5; slot >= 0; --slot, x = int16_t(x + 32))
    if (*medals_held & medal_masks[slot]) screen::draw_masked(medal_shapes[slot], x, 112, 0, 0);
}
ORIGINAL(draw_icons, 0, 0, 0, 0x263b4, 0x23c30);

// $285a0 (RedStart $270c4): recolour a rectangle (pens a -> b) in the
// bitmap of a style.
void draw_entry(const screen::TextStyle& style, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t a, int16_t b) {
  recolour_rect(screen::bitmap_at(int16_t(style.bitmap)), x0, y0, x1, y1, a, b);
}
ORIGINAL(draw_entry, 0, 0, 0, 0x270c4, 0x285a0);

// $29d1e (RedStart $2492e): a row of a row menu is shown: its y goes to
// the row list, the row is recoloured in the window style.
void add_row(const RowMenu& m, int16_t row) {
  (ui::state().window_style)->bitmap = uint16_t(screen::drawing_bitmap());
  const int16_t y = int16_t(m.y + uint16_t(uint32_t(uint16_t(row)) * m.step));
  uint8_t i = 0;
  while (ui::state().row_list[i] != 0) ++i;
  ui::state().row_list[i] = uint16_t(y);
  ui::state().row_list[uint8_t(i + 1)] = 0;
  const screen::TextStyle& c = *ui::state().row_colours;
  draw_entry(*ui::state().window_style, m.x, y, int16_t(m.width + m.x), int16_t(m.height + y - 1), c.y, c.fg);
}
ORIGINAL(add_row, 0, 0, 0, 0x2492e, 0x29d1e);

// $29a2e (RedStart $2463e): choose a row of a row menu, starting at entry
// `start` (entries from 10 are in the second column), by a click or the
// direction keys (the rows in row_list are passed over), until fire.
// The chosen entry is lit. Returns it.
int16_t choose_row(const RowMenu& m, int16_t start) {
  const screen::TextStyle& c = *ui::state().row_colours;
  screen::restore_pointer();
  (ui::state().window_style)->bitmap = uint16_t(screen::drawing_bitmap());
  int16_t chosen = start;
  ui::state().row_chosen = -1;
  int16_t y = start < kSecondColumn ? int16_t(m.step * start + m.y) : int16_t((start - kSecondColumn) * m.step + m.y);
  int16_t x = start < kSecondColumn ? int16_t(m.x) : int16_t(m.x + m.column);
  draw_entry(*ui::state().window_style, x, y, int16_t(m.width + x), int16_t(m.height + y - 1), c.y, int16_t(c.mode));
  input::flush_keys();
  uint8_t dirs = 0;
  const int16_t step = int16_t(m.step);
  while (!ui::state().quit_key && !(dirs & input::kFire)) {
    if (mouse_clicked()) {
      mouse_clicked() = 0;
      x = m.column ? int16_t(int16_t((menu_mouse_pos().x - m.x) / m.column) * m.column + m.x) : int16_t(m.x);
      if (step) {
        y = int16_t(int16_t((menu_mouse_pos().y - m.y) / step) * step + m.y);
        if (y < m.y) y = m.y;
        if (y > int16_t(m.y_end - step)) y = int16_t(m.y_end - step);
      } else {
        y = m.y;
      }
    } else {
      if (dirs & input::kRight) {
        x = int16_t(x + m.column);
        if (x >= m.x_end) x = m.x;
      }
      if (dirs & input::kLeft) {
        x = int16_t(x - m.column);
        if (x < m.x) x = m.column;  // (as the original: not the last column)
      }
      if (dirs & input::kDown) {
        y = int16_t(y + step);
        if (y >= m.y_end) y = m.y;
      }
      if (dirs & input::kUp) {
        y = int16_t(y - step);
        if (y < m.y) y = int16_t(m.y_end - step);
      }
    }
    const int16_t second = m.column ? int16_t((x - m.x) / m.column) : int16_t(0);
    const int16_t index = int16_t((y - m.y) / step + (second ? kSecondColumn : 0));
    if (index != chosen) {
      x = m.column ? int16_t(int16_t((x - m.x) / m.column) * m.column + m.x) : int16_t(m.x);
      y = int16_t(int16_t((y - m.y) / step) * step + m.y);
      uint8_t i = 0;
      while (ui::state().row_list[i] != 0 && ui::state().row_list[i] != uint16_t(y)) ++i;
      if (ui::state().row_list[i] == 0) {
        draw_entry(*ui::state().window_style, m.x, m.y, m.x_end, m.y_end, int16_t(c.mode), c.y);
        draw_entry(*ui::state().window_style, x, y, int16_t(x + m.width), int16_t(y + m.height - 1), c.y, int16_t(c.mode));
        chosen = index;
        if (!mouse_clicked())
          for (int k = 0; k < 10; ++k) screen::wait_tof();
      }
    }
    ui::state().menu_flags = ui::state().menu_flags & ~input::kMenuKeyRead;
    input::frame_tick();
    if (index == chosen || !dirs) dirs = uint8_t(ui::state().menu_dirs);
  }
  screen::hide_pointer();
  return chosen;
}
ORIGINAL(choose_row, 0, 0, 0, 0x2463e, 0x29a2e);

// $29884 (RedStart $24494): a menu page in the window style: cleared and
// framed from its top, its blocks of text lines (selectable rows in
// their own colour, their y kept in the row list), and the two footers
// when the last block is plain text.
void menu_page(const PageBlock* blocks) {
  // The row colours' style read as its first five words (pens).
  const sbe16* pens = reinterpret_cast<const sbe16*>(&*ui::state().row_colours);
  screen::TextStyle& s = *ui::state().window_style;
  s.fg = uint16_t(pens[0]);
  s.bg = uint16_t(pens[1]);
  s.bitmap = uint16_t(screen::drawing_bitmap());
  screen::fill_style_fg(s, 0, ui::state().menu_page_top, kPageWidth, 199);
  screen::fill_style_bg(s, 7, int16_t(ui::state().menu_page_top + 7), 312, 192);
  ui::state().row_list[0] = 0;
  int16_t step = 0;
  for (const PageBlock* b = blocks; b->count != 0; ++b) {
    int16_t count = b->count, text = b->first, y = b->y;
    step = b->step;
    if (step) {
      s.fg = uint16_t(pens[2]);
    } else {
      int i = 0;
      while (ui::state().row_list[i] != 0) ++i;
      ui::state().row_list[i] = uint16_t(y);
      ui::state().row_list[i + 1] = 0;
      s.fg = uint16_t(pens[3]);
    }
    while (count-- != 0) {
      text_n_ptr(text++);
      screen::text_lines(s, ui::state().unpacked_text, b->unused, b->x, y, 8);
      y = int16_t(y + step);
    }
  }
  if (step == 0) return;
  s.fg = uint16_t(pens[4]);
  screen::text_lines(s, (text_n_ptr(kFooterA)), 300, 80, 185, 8);
  screen::text_lines(s, (text_n_ptr(kFooterB)), 300, 68, 193, 8);
}
ORIGINAL_INSIDE(menu_page, 0, 0, 0, 0x24494, 0x29884);

void menu_page(const mem::Const<PageBlock>& page) {
  menu_page(page.until([](const PageBlock& b) { return b.count == 0; }));
}

// $29f8e (RedStart $24b9e): a character at the window's cursor, which
// moves on.
void window_char(int16_t ch) {
  ui::state().window_text[0] = char(ch);
  ui::state().window_text[1] = 0;
  ui::Point& at = ui::state().window_cursor;
  at.x = int16_t(screen::text_str(*ui::state().window_style, ui::state().window_text, at.x, at.y));
}
ORIGINAL(window_char, 0, 0, 0, 0x24b9e, 0x29f8e);

// $29db8 (RedStart $249c8): draw into `bitmap`, through the window style
// too. Red never calls its copy.
void draw_to_window(int16_t bitmap) {
  screen::draw_to(bitmap);
  (ui::state().window_style)->bitmap = uint16_t(bitmap);
}
ORIGINAL(draw_to_window, 0, 0, 0, 0x249c8, 0);

// $285e8 (RedStart $24bd0): recolour the pixels of colour `from` in a rectangle of a
// bitmap (6 planes) to `to`, with the blitter: three blits compare the
// planes with `from` into a temporary plane (the matching pixels), then
// each plane where `from` and `to` differ is flipped through it, and the
// temporary plane is cleared.
void recolour_rect(const amiga::BitMap& bm, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t from, int16_t to) {
  os::own_blitter();
  const uint16_t bpr = bm.bytes_per_row;
  const uint32_t off = uint32_t(uint16_t(y0)) * bpr + ((uint32_t(uint16_t(x0)) & ~15u) >> 3);
  const int16_t width = int16_t(((x1 >> 4) - (x0 >> 4)) * 2 + 2);  // bytes
  const uint16_t mod = uint16_t(bpr - width);
  const uint16_t size = uint16_t(((y1 - y0 + 1) << 6) + (width >> 1));
  const uint16_t first = recolour_first_masks[x0 & 15], last = recolour_first_masks[16 + (x1 & 15)];
  const uint32_t temp = temp_plane() + off;
  auto plane = [&](int p) { return uint32_t(bm.planes[p]) + off; };
  using hw::Blitter;
  Blitter& b = hw::blitter();
  auto masks = [&] {
    b.afwm = first;
    b.alwm = last;
  };
  // The pixels whose planes 0-2 match `from`, into the temporary plane.
  masks();
  b.pt[Blitter::A] = plane(0);
  b.pt[Blitter::B] = plane(1);
  b.pt[Blitter::C] = plane(2);
  b.pt[Blitter::D] = temp;
  for (int16_t& m : b.mod) m = int16_t(mod);
  b.con0 = uint16_t(kBlitABCD | recolour_minterms[from & 7]);
  b.con1 = 0;
  hw::blit(size);
  // ... and planes 3-4.
  b.pt[Blitter::B] = plane(3);
  b.pt[Blitter::C] = plane(4);
  b.pt[Blitter::A] = temp;
  b.pt[Blitter::D] = temp;
  b.con0 = uint16_t(kBlitABCD | recolour_minterms[((from >> 2) | 1) & 7]);
  masks();
  hw::blit(size);
  // ... and plane 5.
  b.pt[Blitter::B] = plane(5);
  b.pt[Blitter::A] = temp;
  b.pt[Blitter::D] = temp;
  const int i = ((from >> 4) | 1) & 3;
  b.con0 = uint16_t(kBlitABD | uint8_t(recolour_minterms[i] | recolour_minterms[4 + i]));
  masks();
  hw::blit(size);
  // Flip the planes in which the colours differ, where the pixels match.
  const int16_t diff = int16_t(from ^ to);
  for (int p = 5; p >= 0; --p) {
    if (!(diff & (1 << p))) continue;
    b.pt[Blitter::A] = temp;
    b.pt[Blitter::C] = plane(p);
    b.pt[Blitter::D] = plane(p);
    b.con0 = kBlitAC_D_Flip;
    b.con1 = 0;
    b.mod[Blitter::A] = b.mod[Blitter::C] = b.mod[Blitter::D] = int16_t(mod);
    b.afwm = hw::kAllBits;
    b.alwm = hw::kAllBits;
    hw::blit(size);
  }
  // Clear the temporary plane.
  b.pt[Blitter::D] = temp;
  b.mod[Blitter::D] = int16_t(mod);
  b.con0 = kBlitDOnly;
  b.con1 = 0;
  hw::blit(size);
  os::disown_blitter();
}
ORIGINAL(recolour_rect, 0, 0, 0, 0x24bd0, 0x285e8);

}  // namespace game::red
