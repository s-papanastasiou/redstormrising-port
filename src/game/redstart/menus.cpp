// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// RedStart: the first menu and the identification test.
#include "game/audio/calls.hpp"
#include "game/common/math.hpp"
#include "game/input/calls.hpp"
#include "game/input/vars.hpp"
#include "game/mem/call.hpp"
#include "game/mem/register.hpp"
#include "game/red/calls.hpp"
#include "game/redstart/calls.hpp"
#include "game/redstart/vars.hpp"
#include "game/screen/calls.hpp"
#include "game/ui/state.hpp"
#include "game/redstart/state.hpp"
#include "game/screen/styles.hpp"

#include <cstring>

namespace game::redstart {

namespace {
bool quit() { return ui::state().quit_key != 0; }

// A random ship of the test (0-19, not 3).
uint8_t random_ship() {
  uint8_t s = kIdentSkip;
  while (s == kIdentSkip || s > kIdentShips - 1) s = uint8_t(common::rnd(32));
  return s;
}
}  // namespace

// $203cc: the first menu: start a new game, resume, save, recall, view
// or add to the honor roll (some rows are not offered, by the module's
// start mode and the score); the file choices need the save disk. Returns
// the choice that leaves (start or resume: 1; quit: -1).
int16_t main_menu() {
  input::flush_keys();
  uint8_t choice = state().start_mode == 2 ? uint8_t(5) : state().start_mode == 1 ? uint8_t(2) : uint8_t(8);
  while (choice > 1 && !quit()) {
    const int16_t back = screen::shown_bitmap() == 0 ? int16_t(1) : int16_t(0);
    red::draw_to_window(back);
    screen::clear_screen();
    screen::set_palette(main_palette.array(screen::kPaletteWords));
    red::menu_page(page_main);
    screen::box(20, 8, 300, 174, 3);
    if (state().start_mode == 2 || state().start_mode == 0) {
      red::add_row(*menu_main, 1);
      red::add_row(*menu_main, 2);
    }
    if (!(state().start_mode == 2 && player()->score)) red::add_row(*menu_main, 5);
    screen::show_bitmap(back);
    choice = uint8_t(red::choose_row(*menu_main, int16_t(choice & 7)));
    if (quit()) continue;
    switch (choice) {
      case 2:
        if (save_disk_ok()) save_game();
        break;
      case 3:
        if (!save_disk_ok()) break;
        choice = uint8_t(recall_game());
        if (choice != kNotRecalled && identification_test()) player()->started = 1;
        break;
      case 4:
        if (!save_disk_ok()) break;
        view_honor_roll();
        choice = 4;
        break;
      case 5:
        if (!save_disk_ok()) break;
        add_to_honor_roll();
        choice = 4;
        break;
      default: break;
    }
  }
  return quit() ? int16_t(-1) : int16_t(choice);
}
ORIGINAL(main_menu, 0, 0, 0, 0x203cc, 0);

// $20d72: the warship identification test (for a new player): a random
// ship's picture, the player names it from the list. Returns whether the
// answer is right (1 without the test).
int16_t identification_test() {
  if (!state().new_player) return 1;
  if (!ui::state().shapes_preloaded) screen::need_disk();  // the ship file in memory: no need for disk B
  const int16_t back = screen::shown_bitmap() != 1 ? int16_t(1) : int16_t(0);
  red::draw_to_window(back);
  screen::clear_screen();
  const uint8_t ship = random_ship();
  red::menu_page(page_ident);
  screen::box(30, 14, 290, 82, 16);
  draw_spans(spans_ident, 3);
  screen::draw_masked(ident_shapes[ship], 160, 32, 0, 1);
  screen::show_bitmap(back);
  const uint8_t first = random_ship();
  return red::choose_row(*menu_ident, first) == ship ? 1 : 0;
}
ORIGINAL(identification_test, 0, 0, 0, 0x20d72, 0);

namespace {
int16_t back_of_one() { return screen::shown_bitmap() != 1 ? int16_t(1) : int16_t(0); }
int16_t back_of_zero() { return screen::shown_bitmap() == 0 ? int16_t(1) : int16_t(0); }
}  // namespace

// $20ee2: the new player's rank: the ranks offered in the year chosen (a
// new player's rank bits set first), or one at random by the year's odds;
// then a free bit of that rank (rank_code = row * 8 + bit).
void choose_rank() {
  if (!ui::state().shapes_preloaded) screen::need_disk();  // the ship file in memory: no need for disk B
  const int16_t back = back_of_zero();
  red::draw_to_window(back);
  screen::clear_screen();
  red::menu_page(page_rank);
  draw_spans(spans_rank, 3);
  screen::draw_masked(kRankShape, 10, 30, 0, 0);
  PlayerRecord& p = *player();
  if (state().new_player)
    for (uint8_t i = 0; i < 5; ++i) p.rank_bits[i] = rank_init[i];
  const int16_t row0 = int16_t(state().era * 4);
  for (uint8_t i = 0; i < kRankRows; ++i) {
    if (rank_table[row0 + i] == 128) red::add_row(*menu_rank, i);
    else state().rank_row = i;
  }
  if (state().era != 3) red::add_row(*menu_rank, kRankRows);
  screen::show_bitmap(back);
  state().rank_row = red::choose_row(*menu_rank, state().rank_row);
  if (state().rank_row == kRankRandom) {
    const uint8_t roll = uint8_t(common::rnd(128));
    for (uint8_t i = 0;; ++i) {
      state().rank_row = i;
      if (roll <= (rank_table[row0 + i] & 127) || i >= kRankRows) break;
    }
  }
  uint8_t bit;
  do bit = uint8_t(common::rnd(8)); while (p.rank_bits[state().rank_row] & rank_bits[bit]);
  state().rank_code = int16_t((state().rank_row << 3) | bit);
}
ORIGINAL(choose_rank, 0, 0, 0, 0x20ee2, 0);

// $21076: the assignment: the commander's name, rank and boat, for three
// seconds.
void show_assignment() {
  const int16_t back = back_of_zero();
  red::draw_to_window(back);
  screen::clear_screen();
  red::menu_page(page_assignment);
  const uint32_t style = (ui::state().window_style).addr;
  screen::text_str(screen::style_at(style), concat(state().commander_name, text_usn3.str()), 68, 80);
  const char* a = copy_static(text_n_ptr(rank_texts[state().rank_row]));
  screen::text_str(screen::style_at(style), concat(a, text_n_ptr(kRankText)), 40, 120);
  a = copy_static(text_n_ptr(int16_t(kRankCodeTexts + state().rank_code)));
  screen::text_str(screen::style_at(style), concat(text_uss2.str(), a), 40, 128);
  draw_spans(spans_assignment, 3);
  screen::show_bitmap(back);
  input::wait_seconds(3);
}
ORIGINAL(show_assignment, 0, 0, 0, 0x21076, 0);

// $21154: the difficulty chosen.
void choose_difficulty() {
  if (!ui::state().shapes_preloaded) screen::need_disk();  // the ship file in memory: no need for disk B
  const int16_t back = back_of_one();
  red::draw_to_window(back);
  screen::clear_screen();
  red::menu_page(page_difficulty);
  draw_spans(spans_difficulty, 3);
  ui::state().row_hook = mem::code_address(kEmptyHook);
  screen::draw_masked(kDifficultyShape, 20, 30, 0, 0);
  screen::set_palette(difficulty_palette.array(screen::kPaletteWords));
  screen::show_bitmap(back);
  state().difficulty = red::choose_row(*menu_difficulty, state().difficulty);
  ui::state().row_hook = mem::code_address(kZeroHook);
}
ORIGINAL(choose_difficulty, 0, 0, 0, 0x21154, 0);

// $211e0: the scenario chosen (some rows not offered).
void choose_scenario() {
  if (!ui::state().shapes_preloaded) screen::need_disk();  // the ship file in memory: no need for disk B
  const int16_t back = back_of_one();
  red::draw_to_window(back);
  screen::clear_screen();
  red::menu_page(page_scenario);
  screen::draw_masked(kScenarioShape, 0, 0, 0, 0);
  for (const int16_t row : {2, 3, 13, 14}) red::add_row(*menu_scenario, row);
  draw_spans(spans_scenario, 3);
  screen::set_palette(main_palette.array(screen::kPaletteWords));
  screen::show_bitmap(back);
  state().scenario = scenario_codes[red::choose_row(*menu_scenario, 0)];
}
ORIGINAL(choose_scenario, 0, 0, 0, 0x211e0, 0);

// $20d1c: the year chosen.
void choose_era() {
  (ui::state().row_colours)->width = 23;
  const int16_t back = back_of_one();
  red::draw_to_window(back);
  red::menu_page(page_era);
  draw_spans(spans_era, 3);
  screen::show_bitmap(back);
  state().era = red::choose_row(*menu_era, state().era);
}
ORIGINAL(choose_era, 0, 0, 0, 0x20d1c, 0);

// $20e64: the commander's name typed (after a space), then shown.
void enter_name() {
  red::menu_page(page_name);
  state().commander_name[0] = ' ';
  input_text(state().commander_name + 1, kNameLength, kNameX, kNameY);
  if (quit()) return;
  input::flush_keys();
  red::menu_page(page_name_done);
  screen::text_str(*ui::state().window_style, state().commander_name, 68, 106);
  draw_spans(spans_name, 3);
  input::wait_seconds(3);
}
ORIGINAL(enter_name, 0, 0, 0, 0x20e64, 0);

// $20ece: the page after a wrong identification ("Sorry, wrong answer!
// ... sent to the Submarine Training Facility"), three seconds.
void wrong_answer() {
  red::menu_page(page_wrong_answer);
  input::wait_seconds(3);
}
ORIGINAL(wrong_answer, 0, 0, 0, 0x20ece, 0);

// $2128c: the introduction: five pages, ten seconds each (a key skips).
void intro_sequence() {
  (ui::state().row_colours)->width = 28;
  if (!ui::state().shapes_preloaded) screen::need_disk();  // the ship file in memory: no need for disk B
  page_sequence(intro_pages, 5, 10, 1);
}
ORIGINAL(intro_sequence, 0, 0, 0, 0x2128c, 0);

// $212be: the introduction of scenario n (9, 10: their own; others: one
// at random) on a new screen.
void intro_page(int16_t n) {
  const int16_t k = (n == 9 || n == 10) ? int16_t(n - 5) : common::rnd(4);
  // The page's one block shows text intro_pictures[k] (the original wrote
  // it into the page in its data; a copy here), then the end block.
  const red::PageBlock* page = page_intro.array(2);
  red::PageBlock blocks[2] = {page[0], page[1]};
  blocks[0].first = intro_pictures[k];
  const int16_t back = back_of_one();
  red::draw_to_window(back);
  screen::clear_screen();
  (ui::state().row_colours)->width = 28;
  red::menu_page(blocks);
  screen::show_bitmap(back);
}
ORIGINAL(intro_page, 0, 0, 0, 0x212be, 0);

// $21330: the choices into the player's record: order, difficulty, rank,
// year, the boat's name, the commander, no campaign yet, 1 July.
void store_choices() {
  PlayerRecord& p = *player();
  p.order = int16_t(state().scenario - 1);
  p.spread = state().difficulty;
  p.rank = uint16_t(state().rank_code);
  p.level = state().era;
  string_copy(p.sub_name, text_n_ptr(int16_t(kRankCodeTexts + state().rank_code)));
  string_copy(p.commander, state().commander_name);
  p.situation = red::kNoCampaign;
  p.month = 6;
  p.day = 1;
}
ORIGINAL(store_choices, 0, 0, 0, 0x21330, 0);

// $20388: horizontal lines in `colour` from a list of spans.
void draw_spans(const mem::Const<Span>& spans, int16_t colour) {
  for (const Span* s = spans.until([](const Span& s) { return !s.x0 && !s.y; }); s->x0 || s->y; ++s)
    screen::line_colour(s->x0, s->y, s->x1, s->y, colour);
}
ORIGINAL(draw_spans, 0, 0, 0, 0x20388, 0);

// $24a02: a line typed into `buffer` (at most `length` characters,
// space to 'z'; Backspace deletes) at the text cell column, row of the
// window, after a '>' and with a '_' cursor, until Return (or 169, which
// sets the quit flag). Red has an unused copy ($29df2).
void input_text(char* text, int16_t length, int16_t column, int16_t row) {
  screen::TextStyle& style = *ui::state().window_style;
  ui::Point& at = ui::state().window_cursor;
  uint8_t n = 0;
  style.mode = 1;
  style.fg = 9;
  at.y = int16_t(uint8_t(row) << 3);
  at.x = int16_t((uint8_t(column) - 1) << 3);
  red::window_char('>');
  while (input::getkey()) {}
  ui::state().input_xs[n] = int16_t(uint8_t(column) << 3);
  at.x = ui::state().input_xs[n];
  red::window_char('_');
  for (;;) {
    const int16_t key = input::getkey();
    if (key == 13) break;
    const uint8_t ch = uint8_t(key);
    if (key == input::kKeyAltQ) {
      ui::state().quit_key = 1;
      break;
    }
    if (ch == 8 && n) {
      text[--n] = 0;
      at.x = ui::state().input_xs[n];
      red::window_char(text[n]);  // the original draws the 0 just written
      red::window_char('_');
      red::window_char(' ');
      red::window_char(' ');
    } else if (ch >= 32 && ch <= 122 && n < uint8_t(length)) {
      at.x = ui::state().input_xs[n];
      text[n++] = char(ch);
      text[n] = 0;
      red::window_char(ch);
      ui::state().input_xs[n] = at.x;
      red::window_char('_');
    }
  }
  style.mode = 0;
  while (input::getkey()) {}
}
ORIGINAL_INSIDE(input_text, 0, 0, 0, 0x24a02, 0);

// $24424: up to `count` pages shown in turn, `seconds` each, until a key
// (with control_only, only a key below space counts). Returns the key.
uint8_t page_sequence(const mem::Const<SequencePage>& pages, int16_t count, int16_t seconds, uint8_t control_only) {
  uint8_t key = 0;
  for (int i = 0; !quit() && count && !key; ++i) {
    const SequencePage& page = pages[i];
    red::show_text_page(text_n_ptr(int16_t(page.picture)));
    ui::state().menu_page_top = 112;
    red::menu_page(mem::Const<red::PageBlock>{pages.off + i * int32_t(sizeof(SequencePage))});  // its blocks
    ui::state().menu_page_top = 0;
    if (!key) key = uint8_t(input::wait_seconds(seconds));
    if (control_only && key >= 32) key = 0;
    --count;
  }
  return key;
}
ORIGINAL(page_sequence, 0, 0, 0, 0x24424, 0);

// $201f8: RedStart's main routine (mode: the module's start mode): the
// first menu, then for a new game the year, the identification test and,
// if it is passed, the name (a new player), rank, assignment, difficulty,
// scenario and its introduction; failing the test (the manual's copy
// check) gives the "wrong answer" page and scenario 9 in the first year
// at the lowest difficulty. Unless --protection is given the answer is
// not checked (the emulator NOPs the original's branch at $202dc, as
// JOTD's WHDLoad slave does). The choices go into the player's record. Returns -1
// (quit) or 0 (1 if the quit flag was already set).
int16_t start_main(int16_t mode) {
  if (quit()) return 1;
  state().unread_a = 0;
  state().unread_b = 0;
  ui::state().space_requests = 0;
  ui::state().frame_ticked = 0;
  ui::state().tick_frames = 0;
  ui::state().double_buffer = 0;
  ui::state().mouse_steering = 0;
  ui::state().no_help_request = 1;
  state().unread_c = 0;
  ui::state().flips = 0;
  ui::state().page_text_style = page_style.addr();
  PlayerRecord& p = *player();
  state().new_player = p.started == 0;
  if (state().new_player) p.commander[0] = 0;
  std::strncpy(state().commander_name, p.commander, 32);
  ui::state().text_list = mem::code_address(kTextList);
  state().start_mode = mode;
  ui::state().row_colours = row_style.addr();
  p.ending_rank = kNewEndingRank;
  state().start_mode_byte = uint8_t(mode);
  audio::play_effect(kStartEffect);
  red::draw_to_window(0);
  screen::set_palette(main_palette.array(screen::kPaletteWords));
  if (quit()) return -1;
  if (main_menu() == 0) {
    choose_era();
    if (quit()) return -1;
    const bool right = identification_test();
    if (right || mem::protection_bypassed()) {
      p.started = 1;
      if (!quit() && state().new_player) {
        ui::state().menu_page_top = kNamePageTop;
        enter_name();
        ui::state().menu_page_top = 0;
      }
      if (!quit()) choose_rank();
      if (!quit()) show_assignment();
      if (!quit()) choose_difficulty();
      if (!quit()) choose_scenario();
      if (!quit()) {
        if (state().scenario) intro_page(state().scenario);
        else intro_sequence();
      }
    } else if (!quit()) {
      ui::state().menu_page_top = kNamePageTop;
      wrong_answer();
      ui::state().menu_page_top = 0;
      state().era = 0;
      state().difficulty = 0;
      state().scenario = kDemoScenario;
      intro_page(kDemoScenario);
    }
    store_choices();
  }
  return quit() ? int16_t(-1) : int16_t(0);
}
ORIGINAL(start_main, 0, 0, 0, 0x201f8, 0);

}  // namespace game::redstart
