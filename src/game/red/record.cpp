// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Red (the campaign module): taking over the player's record when the
// module starts: the career (names, rank, level) and, for a campaign
// under way, its saved state; then the loadout options by rank and level
// and the score multiplier.
#include "game/mem/register.hpp"
#include "game/red/calls.hpp"
#include "game/red/texts_state.hpp"
#include "game/red/vars.hpp"
#include "game/ui/state.hpp"

#include <utility>

namespace game::red {

namespace {
void set_flag(uint32_t bit, bool on) { loadout.flags = on ? (loadout.flags | bit) : (loadout.flags & ~bit); }
}  // namespace

// $20f40: start the module from the player's record.
void load_campaign() {
  text_screen();
  data.unread_word = 0;
  const PlayerRecord& rec = *career();
  for (int i = 0; i < kNameBytes; ++i) texts().sub_name[i] = rec.sub_name[i];
  for (int i = 0; i < 32; ++i) texts().commander_name[i] = rec.commander[i];
  for (int i = 0; i < 5; ++i) player.rank_bits[i] = rec.rank_bits[i];
  player.spread = rec.spread;
  player.level = rec.level;
  player.rank = int16_t((int16_t(rec.rank) >> 3) & 7);
  player.rank_bit = int16_t(rec.rank & 7);
  if (rec.situation != kNoCampaign) {  // a campaign under way
    image::read(rec.clock, clock);
    image::read(rec.phase, phase);
    image::read(rec.groups, groups);
    image::read(rec.weapons_etc, loadout);
    image::read(rec.days, days);
    image::read(rec.battles, battles);
    image::read(rec.scores, scores);
    image::read(rec.war, war);
    image::read(rec.mission, mission);
    image::read(rec.units_left, units_left);
    war.situation = rec.situation;
  }
  const int16_t s = war.situation;
  if (s == 10 || s == 20 || s == 30) {  // back from a battle
    for (int i = 0; i < 7; ++i) loadout.weapons[i] = rec.weapons[i];
    for (int i = 0; i < 8; ++i) loadout.tubes[i] = int16_t((rec.tubes[i] & 127) - 1);
    loadout.unit_bits = rec.unit_bits;
    war.battle.count = rec.count_70;
    for (int i = 0; i < rec.count_70; ++i) {
      war.battle.members[i] = rec.words_72[i];
      war.battle.results[i] = rec.words_82[i];
    }
  }
  turn_flags.war_flag = (loadout.unit_bits & 0xf0) ? 1 : 0;
  turn_flags.war_phase9 = (phase.war_phase == 9 || phase.war_phase == 10) ? 1 : 0;
  no_help_request() = 1;
  turn_flags.no_picture_a = turn_flags.turn_over = turn_flags.no_picture_b = turn_flags.no_arrivals = turn_flags.names_shown = 0;
  turn_flags.unit5_warned = turn_flags.unit2_warned = 0;
  map_active() = 0;
  map_controls() = 0;
  turn_flags.no_pursuit = turn_flags.fronts_settling = 0;
  ui::state().unread_page_flag = 0;
  turn_flags.unread_flag_b = 0;
  map_redrawn() = 0;
  turn_flags.unread_flag_a = 0;

  // The loadout options by rank and level.
  loadout.flags = 0;
  const int16_t rank = player.rank, level = player.level;
  set_flag(kShortList, rank == 3);
  set_flag(kTubes5to8, rank == 4);
  set_flag(kRow1, level >= 2);
  set_flag(kRow5, loadout.flags & kRow1);
  set_flag(kRow2_29, level >= 1);
  set_flag(kRow3, loadout.flags & kRow2_29);
  set_flag(kRow28, level >= 3 && (loadout.flags & kTubes5to8));
  if (turn_flags.war_flag || war.situation == kNoCampaign) {  // a new campaign: the starting weapons
    for (int i = 0; i < 7; ++i) loadout.weapons[i] = starting_weapons[level * 35 + rank * 7 + i];
    loadout.capacity = starting_total[rank];
    loadout.on_board = starting_total[rank];
    free_tubes();
  }
  if (!(loadout.flags & kRow5)) loadout.weapons[5] = -3;
  if (!(loadout.flags & kRow1)) loadout.weapons[1] = -3;
  if (!(loadout.flags & kRow3)) {
    loadout.weapons[3] = -3;
    loadout.weapons[6] = -3;
    if (!(loadout.flags & kShortList)) loadout.weapons[4] = -3;
  }
  loadout.tube_count = int16_t((loadout.flags & kTubes5to8) ? 8 : 4);
  if (!turn_flags.war_flag && (loadout.unit_bits & 4)) {
    for (int16_t i = 3; i < loadout.tube_count; ++i) loadout.tubes[i] = kTubeFixed;
    loadout.tube_count = 3;
  }

  // The score multiplier.
  int16_t m = int16_t(int16_t(mul_by_spread[player.spread] * mul_by_level[level]) >> 4);
  m = int16_t(int16_t(m * mul_by_order[mission.order + 1]) >> 3);
  m = int16_t(int16_t(m * mul_by_rank[rank]) >> 1);
  tally.multiplier = uint16_t(m);
  reset_route_units();
  phase.seen_by = phase.hunters = phase.short_ranges = phase.alerted = 0;
  ui::state().menu_flags &= ~(kKeyRead | kCursorMoved);
  map_draw_bitmap() = 0;
}
ORIGINAL_IN("red", 0x20f40, load_campaign);

// $2dd10: before a battle (situation 10 or 20: with battle group 0 or 1;
// 30), the campaign's state goes into the player's record for Storm.
void save_campaign(int16_t situation) {
  effect_none();
  save_fronts();
  update_targets();
  turn_flags.names_shown = 0;
  war.situation = situation;
  mission.situation_copy = situation;
  const int16_t g = situation == kSituationGroup1 ? 1 : 0;
  war.battle = groups[g];
  if (mission.order == -1) {
    init_headings();
    count_terrain();
  }
  const int16_t request = mission.request;
  if (request == 0 || request == 4 || request == 5) conroom();
  PlayerRecord& rec = *career();
  rec.situation = war.situation;
  const int16_t t1 = war.terrain.colour_1;
  if (t1 > 17) rec.terrain = 1;
  else if (t1 > 1) rec.terrain = 2;
  else rec.terrain = int16_t(2 * war.terrain.fronts + war.terrain.colour_26) >= 16 ? 0 : 3;
  rec.enemy_heading = war.heading_enemy;
  rec.group_speed = unit_records[g].speed;
  rec.unit8_heading = war.heading_unit8;
  rec.count_70 = war.battle.count;
  for (int i = 0; i < 5; ++i) {
    rec.words_72[i] = war.battle.members[i];
    rec.words_82[i] = 0;
  }
  rec.player_speed = loadout.player_speed;
  rec.player_heading = war.heading_player;
  rec.unit_bits = loadout.unit_bits;
  for (int i = 0; i < 8; ++i) {
    const int16_t t = loadout.tubes[i];
    rec.tubes[i] = t > -1 ? uint16_t((t + 1) | 128) : 0;
  }
  for (int i = 0; i < 7; ++i) rec.weapons[i] = loadout.weapons[i] == -3 ? int16_t(0) : int16_t(loadout.weapons[i]);
  image::write(rec.clock, clock);
  image::write(rec.phase, phase);
  image::write(rec.groups, groups);
  image::write(rec.weapons_etc, loadout);
  image::write(rec.days, days);
  image::write(rec.battles, battles);
  image::write(rec.scores, scores);
  image::write(rec.war, war);
  image::write(rec.mission, mission);
  image::write(rec.units_left, units_left);
  rec.month = int16_t(clock.month - 1);
  rec.day = clock.day;
}
ORIGINAL_IN("red", 0x2dd10, save_campaign);

// $2dfc4: the mission's names are to be built again.

// $20ddc: a new campaign: everything cleared, the reserves at 10, a
// new phase and the fronts at their start, the date by the level, the
// units available; then load_campaign.
void new_campaign() {
  for (int row = 0; row < kPageRows; ++row)
    for (int i = 0; i < kPageColumns; ++i) {
      page_byte(row * kPageRow + i) = ' ';
      page_byte(row * kPageRow + kPageColours + i) = 3;
    }
  loadout = Loadout{};
  scores = Scores{};
  battles = Battles{};
  days = Days{};
  mission = MissionState{};
  turn_flags.hours_to_run = turn_flags.minutes_to_run = 0;
  tally = Tally{};
  scores.day_count += 1;
  scores.reserve_b = 10;
  scores.reserve_a = 10;
  scores.outcome.record_ending_rank = 0xff;
  days.arrival_countdown += 1;
  phase.war_phase = -1;
  new_phase();
  reset_fronts();
  PlayerRecord& rec = *career();
  mission.order = rec.order;
  player.level = rec.level;
  rec.final_rank = 0;
  rec.medals = 0;
  rec.score = 0;
  clock = {start_years[player.level], kStartMonth, kStartDay, kStartHour, 0};
  for (int i = 0; i < 32; ++i) units_left[i] = units_by_level[player.level * 32 + i];
  load_campaign();
}
ORIGINAL_IN("red", 0x20ddc, new_campaign);

// $20c12: the units with fixed routes reset (records 2-7: states,
// timers, ranges; unit 7 at its first stop).
void reset_route_units() {
  unit_records[4].state = 0;
  unit_records[4].speed = -1;
  unit_records[2].state = -1;
  unit_records[5].state = -1;
  map_units[6].range = 16;
  map_units[3].range = 16;
  unit_records[6].time = 0;
  unit_records[3].time = 0;
  unit_records[6].state = -1;
  unit_records[3].state = -1;
  map_units[7].range = 6;
  unit_records[7].time = 0;
  unit_records[7].state = -1;
  map_units[7].pos = unit7_route[0];
}
ORIGINAL_IN("red", 0x20c12, reset_route_units);

// $2184e: the submarine, units 0 and 1 back where the record left them;
// the map cursor on the submarine.
void restore_positions() {
  map_units[kPlayerSub].pos = mission.positions[0];
  map_cursor() = ui_point(map_units[kPlayerSub].pos);
  map_units[0].pos = mission.positions[1];
  map_units[1].pos = mission.positions[2];
}
ORIGINAL_IN("red", 0x2184e, restore_positions);

}  // namespace game::red
