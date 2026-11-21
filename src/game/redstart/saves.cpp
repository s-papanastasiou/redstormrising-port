// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// RedStart: saved games (an index of five entries and a file per game).
#include "game/input/calls.hpp"
#include "game/mem/os.hpp"
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
int16_t other_bitmap() { return screen::shown_bitmap() == 0 ? int16_t(1) : int16_t(0); }
}  // namespace

// $20548: a file into the file buffer (size bytes); missing: the buffer
// cleared. Returns whether it was read.
int16_t load_file(const char* name, uint16_t size) {
  state().save_file = os::open(name, os::MODE_OLDFILE);
  if (state().save_file) {
    os::read(state().save_file, state().file_buffer, size);
    os::close(state().save_file);
    return 1;
  }
  std::memset(state().file_buffer, 0, size);
  return 0;
}
ORIGINAL_INSIDE(load_file, 0, 0, 0, 0x20548, 0);

// $20810: the current save entry on row `row` of a save menu: its rank,
// name and "USN", then its ship, day, month and level.
void save_line(int16_t row) {
  const SaveEntry& e = saves()[state().current_save];
  const uint32_t style = (ui::state().window_style).addr;
  const int16_t y = int16_t(row * 24 + 40);
  const char* t = concat(text_n_ptr(int16_t(e.rank)), e.name);
  t = concat(t, text_usn.str());
  screen::text_str(screen::style_at(style), t, 48, y);
  t = concat(text_uss.str(), e.ship);
  t = concat(t, text_comma.str());
  t = concat(t, format_thousands(e.day));
  t = concat(t, const_text(month_texts[e.month]));
  t = concat(t, const_text(level_texts[e.level]));
  screen::text_str(screen::style_at(style), t, 48, int16_t(y + 8));
}
ORIGINAL(save_line, 0, 0, 0, 0x20810, 0);

// $205b4: the game saved: into a free slot of the index (all full: the
// player chooses one to replace), the entry shown for confirmation, then
// the index and the game's file written.
void save_game() {
  red::draw_to_window(screen::shown_bitmap());
  screen::clear_screen();
  int16_t back = other_bitmap();
  red::draw_to_window(back);
  os::delay(60);
  load_file(games_file.str(), kSaveIndexBytes);
  uint8_t slot = 0;
  while (saves()[slot].rank != 0 && slot < kSaveSlots) ++slot;
  if (slot == kSaveSlots) {
    red::menu_page(page_saves_full);
    window_text_style->fg = uint16_t(int16_t((ui::state().row_colours)->y));
    for (int16_t k = 0; k < kSaveSlots; ++k) {
      state().current_save = uint32_t(k);
      save_line(k);
    }
    screen::show_bitmap(back);
    slot = uint8_t(red::choose_row(*menu_saves_full, kSaveSlots));
    if (slot == kSaveSlots) return;
    state().file_buffer[slot * int(sizeof(SaveEntry))] = 0;
  }
  SaveEntry& e = saves()[slot];
  const PlayerRecord& p = *player();
  e.rank = p.ending_rank;
  state().current_save = slot;
  string_copy(e.name, p.commander);
  string_copy(e.ship, p.sub_name);
  e.level = p.level;
  e.month = p.month;
  e.day = p.day;
  back = other_bitmap();
  red::draw_to_window(back);
  red::menu_page(page_save);
  window_text_style->fg = uint16_t(int16_t((ui::state().row_colours)->y));
  save_line(0);
  for (int16_t k = 1; k < kSaveSlots; ++k) red::add_row(*menu_save, k);
  screen::show_bitmap(back);
  if (red::choose_row(*menu_save, 0) != 0) return;
  state().save_file = os::open(games_file2.str(), os::MODE_NEWFILE);
  if (!state().save_file) return;
  os::write(state().save_file, state().file_buffer, kSaveIndexBytes);
  os::close(state().save_file);
  os::delay(120);
  std::memcpy(state().file_buffer, common::player_record_bytes() + 2, kSaveGameBytes);
  state().save_file = os::open(concat(game_file_prefix.str(), format_thousands(slot)), os::MODE_NEWFILE);
  if (!state().save_file) return;
  os::write(state().save_file, state().file_buffer, kSaveGameBytes);
  os::close(state().save_file);
}
ORIGINAL(save_game, 0, 0, 0, 0x205b4, 0);

// $208ea: a saved game recalled: the player chooses one of the index's
// entries (or exit); its file becomes the player's record.
int16_t recall_game() {
  os::delay(60);
  red::draw_to_window(screen::shown_bitmap());
  screen::clear_screen();
  const int16_t back = other_bitmap();
  red::draw_to_window(back);
  red::menu_page(page_recall);
  window_text_style->fg = uint16_t(int16_t((ui::state().row_colours)->y));
  load_file(games_file3.str(), kSaveIndexBytes);
  for (int16_t k = 0; k < kSaveSlots; ++k) {
    state().current_save = uint32_t(k);
    if (saves()[k].rank) save_line(k);
    else red::add_row(*menu_recall, k);
  }
  screen::show_bitmap(back);
  const uint8_t slot = uint8_t(red::choose_row(*menu_recall, kSaveSlots));
  if (slot >= kSaveSlots) return kNotRecalled;
  load_file(concat(game_file_prefix2.str(), format_thousands(slot)), kSaveGameBytes);
  std::memcpy(common::player_record_bytes() + 2, state().file_buffer, kSaveGameBytes);
  state().start_mode = 1;
  return kRecalled;
}
ORIGINAL(recall_game, 0, 0, 0, 0x208ea, 0);

