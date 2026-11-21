// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Red (the campaign module): the campaign clock (days, reinforcements),
// and small helpers of the map and text screens.
#include "game/mem/register.hpp"
#include "game/red/calls.hpp"
#include "game/red/vars.hpp"
#include "game/ui/state.hpp"

#include <algorithm>

namespace game::red {

// $207de: the clock moves on. Each new day the day timers count down,
// the guards are reset (while place 63 is on land), and every third day
// one of five reinforcements arrives (unless that is off): its numbers
// are taken from the reserves, the fronts move, its message is shown.
void advance_time(int16_t add_days, int16_t hours, int16_t minutes) {
  clock.minute += minutes;
  if (clock.minute <= 59) return;
  hours = int16_t(hours + clock.minute / 60);
  clock.minute = int16_t(clock.minute % 60);
  clock.hour += hours;
  if (clock.hour <= 23) return;
  add_days = int16_t(add_days + clock.hour / 24);
  clock.hour = int16_t(clock.hour % 24);
  scores.day_count += add_days;
  clock.day += add_days;
  const int16_t month = clock.month;
  if (clock.day > month_lengths[month]) {
    clock.month += 1;
    clock.day = int16_t(clock.day % month_lengths[month]);
  }
  days.arrival_countdown -= 1;
  for (int16_t& t : days.timers)
    if (t > 0) t -= 1;
  if (!at_sea(kGuardPlace))
    for (int i = 0; i < kGuards; ++i) days.timers[kGuardTimer + i] = 3;
  if (turn_flags.war_flag || turn_flags.no_arrivals || turn_flags.war_phase9) return;
  if (days.arrival_index >= kArrivals || days.arrival_countdown > 0) return;
  const int16_t arrival = arrivals[days.arrival_index];
  const int16_t a = int16_t((arrival >> 4) & 15), b = int16_t(arrival & 15);
  scores.reserve_a -= std::min<int16_t>(scores.reserve_a, a);
  scores.reserve_b -= std::min<int16_t>(scores.reserve_b, b);
  if (a != 0) {
    front_advance(0);
    front_advance(1);
  }
  message_page(arrival_messages[days.arrival_index]);
  if (!quit_key_pressed() && !turn_flags.no_picture_a && !turn_flags.no_picture_b) enter_map_screen();
  days.arrival_countdown = 3;
  days.arrival_index += 1;
}
ORIGINAL_IN("red", 0x207de, advance_time);

// $2079c: play the hours to run minute by minute (the enemy's steps and
// the clock).
void run_minutes() {
  turn_flags.minutes_to_run += int16_t(turn_flags.hours_to_run * 60);
  for (;;) {
    const int16_t n = turn_flags.minutes_to_run;
    turn_flags.minutes_to_run = int16_t(n - 1);
    if (n == 0) break;
    map_ticks() += 1;
    enemy_tick3();
    enemy_tick5();
    advance_time(0, 0, 1);
  }
  turn_flags.hours_to_run = 0;
  turn_flags.minutes_to_run = 0;
}
ORIGINAL_IN("red", 0x2079c, run_minutes);

// $2b1d4: the current menu: entry `index` of `list` (the left column).
void set_menu(int16_t index, const mem::Const<MenuEntry>& list) {
  ui::state().menu_keys = 28;
  ui::state().menu_index = index;
  ui::state().menu_side &= uint8_t(~kRightSide);
  ui::state().menu_list = list.addr();
}
ORIGINAL_IN("red", 0x2b1d4, set_menu);

// $2bb70: an order's code: v, or for 0 one from the order bits (8, 7 or 0).
int16_t order_code(int16_t v) {
  if (v) return v;
  return (loadout.flags & kRow28) ? 8 : (loadout.flags & kRow2_29) ? 7 : 0;
}
ORIGINAL_IN("red", 0x2bb70, order_code);

// $2403a: a character on the text page at the cursor, in the current
// style's colour; the cursor moves right, within the style's width.
void put_char(int16_t ch) {
  const screen::TextStyle& s = *ui::state().page_text_style;
  const int32_t at = ui::state().page_row * kPageRow + ui::state().page_col;
  page_byte(at + kPageColours) = uint8_t(s.fg);
  page_byte(at) = uint8_t(ch);
  const int16_t limit = int16_t(int16_t(s.x + s.width) / 8);
  ui::state().page_col = std::min<int16_t>(limit, int16_t(ui::state().page_col + 1));
}
ORIGINAL_IN("red", 0x2403a, put_char);

// $20996: the submarine, unit 7 and (unless player.spread is set)
// the battle groups that have members are drawn again.
void mark_redraw() {
  auto mark = [](int16_t u) { map_units[u].status |= kMoved | kArrived; };
  mark(kPlayerSub);
  mark(7);
  if (player.spread) return;
  if (groups[0].count > 0) mark(0);
  if (groups[1].count > 0) mark(1);
}
ORIGINAL_IN("red", 0x20996, mark_redraw);

// $220b4: whether a reinforcement comes (the more in reserve, the likelier).
int16_t reinforce_chance() {
  const int16_t n = std::max<int16_t>(int16_t(scores.reserve_a + scores.reserve_b - 14), 0);
  return rnd(8) > reinforce_thresholds[n] ? 1 : 0;
}
ORIGINAL_IN("red", 0x220b4, reinforce_chance);

// $21ede: the second battle group's mission, from the first one's list
// of related missions: the mission number itself (half
// the time, or when its string is empty), else one 1-16 places along its
// string (cyclically), minus 1.
int16_t pick_second_mission() {
  groups[1].count = -1;
  const int16_t m = phase.war_phase;
  const ConstPtr<int8_t> start = missions[m].chars;
  if (start[0] == 0 || rnd(2) != 0) return int8_t(m);
  ConstPtr<int8_t> p = start;
  int8_t ch = 0;
  for (uint16_t n = uint16_t(rnd(16) + 1); n != 0; --n) {
    ch = *p;
    p = p.addr + 1;
    if (ch == 0) {
      p = start;
      ch = *p;
    }
  }
  return int8_t(ch - 1);
}
ORIGINAL_IN("red", 0x21ede, pick_second_mission);

}  // namespace game::red
