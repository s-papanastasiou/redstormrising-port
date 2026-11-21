// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Red (the campaign module): the turn of the strategic map (what happens
// when the submarine is at its base or near its target, new phases) and
// the map screen.
#include "game/audio/calls.hpp"
#include "game/common/math.hpp"
#include "game/input/calls.hpp"
#include "game/input/vars.hpp"
#include "game/mem/register.hpp"
#include "game/red/calls.hpp"
#include "game/red/texts_state.hpp"
#include "game/red/vars.hpp"
#include "game/screen/calls.hpp"
#include "game/ui/state.hpp"

namespace game::red {

namespace {
// The start of a new phase: the battle groups, the day's message, the
// campaign page and (unless suppressed) the picture screen.
void start_phase() {
  choose_missions();
  phase_values();
  start_group(0);
  start_group(1);
  mark_redraw();
  if (phase.history[0] != 31 && !turn_flags.phase_none) day_message();
  if (!quit_key_pressed()) campaign_page();
  if (turn_flags.no_pursuit) return;
  if (turn_flags.war_flag || turn_flags.phase_none) {
    port_screen();
    turn_flags.war_flag = 0;
  }
  enter_map_screen();
}

// The submarine meets its target: ten effects with pauses, then the
// fronts and targets are kept and the meeting is played.
void meet_target() {
  mission.request = kRequestMeet;
  phase.alerted = 0;
  phase.short_ranges = 0;
  loadout.weapons[6] -= 10;
  for (int i = 0; i < 10; ++i) {
    for (int16_t n = int16_t(rnd(30) + 30); n > 0; --n) screen::wait_tof();
    audio::play_effect(kEffectMeet);
  }
  save_fronts();
  update_targets();
  battle_result(1);
}
}  // namespace

// $20518: the map's turn. At the start of the war (no order phase yet) a
// phase starts. Otherwise: at the base the submarine reports (a new phase
// at phase 31); near its target the request goes to 5, then the meeting
// (6); while the order phase changes, the fronts move and a new phase
// starts.
void turn() {
  if (phase.order_phase == -1) return start_phase();
  if (sub_at_base()) {
    base_report();
    if (phase.war_phase != 31) {
      enter_map_screen();
      if (phase.order_phase == phase.last_order_phase || turn_check() || turn_flags.turn_over) return;
      fronts_turn();
    }
  } else if (sub_near_target()) {
    if (mission.request != kRequestNear) {
      mission.request = kRequestNear;
      if (phase.order_phase == phase.last_order_phase || turn_check() || turn_flags.turn_over) return;
      fronts_turn();
    } else {
      meet_target();
      fronts_turn();
    }
  } else {
    if (phase.order_phase == phase.last_order_phase || turn_check() || turn_flags.turn_over) return;
    fronts_turn();
  }
  new_phase();
  start_phase();
}
ORIGINAL_IN("red", 0x20518, turn);

// $20a86: the map screen in the bitmap not shown: cleared, its palette,
// the heading for the war's situation, shown (with a pause for some).
void map_screen() {
  const int16_t b = screen::shown_bitmap() == 0 ? 1 : 0;
  page_style->bitmap = uint16_t(b);
  screen::draw_to(b);
  screen::clear_screen();
  screen::set_palette(map_palette.array(screen::kPaletteWords));
  screen::set_apen(0);
  page_style->x = 0;
  page_style->y = 0;
  page_style->cursor_y = 0;
  page_style->cursor_x = 0;
  const int16_t s = war.situation;
  print_text(s == kNoCampaign ? situation_text_none.str() : s == kSituationNoBattle ? situation_text_40.str() : situation_text.str());
  screen::show_bitmap(b);
  if (s == -1 || s == 40 || s == 30) wait_seconds(1);
}
ORIGINAL_IN("red", 0x20a86, map_screen);

// $2cf7e: the mission's three names (by the mission order, or the war
// phase) into one line.
void mission_names() {
  const int16_t order = mission.order;
  const int16_t m = order == -1 ? phase.war_phase : int16_t((order << 2) + player.spread + 70);
  phase.mission_names[0] = name_table_a[m];
  phase.mission_names[1] = name_table_b[m];
  phase.mission_names[2] = name_table_c[m];
  texts().mission_line[0] = 0;
  for (int k = 0; k < 3; ++k) string_append(texts().mission_line, message2(int16_t(phase.mission_names[k])));
  turn_flags.names_shown = 1;
}
ORIGINAL_IN("red", 0x2cf7e, mission_names);

namespace {
// $2151c / $21532 (two copies): fewer than half of the enemy group are
// left.
bool half_lost() { return uint16_t(tally.after_a) < uint16_t(uint16_t(tally.before_a) >> 1); }

// $21548: every unit of the mission's target type in battle group 0 is
// sunk (no target type: whether the group's first member is).
bool targets_sunk() {
  const int16_t k = mission_target(phase);
  if (!k) return war.battle.results[0] == kMemberSunk;
  uint32_t units = type_masks[mission_target_types[k]];  // bit 31: unit 0
  for (int16_t unit = 31; unit >= 0; --unit, units >>= 1) {
    if (!(units & 1)) continue;
    for (int16_t i = 0; i < groups[0].count; ++i)
      if (groups[0].members[i] == unit && war.battle.results[i] != kMemberSunk) return false;
  }
  return true;
}
}  // namespace
ORIGINAL_INSIDE(half_lost, 0, 0, 0, 0, 0x2151c);
ORIGINAL_INSIDE(half_lost, 0, 0, 0, 0, 0x21532);
ORIGINAL_INSIDE(targets_sunk, 0, 0, 0, 0, 0x21548);

// $2146c: battle group 0's battle judged by the mission's type (two hours
// and some minutes pass first): 1 achieved, 2 failed (the next mission is
// set up), both through battle_result; 0 the mission goes on (type 5
// without the units it needs; other types without the war flag).
int16_t battle_won() {
  turn_flags.no_arrivals = 1;
  advance_time(0, 2, common::rnd(60));
  turn_flags.no_arrivals = 0;
  int16_t result;
  switch (mission_type(phase)) {
    case kMissionLosses:
    case kMissionLosses2: result = half_lost() ? 1 : 2; break;
    case kMissionSinkType: result = targets_sunk() ? 1 : 2; break;
    case kMissionUnits:
      if (!enough_units()) return 0;
      result = 2;
      break;
    default: return turn_flags.war_flag ? 1 : 0;
  }
  if (result == 2) mission_setup();
  battle_result(result);
  return 1;
}
ORIGINAL_IN("red", 0x2146c, battle_won);

// $20ce0: Red's start: a new campaign, or the saved one with the
// result of the battle Storm played (situation 10/20: with battle group
// 0/1; 30: a single mission; 40: none). Returns 2 when the campaign
// ended, else 4.
int16_t start_red() {
  bool lost_round = false, done = false, battle0 = false, regroup = false;
  if (war.situation == kNoCampaign) {
    new_campaign();
  } else {
    load_campaign();
    init_fronts(0);
    restore_positions();
    if (war.situation == kSituationNoBattle) {
      regroup = true;
    } else {
      scores.divisor += 1;
      tally_battle();
      switch (war.situation) {
        case kSituationGroup0:
          battle0 = true;
          break;
        case kSituationGroup1:
          if (turn_flags.war_flag || ((phase.mission_needs & 0xf0) == 16 && enough_units())) {
            battle_result(2);
            done = true;
          } else {
            lost_round = true;
          }
          break;
        case kSituationSingle:
          single_mission_end();
          return 2;
        default:
          break;
      }
    }
    if (battle0) {
      if (battle_won()) done = true;
      else lost_round = true;
    }
    if (done) {
      fronts_turn();
      new_phase();
    } else {
      if (lost_round) remove_losses();
      if (regroup || lost_round) {
        phase_values();
        start_group(0);
        start_group(1);
        mark_redraw();
      }
      if (lost_round) {
        score_factor();
        campaign_end();
        enter_map_screen();
      }
    }
  }
  return turn_flags.turn_over ? 2 : 4;
}
ORIGINAL_IN("red", 0x20ce0, start_red);

// $20210: Red's main loop: the map screen, then turns of the strategic
// map frame by frame until a battle (the request), the campaign's end or
// a single mission; each time the campaign is saved for Storm. Returns
// the request (below 4: what to do next).
int16_t red_main() {
  screen::set_pointer(ui::state().pointer_8x8);
  screen::hide_pointer();
  war.situation = career()->situation;
  map_double_buffer() = 1;
  map_screen();
  init_map_units();
  mission.request = start_red();
  if (quit_key_pressed()) mission.request = 3;
  int16_t result = 0;  // the original starts from its caller's d7 (never used before a turn sets it)
  while (mission.request >= 4) {
    if (war.situation == kSituationNoBattle) {
      result = mission.situation_copy;
      war.situation = result;
      mission.request = mission.request_copy;
    } else if (mission.order != -1) {
      result = single_mission();
      mission.request = 0;
    } else {
      if (mission.request != kRequestNear) mission.request = kRequestMeet;
      for (;;) {
        const uint32_t deadline = audio::frames() + 1;
        input::frame_tick();
        turn();
        if (turn_flags.turn_over) break;
        map_frame();
        screen::restore_pointer();
        draw_units();
        advance_time(0, 0, 1);
        result = sub_meets_unit();
        while (deadline > audio::frames()) screen::wait_tof();
        if (result || mission.request < kRequestMeet) break;
      }
    }
    map_double_buffer() = 0;
    save_campaign(result);
    map_double_buffer() = 1;
  }
  map_double_buffer() = 0;
  return mission.request;
}
ORIGINAL_IN("red", 0x20210, red_main);

// $20642: the order phase moves on (kept in phase.last_order_phase): phase 1
// needs no check (1); phase 2 sets up the mission (1 unless it is a
// "3x" one); phase 3 (and 2 with a 3x mission) is the end: the fronts
// and targets are kept, the result is lost (0).
int16_t turn_check() {
  const int16_t p = phase.order_phase;
  phase.last_order_phase = p;
  if (p == 1) return 1;
  if (p == 2) {
    mission_setup();
    if ((phase.mission_needs & 0xf0) != 48) return 1;
  } else if (p != 3) {
    return 0;  // not reached (the original returns its caller's d7)
  }
  save_fronts();
  update_targets();
  phase.last_order_phase = 4;
  battle_result(2);
  return 0;
}
ORIGINAL_IN("red", 0x20642, turn_check);

}  // namespace game::red