// $20a3a: the medals held as a row of icons at the top.
void medal_row(int16_t medals) {
  int16_t x = 32;
  for (int16_t bit = 5; bit >= 0; --bit) {
    if (!(medals & (1 << bit))) continue;
    x = int16_t(x + 32);
    screen::draw_masked(medal_row_shapes[bit], x, 8, 0, 0);
  }
}
ORIGINAL(medal_row, 0, 0, 0, 0x20a3a, 0);

// $20a86: the honor roll: the five best officers (rank, name, score,
// final rank and medals), the best one's medals above, for 15 seconds.
void view_honor_roll() {
  screen::draw_to(screen::shown_bitmap());
  screen::clear_screen();
  const int16_t back = other_bitmap();
  red::draw_to_window(back);
  screen::clear_screen();
  os::delay(60);
  load_file(fames_file.str(), kFameBytes);
  if (!ui::state().shapes_preloaded) screen::need_disk();  // the ship file in memory: no need for disk B
  red::menu_page(page_honor_roll);
  state().current_fame = 0;
  if (fames()[0].rank) medal_row(int16_t(fames()[0].medals));
  const uint32_t style = (ui::state().window_style).addr;
  for (int16_t k = 0; k < kFameEntries; ++k) {
    const FameEntry& e = fames()[k];
    state().current_fame = uint32_t(k);
    window_text_style->fg = k == 0 ? 1 : 13;
    if (!e.rank) continue;
    const char* t = concat(text_n_ptr(int16_t(e.rank)), e.name);
    t = concat(t, text_usn2.str());
    t = concat(t, format_thousands(int16_t(e.score)));
    t = concat(t, text_er.str());
    screen::text_str(screen::style_at(style), t, 48, int16_t(k * 24 + 64));
    t = concat(text_n_ptr(int16_t(e.final_rank)), flag_names(int16_t(e.medals)));
    screen::text_str(screen::style_at(style), t, 48, int16_t(k * 24 + 72));
  }
  screen::set_palette(honor_palette.array(screen::kPaletteWords));
  screen::show_bitmap(back);
  input::wait_seconds(15);
}
ORIGINAL(view_honor_roll, 0, 0, 0, 0x20a86, 0);

// $20bfc: the player's name added to the honor roll in score order (the
// last place is always taken), the file written, then the roll shown.
void add_to_honor_roll() {
  os::delay(60);
  red::draw_to_window(screen::shown_bitmap());
  screen::clear_screen();
  load_file(fames_file2.str(), kFameBytes);
  const PlayerRecord& p = *player();
  for (int16_t k = 0; k < kFameEntries; ++k) {
    FameEntry& e = fames()[k];
    state().current_fame = uint32_t(k);
    if (!(p.score > e.score || k == kFameEntries - 1)) continue;
    for (uint16_t i = 160; i != uint16_t(k * 40 - 1); --i) state().file_buffer[i + 40] = state().file_buffer[i];
    e.rank = p.ending_rank;
    string_copy(e.name, p.commander);
    e.score = p.score;
    e.final_rank = p.final_rank;
    e.medals = p.medals;
    state().save_file = os::open(fames_file3.str(), os::MODE_NEWFILE);
    if (state().save_file) {
      os::write(state().save_file, state().file_buffer, kFameBytes);
      os::close(state().save_file);
      os::delay(120);
    }
    state().start_mode = 0;
    break;
  }
  view_honor_roll();
}
ORIGINAL(add_to_honor_roll, 0, 0, 0, 0x20bfc, 0);

// $209f4: the base text and the names of the medals held (bits 5 to 0),
// in a static buffer. Returns it.
const char* flag_names(int16_t medals) {
  const char* text = medal_base_text.str();
  for (int16_t bit = kMedalBits - 1; bit >= 0; --bit)
    if (medals & (1 << bit)) text = concat(text, const_text(medal_names[bit]));
  return copy_static(text);
}
ORIGINAL_INSIDE(flag_names, 0, 0, 0, 0x209f4, 0);

// $24626 (Red $29a16): the menus' per-row routine.

// $24e18: the packed text list.

// $23f32: the save disk is there (locked, or the requester asked for it
// and got it). Returns 1, or 0 when the player gave up.
int16_t save_disk_ok() {
  uint32_t lock = os::lock(save_volume.str(), os::ACCESS_READ);
  if (!lock) screen::disk_requester(insert_save_disk_text.str(), lock, 1);
  if (!lock) return 0;
  os::unlock(lock);
  return 1;
}
ORIGINAL(save_disk_ok, 0, 0, 0, 0x23f32, 0);

}  // namespace game::redstart
