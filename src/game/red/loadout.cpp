// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Red (the campaign module): the loadout screens: the weapons on board
// (counts, load times), the torpedo tubes and what they hold, the tube
// status rows, the status line with the war day and the PACT/NATO
// gauge.
#include "game/common/strings.hpp"
#include "game/mem/register.hpp"
#include "game/red/calls.hpp"
#include "game/red/texts_state.hpp"
#include "game/red/vars.hpp"
#include "game/screen/calls.hpp"
#include "game/ui/state.hpp"

namespace game::red {

namespace {
screen::TextStyle& style() { return *ui::state().page_text_style; }
screen::TextStyle& page_text_style() { return *ui::state().page_text_style; }
void print(const Const<char>& text) { print_text(text.str()); }
void at(int16_t col, int16_t row) {
  set_column(col);
  set_row(row);
}
bool flag(uint32_t bit) { return loadout.flags & bit; }
}  // namespace

// $2bad4: a weapon's name (a '*' as a space) with a plural ending for
// more than one, then the rest of the row cleared.
void print_name_count(int16_t count, int16_t code) {
  char text[2] = {plural_template[0], plural_template[1]};
  const uint8_t* name = reinterpret_cast<const uint8_t*>(skip_strings(name_list.str(), int16_t(order_code(code) + 2)));
  uint8_t last = 0;
  for (int i = 0; name[i] != 0; ++i) {
    last = name[i] == kNameEnd ? ' ' : name[i];
    text[0] = char(last);
    text_object(page_text_style(), text);
  }
  if (count > 1) {
    if (last == 'o') text_object(page_text_style(), plural_e.str());
    text_object(page_text_style(), plural_s.str());
  }
  clear_row_rest();
}
ORIGINAL_IN("red", 0x2bad4, print_name_count);

// $2b932: one row of the loading list: "add"/"remove" (kind 5: "add"
// only), the minutes, the weapon's name, the count on board.
void load_row(int16_t k, int16_t shown) {
  if (!shown) return;
  style().fg = 3;
  print(text_add);
  set_column(6);
  if (k != 5) print(text_remove);
  style().fg = 28;
  set_column(14);
  two_digits(load_minutes[k]);
  print(text_min);
  set_column(21);
  print_name(k);
  style().fg = 1;
  set_column(38);
  clear_cells(2);
  if (loadout.weapons[k] >= 0) two_digits(loadout.weapons[k]);
  new_line();
}
ORIGINAL_IN("red", 0x2b932, load_row);

// $2b396: the count of a weapon kind ("no" for 0) and its name.
void count_and_name(int16_t k) {
  const int16_t n = loadout.weapons[k];
  if (n) number_cells(n, 2, kBlankCell);
  else print(text_no);
  style().cursor_x += 16;
  ui::state().page_col = int16_t(style().cursor_x / 8);
  print_name_count(n, k);
}
ORIGINAL_IN("red", 0x2b396, count_and_name);

// $2b30e: the counts of kinds 6 and 4 at rows 16 and 17.
void two_counts() {
  style().fg = 16;
  at(2, 16);
  count_and_name(6);
  at(2, 17);
  count_and_name(4);
}
ORIGINAL_IN("red", 0x2b30e, two_counts);

// $2b6b4: the loading list (rows by the loadout flags), the total on
// board (kinds other than 4 and 5), and in the short list another total.
void loading_panel() {
  style().margin = 16;
  at(2, 4);
  load_row(0, 1);
  load_row(2, 1);
  load_row(3, flag(kRow3));
  if (!flag(kShortList)) load_row(6, flag(kRow3));
  load_row(1, flag(kRow1));
  print(text_total_on_board);
  loadout.on_board = 0;
  for (int16_t i = flag(kShortList) ? 3 : 6; i >= 0; --i)
    if (i != 5 && i != 4 && loadout.weapons[i] > 0) loadout.on_board += loadout.weapons[i];
  clear_cells(2);
  two_digits(loadout.on_board);
  if (flag(kRow5)) {
    at(2, 11);
    if (!flag(kShortList)) new_line();
    load_row(5, 1);
  }
  if (flag(kShortList)) {
    at(2, 14);
    load_row(4, 1);
    load_row(6, 1);
    style().fg = 24;
    at(38, 16);
    clear_cells(2);
    loadout.total_4_6 = int16_t(loadout.weapons[4] + loadout.weapons[6]);
    two_digits(loadout.total_4_6);
  }
  style().margin = 0;
}
ORIGINAL_IN("red", 0x2b6b4, loading_panel);

// $2b3fa: the weapons on board, one line per kind (kinds 5 and 6, and
// none on board, in another colour).
void weapon_list() {
  style().margin = 16;
  at(2, 4);
  for (int i = 0; i < 99; ++i) {
    const int16_t k = weapon_order[i];
    if (k == -1) break;
    const int16_t n = loadout.weapons[k];
    if (n == -3 || (k == 6 && flag(kShortList))) continue;
    style().fg = uint16_t(k == 5 || k == 6 || n == 0 ? 16 : 3);
    clear_cells(2);
    two_digits(n);
    text_object(page_text_style(), text_gap.str());
    print_name_count(n, k);
    new_line();
  }
  style().margin = 0;
}
ORIGINAL_IN("red", 0x2b3fa, weapon_list);

// $2b60c: tubes first..first+3: "n: " and the weapon loaded.
void tube_rows(int16_t first) {
  for (int16_t i = first; i < first + 4; ++i) {
    const int16_t load = loadout.tubes[i - 1];
    style().fg = uint16_t(load == kTubeEmpty ? 16 : 3);
    number_cells(i, 1, ' ');
    text_object(page_text_style(), text_colon.str());
    clear_row_rest();
    print_name(load);
    new_line();
  }
}
ORIGINAL_IN("red", 0x2b60c, tube_rows);

// $2b51c: the tubes 1-4 (and 5-8) in two halves of the screen.
void tubes_screen() {
  auto half = [](int16_t x, int16_t col, int16_t first) {
    style().margin = 0;
    style().x = x;
    style().width = int16_t((x == 16 ? 159 : 319) - x);
    at(col, 11);
    tube_rows(first);
  };
  half(16, 2, 1);
  if (flag(kTubes5to8)) half(176, 22, 5);
  style().margin = 0;
  style().x = 0;
  style().width = kPageWidth;
}
ORIGINAL_IN("red", 0x2b51c, tubes_screen);

// $2d352: a tube status row at column 24: "operational", "inoperative"
// or the hours until it is (in another colour).
void tube_status(int16_t i, int16_t inoperative) {
  set_column(24);
  const uint16_t fg = style().fg;
  if (!(loadout.unit_bits & tube_masks[i])) {
    print(text_operational);
  } else if (inoperative) {
    print(text_inoperative);
  } else {
    style().fg = 3;
    number_cells(tube_hours[i], 2, kBlankCell);
    print(text_hours);
  }
  new_line();
  style().fg = fg;
}
ORIGINAL_IN("red", 0x2d352, tube_status);

// $2d29c: the four tube status rows from row 19, with the heading
// ("All", "(1)" or "(5)" Torpedo Tubes) before the third.
void tube_status_rows(int16_t inoperative) {
  style().fg = 28;
  style().margin = 16;
  set_row(19);
  for (int16_t k = 0; k < 4; ++k) {
    if (k == 2) {
      const int16_t choice = int16_t((loadout.unit_bits & 4) ? 4 * (((loadout.flags >> 31) & 1) + 1) : 0);
      string_copy(number_text(), tube_choices.str() + uint32_t(choice));
      string_append(number_text(), text_torpedo_tubes.str());
      print_text(number_text());
    }
    tube_status(k, inoperative);
  }
  style().margin = 0;
}
ORIGINAL_IN("red", 0x2d29c, tube_status_rows);

// $2d75e: the PACT/NATO gauge: ten blocks, the reserves above 13 from
// the right in the NATO colour.
void gauge_row() {
  const int16_t sum = int16_t(scores.reserve_a + scores.reserve_b);
  const int16_t filled = sum >= 13 ? int16_t(sum - 13) : 0;
  at(0, 24);
  clear_row_rest();
  style().fg = 3;
  print(text_pact);
  set_column(35);
  style().cursor_x += 5;
  style().fg = 16;
  print(text_nato);
  set_column(5);
  const uint16_t font = style().font;
  style().font = symbol_style->font;
  for (int16_t k = 10; k > 0; --k) {
    style().fg = uint16_t(filled >= k ? 16 : 3);
    print(text_gauge_block);
  }
  style().font = font;
}
ORIGINAL_IN("red", 0x2d75e, gauge_row);

// $2d6a6: the status line: the score factor and the war day, then the
// gauge.
void status_line() {
  at(5, 23);
  style().fg = 7;
  string_copy(number_text(), text_er.str());
  common::format_long(uint16_t(scores.factor), number_text() + 4);
  print_text(number_text());
  set_column(scores.day_count >= 10 ? 25 : 26);
  string_copy(number_text(), text_war_day.str());
  common::format_long(uint32_t(int32_t(scores.day_count)), number_text() + 9);
  print_text(number_text());
  gauge_row();
}
ORIGINAL_IN("red", 0x2d6a6, status_line);

// $2d85e: the whole screen as the style's area, the header and the
// score factor.
void print_header() {
  style().margin = 0;
  style().y = 0;
  style().height = 199;
  style().x = 0;
  style().width = kPageWidth;
  style().mode = 0;
  print(text_header);
  style().mode = 1;
  common::format_long(uint16_t(scores.factor), number_text());
  print_text(number_text());
}
ORIGINAL_IN("red", 0x2d85e, print_header);

// $2d00a: the mission summary texts and its count.
void mission_summary() {
  print(text_summary_a);
  print(text_tubes_at);
  style().fg = 23;
  if (flag(kRow5)) {
    at(0, 10);
    if (!flag(kShortList)) new_line();
    print(text_sail_magazine);
  }
  if (flag(kShortList)) print(text_summary_b);
  at(21, 3);
  style().fg = 23;
  number_cells(loadout.capacity, 2, kBlankCell);
}
ORIGINAL_IN("red", 0x2d00a, mission_summary);

// $2aaa2: loading the tubes: the ship status page and the weapons; a
// chosen weapon goes into the first free tube, a chosen tube (kRemove)
// is emptied back to the stock, until Escape.
void loadout_screen() {
  ui::state().page_text_style = page_style.addr();
  const int16_t b = screen::shown_bitmap() != 1 ? 1 : 0;
  screen::draw_to(b);
  style().bitmap = uint16_t(b);
  screen::clear_screen();
  screen::read_palette();
  set_menu(0, menu_split);
  ship_status_page();
  if (flag(kShortList)) two_counts();
  screen::set_palette(map_palette.array(screen::kPaletteWords));
  screen::show_bitmap(b);
  while (!quit_key_pressed()) {
    weapon_list();
    tubes_screen();
    const uint8_t code = uint8_t(choose(0, 14, menu_split, menu_split));
    if (code == kEscape) break;
    if (code & kRemove) {
      int16_t& tube = loadout.tubes[code & 127];
      if (tube == kTubeFixed) continue;
      loadout.weapons[tube] += 1;
      tube = kTubeFree;
      if (!mouse_clicked()) ui::state().menu_index += 1;
      continue;
    }
    if (loadout.weapons[code] <= 0 || code == kNotLoadable) continue;
    for (int16_t i = 0; i < loadout.tube_count; ++i) {
      if (loadout.tubes[i] != kTubeFree) continue;
      loadout.weapons[code] -= 1;
      loadout.tubes[i] = code;
      break;
    }
  }
  screen::default_palette();
}
ORIGINAL_IN("red", 0x2aaa2, loadout_screen);

// $2af04: the menu at the first tube system needing repair (else at 13).
void next_repair() {
  for (int16_t i = 0; i < 4; ++i)
    if (loadout.unit_bits & tube_masks[i]) {
      ui::state().menu_index = int16_t(i + 9);
      return;
    }
  ui::state().menu_index = 13;
}
ORIGINAL_IN("red", 0x2af04, next_repair);

// $2ac1c: the port: the tubes are emptied into the stock; then weapons
// are loaded or unloaded (taking their load time) and tube systems
// repaired (taking hours) until Escape, with the clock running.
void port_screen() {
  turn_flags.no_picture_b = 1;
  turn_flags.no_pursuit = 1;
  turn_flags.minutes_to_run = 0;
  turn_flags.hours_to_run = 0;
  bool redraw = true, panel = true, tubes = true;
  map_units[kPlayerSub].pos = places[kPortPlace10].pos;
  unit_records[kPlayerSub].speed = -1;
  set_menu(0, port_menu_left);
  for (int16_t i = 0; i < loadout.tube_count; ++i) {
    const int16_t t = loadout.tubes[i];
    if (t == kTubeFree || t == kTubeFixed) continue;
    loadout.weapons[t] += 1;
    loadout.tubes[i] = kTubeFree;
  }
  while (!quit_key_pressed()) {
    if (redraw) {
      redraw = false;
      const int16_t b = screen::shown_bitmap() != 1 ? 1 : 0;
      screen::draw_to(b);
      style().bitmap = uint16_t(b);
      screen::clear_screen();
      mission_summary();
      screen::set_palette(map_palette.array(screen::kPaletteWords));
      screen::show_bitmap(b);
    }
    style().fg = 7;
    at(0, 1);
    print_hhmm(clock);
    set_column(33);
    style().cursor_x -= 2;
    print_date(clock);
    if (panel) {
      panel = false;
      loading_panel();
    }
    if (tubes) {
      tubes = false;
      tube_status_rows(0);
    }
    const uint8_t code = uint8_t(choose(0, 13, port_menu_left, port_menu_right));
    if (code == kEscape) break;
    const int16_t k = code == kCodeSpecial ? (flag(kShortList) ? 1 : 6) : int16_t(code & 15);
    const bool right = ui::state().menu_side & kRightSide;
    bool changed = false;
    if ((code & 0xf0) == kCodeRepair) {
      if (loadout.unit_bits & tube_masks[k]) {
        loadout.unit_bits = uint16_t(loadout.unit_bits ^ uint16_t(tube_masks[k]));
        turn_flags.hours_to_run += tube_hours[k];
        if (!(loadout.unit_bits & 4)) {
          free_tubes();
          loadout.tube_count = int16_t(flag(kTubes5to8) ? 8 : 4);
        }
        tubes = true;
        next_repair();
      }
    } else {
      bool remove = false, full;
      if ((code & 0xf0) == kCodeWeapons) {
        full = loadout.weapons[k] >= 8;
      } else if ((code & 0xf0) == kCodeTorpedoes) {
        remove = right;
        full = loadout.total_4_6 >= 12;
      } else {
        remove = right;
        full = loadout.on_board >= loadout.capacity;
      }
      if (remove) {
        if (loadout.weapons[k] <= 0) continue;
        loadout.weapons[k] -= 1;
        changed = true;
      } else {
        if (full) continue;
        loadout.weapons[k] += 1;
        changed = true;
      }
      panel = true;
    }
    if (changed) turn_flags.minutes_to_run = load_minutes[k];
    ui::state().text_page_drawn = 0;
    run_minutes();
    if (ui::state().text_page_drawn) redraw = panel = tubes = true;
  }
  turn_flags.no_picture_b = 0;
  turn_flags.no_pursuit = 0;
  menu_dirs_red() = 0;
  effect_none();
}
ORIGINAL_IN("red", 0x2ac1c, port_screen);
ORIGINAL_IN("red", 0x20c96, port_screen);  // a jump to it

}  // namespace game::red
