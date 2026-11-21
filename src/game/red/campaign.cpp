// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Red (the campaign module): the war's phases and the enemy's battle
// groups (their speed, orders and events), scoring, and the player's
// submarine velocity from the input.
#include "game/audio/calls.hpp"
#include "game/input/calls.hpp"
#include "game/mem/register.hpp"
#include "game/red/calls.hpp"
#include "game/red/vars.hpp"

#include <algorithm>

namespace game::red {

namespace {
uint16_t bit16(int n) { return n >= 0 && n < 16 ? uint16_t(1u << n) : 0; }
constexpr int16_t kTowardsTarget = 12, kWaiting = 11, kDone = 10;  // group states
}  // namespace

// $220ec: the phase's two values.
void phase_values() {
  phase.mission_kind = phase_a_table[phase.war_phase];
  phase.mission_needs = phase_b_table[phase.war_phase];
}
ORIGINAL_IN("red", 0x220ec, phase_values);

// $22e48: count v in its class.
void count_class(int16_t v) {
  uint16_t& n = v < 9 ? scores.class_low : v < 13 ? scores.class_mid : v < 18 ? scores.class_high : scores.class_top;
  n += 1;
}
ORIGINAL_IN("red", 0x22e48, count_class);

// $215da: whether enough units are present for the phase.
int16_t enough_units() {
  if (turn_flags.war_flag) return 1;
  int16_t n = 0;
  for (int i = 0; i < 8; ++i) n = int16_t(n + ((loadout.unit_bits >> i) & 1));
  loadout.unit_count = n;
  return n < (phase.mission_needs & 15) ? 0 : 1;
}
ORIGINAL_IN("red", 0x215da, enough_units);

// $22ccc: the score factor: total / divisor * multiplier / 4 * a term
// clamped to 1..11, in 16-bit steps.
void score_factor() {
  uint16_t r = scores.divisor ? uint16_t(int16_t(scores.total / scores.divisor))
                                : uint16_t(scores.total);
  r = uint16_t(uint32_t(r) * tally.multiplier);
  r = uint16_t(r >> 2);
  const int16_t t = int16_t(scores.won + 2 - scores.lost);
  const int16_t factor = std::clamp<int16_t>(t, 1, 11);
  scores.factor = uint16_t(uint32_t(r) * uint16_t(factor));
}
ORIGINAL_IN("red", 0x22ccc, score_factor);

// $217a6: a new phase: the history and mask are updated, the phase state
// reset.
void new_phase() {
  for (int i = 3; i > 0; --i) phase.history[i] = phase.history[i - 1];
  const int16_t last = phase.war_phase;
  phase.history[0] = last;
  phase.mask = uint16_t(phase.mask << 1);
  if (last >= 3 && last <= 8) phase.mask |= 1;
  turn_flags.phase_none = last == -1 ? 1 : 0;
  phase.target_b = phase.target_a = phase.last_order_phase = phase.order_phase = phase.war_phase_b = phase.war_phase = -1;
  phase.seen_by = phase.hunters = phase.short_ranges = phase.alerted = 0;
  turn_flags.names_shown = 0;
}
ORIGINAL_IN("red", 0x217a6, new_phase);

// $2dfca: the positions of the submarine and units 0 and 1, and where
// units 0 and 1 head (their last place when that is the port).
void update_targets() {
  mission.positions[0] = map_units[kPlayerSub].pos;
  mission.positions[1] = map_units[0].pos;
  mission.positions[2] = map_units[1].pos;
  auto heading_for = [](const UnitRecord& r) -> int16_t {
    if (r.dest != kPortPlace) return r.dest;
    return r.from != -1 ? int16_t(r.from) : int16_t(r.target);
  };
  phase.target_a = heading_for(unit_records[0]);
  phase.target_b = heading_for(unit_records[1]);
}
ORIGINAL_IN("red", 0x2dfca, update_targets);
ORIGINAL_IN("red", 0x20c90, update_targets);  // a jump to it

// $222b4: a battle group's speed: that of its slowest member (at most
// 99); groups with members at sea are at sea.
void group_speed(int16_t g) {
  int16_t speed = 99;
  const int16_t count = groups[g].count;
  for (int16_t i = 0; i < count; ++i) {
    const int16_t member = groups[g].members[i];
    speed = std::min<int16_t>(speed, member_speeds[player.level * 32 + member]);
    if (member >= kFirstShipAtSea) unit_records[g].flags |= kAtSea;
  }
  unit_records[g].max_speed = speed;
}
ORIGINAL_IN("red", 0x222b4, group_speed);

// $249ca: a map unit's hit animation: a new hit (seen) starts it; then
// every 24 frames the frame steps 2, 18, 34, 49.
void unit_anim(int16_t n) {
  MapUnit& u = map_units[n];
  constexpr uint8_t kNotHit = 0x40;
  u.status = uint8_t((u.status & ~kNotHit) | ((u.status & kSeen) ? 0 : kNotHit));
  if (u.status & kSeen) {
    u.status |= kArrived;
    u.mode = 2;
    u.anim_timer = 12;
    u.status |= kMoved;
  } else {
    const int16_t t = u.anim_timer;
    u.anim_timer = int16_t(t - 1);
    if (t <= 0) {
      u.anim_timer = 24;
      switch (u.mode) {
        case 2: u.mode = 18; break;
        case 18: u.mode = 34; break;
        case 34: case 49: u.mode = 49; break;
        default: break;
      }
      u.status |= kMoved;
    }
  }
  u.status &= uint8_t(~kSeen);
}
ORIGINAL_IN("red", 0x249ca, unit_anim);

// $225aa: a battle group's event at the end of its wait: the next order
// (1), or it leaves the map; alerted groups head for their target fast
// and an alarm sounds, then go back to their orders. Returns 1 when it
// took its next order.
int16_t group_event(int16_t g) {
  UnitRecord& r = unit_records[g];
  const uint16_t bit = bit16(g);
  if (!(r.flags & kEvent)) {
    if (!(r.flags & kOrderAdvanced)) {
      order_next(g);
      return 1;
    }
    map_units[g].status &= uint8_t(~kArrived);
    r.state = kIdle;
    if (g == 0) phase.order_phase = 3;
    return 0;
  }
  if (!(r.flags & kAlerted)) {
    r.flags |= kAlerted;
    r.dest = r.target;
    set_velocity(g);
    r.time = 225;
    set_speed(g, 12);
    r.speed_class = 2;
    phase.alerted |= bit;
    phase.short_ranges &= uint16_t(~bit);
    audio::play_effect(kEffectAlert);
    return 0;
  }
  r.flags &= uint8_t(~(kAlerted | kEvent));
  if (r.flags & kOrderAdvanced) order_next(g);
  else order_start(g);
  phase.short_ranges &= uint16_t(~bit);
  phase.alerted &= uint16_t(~bit);
  return 0;
}
ORIGINAL_IN("red", 0x225aa, group_event);

// $2472c: a battle group on its way: 12 while it is 3 or more from its
// destination; at a place short of its target it picks the next (11);
// at its target it waits (11), then its event (11 or 10).
int16_t group_state(int16_t g) {
  if (dest_distance(g) >= 3) return kTowardsTarget;
  UnitRecord& r = unit_records[g];
  if (r.target != r.dest) {
    next_place(g);
    return kWaiting;
  }
  const int16_t t = r.time;
  r.time = int16_t(t - 1);
  if (t > 0) return kWaiting;
  return group_event(g) ? kWaiting : kDone;
}
ORIGINAL_IN("red", 0x2472c, group_state);

// $246cc: a battle group waiting (time 32767: for ever): its event when
// the wait is over; near its destination it goes on along the route (11);
// else 12.
int16_t group_wait(int16_t g) {
  UnitRecord& r = unit_records[g];
  if (r.time != INT16_MAX) {
    const int16_t t = r.time;
    r.time = int16_t(t - 1);
    if (t <= 0) return group_event(g) ? kWaiting : kDone;
  }
  if (dest_distance(g) < 3) {
    next_target(g);
    return kWaiting;
  }
  return kTowardsTarget;
}
ORIGINAL_IN("red", 0x246cc, group_wait);

// $22888: the submarine's velocity from the direction bits: full speed
// along the axes pressed, the signs from up and left.
void input_bits() {
  unit_records[kPlayerSub].vy = 0;
  unit_records[kPlayerSub].vx = 0;
  const uint16_t in = menu_dirs_red();
  if (!(in & 15)) return;
  constexpr int16_t kFull = 511;
  if (in & 12) unit_records[kPlayerSub].vy = kFull;
  if (in & 3) unit_records[kPlayerSub].vx = kFull;
  UnitRecord& r = unit_records[kPlayerSub];
  r.flags = uint8_t((r.flags & ~(kMovingUp | kMovingLeft)) | ((in & 8) ? kMovingUp : 0) | ((in & 2) ? kMovingLeft : 0));
}
ORIGINAL_IN("red", 0x22888, input_bits);

// $206e2: a mission's setup (missions 0-30) by its code: 1x: two day
// timers set to 7; 2x: a place marked on the map, and 3x: front x
// advances.
void mission_setup() {
  const int16_t mission = phase.war_phase;
  if (mission > 30) return;
  const int16_t code = mission_codes[mission];
  const int16_t kind = int16_t((code & 0xf0) >> 4), low = int16_t(code & 15);
  if (kind == 1) {
    const int16_t i = mission_timer_index[low];
    days.timers[i + 1] = 7;
    days.timers[i] = 7;
    return;
  }
  if (kind == 2) {
    const Position& p = places[mission_places[low]].pos;
    plot_all(p.x, p.y, 2);
  }
  if (kind == 2 || kind == 3) front_advance(low);
}
ORIGINAL_IN("red", 0x206e2, mission_setup);

// $21e48: whether the submarine must go home: (unless the spread is 2
// or more) at most 5 weapons left, or damage in two systems or more.
int16_t must_go_home() {
  if (player.spread >= 2) return 0;
  int16_t weapons = 0;
  for (int k = 0; k <= 4; ++k) weapons = int16_t(weapons + loadout.weapons[k]);
  for (int16_t i = 0; i < loadout.tube_count; ++i)
    if (loadout.tubes[i] > -1) ++weapons;
  int16_t damage = 0;
  for (int bit : {0, 3, 2, 1})
    if (loadout.unit_bits & (1 << bit)) ++damage;
  return (weapons <= 5 || damage >= 2) ? 1 : 0;
}
ORIGINAL_IN("red", 0x21e48, must_go_home);

// $21c1c: the next mission: home (31) when the submarine must go home,
// 9 or 10 when the reserves are very low or high; otherwise a weighted
// random pick among the missions whose rules allow them now.
int16_t pick_mission() {
  groups[0].count = -1;
  if (must_go_home()) return kMissionGoHome;
  const int16_t reserves = int16_t(scores.reserve_a + scores.reserve_b);
  if (reserves <= 13) {
    turn_flags.war_phase9 = 1;
    return kMissionLow;
  }
  if (reserves >= 23) {
    turn_flags.war_phase9 = 1;
    return kMissionHigh;
  }
  const bool second_half = rnd(2) != 0;
  int16_t total = 0;
  for (int16_t m = 0; m < kMissions; ++m) {
    const MissionRule& rule = mission_rules[m];
    const MissionRange& range = mission_ranges[m];
    mission_eligible[m] = 0;
    if (m >= 3 && m <= 8 && !(loadout.flags & kRow3)) continue;
    if (!(second_half ? rule.when_a : rule.when_b)) continue;
    if (rule.by_reserve_a && (scores.reserve_a < range.a_min || scores.reserve_a > range.a_max)) continue;
    if (rule.by_reserve_b && (scores.reserve_b < range.b_min || scores.reserve_b > range.b_max)) continue;
    if (rule.by_place && (at_sea(range.place) != 0) == bool(range.place & 0x80)) continue;
    mission_eligible[m] = 1;
    total = int16_t(total + missions[m].weight);
  }
  int16_t m = rnd(kMissions);
  for (int16_t left = int16_t(total + rnd(total)); left > 0;) {
    m = int16_t((m + 1) % kMissions);
    if (mission_eligible[m]) left = int16_t(left - missions[m].weight);
  }
  return m;
}
ORIGINAL_IN("red", 0x21c1c, pick_mission);

// $2189c: a single mission from the order (10: a random one): the enemy
// group, the terrain, headings, the date, the campaign page. Returns 30.
int16_t single_mission() {
  int16_t order = mission.order;
  if (order == kRandomOrder) {
    order = rnd(8);
    career()->order = order;
  }
  for (int i = 0; i < 32; ++i) units_available[i] = units_left[i];
  form_group(order_groups.array(5 * ((order << 2) + player.spread) + kGroupMembers) + 5 * ((order << 2) + player.spread), 0);
  group_speed(0);
  war.battle = groups[0];
  int16_t k = order;
  if (k >= 8) k = int16_t(k + ((k & 1) << 2) + player.spread);
  int16_t idx = 0;
  for (int16_t t = int16_t(rnd(16) + 1); t > 0;) {
    t = int16_t(t - order_terrain_odds[k * 4 + idx]);
    if (t > 0) idx = int16_t((idx + 1) % 4);
  }
  war.terrain.colour_1 = terrain_by_odds[idx * 4];
  war.terrain.colour_26 = terrain_by_odds[idx * 4 + 1];
  war.terrain.fronts = terrain_by_odds[idx * 4 + 2];
  war.terrain.other = terrain_by_odds[idx * 4 + 3];
  loadout.player_speed = 5;
  const int16_t h = rnd(16);
  war.heading_unit8 = h;
  war.heading_enemy = wander(h);
  war.heading_player = wander(int16_t((h + 8) & 15));
  phase.time = clock;
  campaign_page();
  mission.order = order;
  return kSingleMission;
}
ORIGINAL_IN("red", 0x2189c, single_mission);

// $21bd0: whether a mission was among the last four, or (3-8) whether
// one of those has been played recently.
int16_t recently_played(int16_t m) {
  for (int i = 0; i < 4; ++i)
    if (phase.history[i] == m) return 1;
  return (m >= 3 && m <= 8 && (phase.mask & 0xff)) ? 1 : 0;
}
ORIGINAL_IN("red", 0x21bd0, recently_played);

// $21aec: the missions of the two battle groups (not recent ones, and
// only with the units to form the group), and the date of the page.
void choose_missions() {
  int16_t m;
  for (;;) {
    m = pick_mission();
    phase.war_phase = m;
    for (int i = 0; i < 32; ++i) units_available[i] = units_left[i];
    if (m != kMissionGoHome && recently_played(m)) continue;
    if (form_group(mission_groups_0.array(5 * m + kGroupMembers) + 5 * m, 0)) break;
  }
  for (int i = 0; i < 32; ++i) units_left[i] = uint8_t(units_available[i]);
  for (;;) {
    const int16_t m2 = pick_second_mission();
    phase.war_phase_b = m2;
    for (int i = 0; i < 32; ++i) units_available[i] = units_left[i];
    if (form_group(mission_groups_1.array(5 * m2 + kGroupMembers) + 5 * m2, 1)) break;
  }
  if (m != kMissionGoHome) {
    scores.missions += 1;
    battles.fought += 1;
  }
  phase.time = clock;
  war.battle = groups[0];
}
ORIGINAL_IN("red", 0x21aec, choose_missions);

// $2168e: a battle's result (1: won): the score, the reserves gain or
// lose by the mission's table (doubled after day 30 and again after day
// 45), the fronts' movement, the situation; then the messages.
void battle_result(int16_t won) {
  const int16_t p = phase.war_phase;
  int16_t a, b;
  if (won == 1) {
    scores.won += 1;
    battles.won += 1;
    a = won_gains[2 * p];
    b = won_gains[2 * p + 1];
  } else {
    scores.lost += 1;
    battles.lost += 1;
    a = lost_losses[2 * p];
    b = lost_losses[2 * p + 1];
  }
  const int shift = (scores.day_count > 30) + (scores.day_count > 45);
  a = int16_t(a << shift);
  b = int16_t(b << shift);
  if (turn_flags.war_flag) {  // as the original: one less, then one more if that was not negative
    const int16_t v = int16_t(scores.reserve_b - 1);
    scores.reserve_b = v >= 0 ? int16_t(v - 1) : int16_t(0);
  }
  if (!turn_flags.war_phase9) {
    if (won == 1) {
      scores.reserve_a += a;
      scores.reserve_b += b;
    } else {
      scores.reserve_a = std::max<int16_t>(int16_t(scores.reserve_a - a), 0);
      scores.reserve_b = std::max<int16_t>(int16_t(scores.reserve_b - b), 0);
    }
  }
  tally.fronts_moving = a;
  tally.fronts_other = b;
  war.situation = won == 1 ? int16_t(0) : int16_t(war.battle.situation);
  score_factor();
  if (turn_flags.war_phase9) {
    end_phase();
  } else if (phase.war_phase != kMissionGoHome) {
    briefing_message();
    award_medal();
    news_report();
  }
}
ORIGINAL_IN("red", 0x2168e, battle_result);

// $2bbd8: a hunting group at its destination moves on towards the
// sighting place (kSightingPlace, put at the submarine when it is seen):
// to the place on its route nearest to it (sea, or near land for units
// not kept at sea), or (at the sighting place) only while the submarine is
// seen. Returns 0 when it does not (sheltered, or the submarine unseen).
int16_t hunt_check(int16_t g) {
  if (sheltered()) return 0;
  UnitRecord& u = unit_records[g];
  const int16_t dest = u.dest;
  if (dest == kSightingPlace) {
    if (!phase.seen_by) return 0;
    places[kSightingPlace].pos = map_point(map_units[kPlayerSub].pos);
  } else {
    if (phase.seen_by) places[kSightingPlace].pos = map_point(map_units[kPlayerSub].pos);
    int16_t nearest = unit_place_distance(g, kSightingPlace);
    int16_t best = kSightingPlace;
    for (ConstPtr<int8_t> next = places[dest].route; next[0] != -1; next = next.addr + 1) {
      const int16_t n = next[0];
      const int8_t kind = places[n].kind;
      if (kind != 0 && (kind != 1 || (u.flags & kAtSea))) continue;
      const int16_t d = place_distance(kSightingPlace, n);
      if (d < nearest) {
        nearest = d;
        best = n;
      }
    }
    if (best != kSightingPlace) u.target = best;
    u.dest = best;
  }
  set_velocity(g);
  return 1;
}
ORIGINAL_IN("red", 0x2bbd8, hunt_check);

// $24696: a hunting battle group: 12 while it is 3 or more from its
// destination; then 11 when it found something or its event says so.
int16_t group_hunt(int16_t g) {
  if (dest_distance(g) >= 3) return kTowardsTarget;
  if (hunt_check(g) || group_event(g)) return kWaiting;
  return kTowardsTarget;
}
ORIGINAL_IN("red", 0x24696, group_hunt);

// $245b6: battle group g's step (on ticks 3 and 5 of every 8): a group
// out of play (-3) takes the phase's mission again; otherwise by its
// speed class it hunts, waits or goes on, and moves; then it looks for
// the submarine and its symbol animates.
void enemy_step(int16_t g) {
  UnitRecord& r = unit_records[g];
  const int16_t state = r.state;
  if (state == kLost) {
    r.state = map_units[g].bit == 10 ? int16_t(phase.war_phase) : int16_t(phase.war_phase_b);
    r.from = -1;
    set_heading_bits(g);
    return;
  }
  if (state == kArrivedState || state == kIdle) return;
  int16_t result;
  switch (r.speed_class) {
    case 1: result = group_hunt(g); break;
    case 2: result = group_wait(g); break;
    case 3: result = group_state(g); break;
    default: return;
  }
  if (result == kDone) return;
  if (result == kTowardsTarget) {
    if (unit_step(g)) move_unit(g);
  } else if (result != kWaiting) {
    return;
  }
  in_range(g);
  unit_anim(g);
}
ORIGINAL_IN("red", 0x245b6, enemy_step);

// $22d40: after a battle, the tally of the enemy group's members by
// Storm's results (hit: an eighth of their value counts, sunk: none, and
// one fewer of the unit is available), and the score.
void tally_battle() {
  tally.score = 0;
  tally.hit = 0;
  tally.sunk = 0;
  tally.after_b = 0;
  tally.before_b = 0;
  tally.after_a = 0;
  tally.before_a = 0;
  tally.group_size = war.battle.count;
  for (int16_t i = 0; i < war.battle.count; ++i) {
    const int16_t m = war.battle.members[i];
    int16_t a = member_value_a[player.level * 32 + m];
    int16_t b = member_value_b[player.level * 32 + m];
    tally.before_a += a;
    tally.before_b += b;
    if (war.battle.results[i] == 1) {
      tally.hit += 1;
      a = int16_t(a >> 3);
      b = int16_t(b >> 3);
      count_class(m);
    } else if (war.battle.results[i] == 2) {
      if (int8_t(units_left[m]) > 0) units_left[m] -= 1;
      tally.sunk += 1;
      a = 0;
      b = 0;
      count_class(m);
    }
    tally.after_a += a;
    tally.after_b += b;
  }
  tally.lost_a = int16_t(tally.before_a - tally.after_a);
  scores.total_b += tally.lost_a;
  tally.lost_b = int16_t(tally.before_b - tally.after_b);
  scores.total += tally.lost_b;
  tally.score = int16_t(tally.sunk + tally.hit);
  if (war.situation != kSingleMission) after_tally();
}
ORIGINAL_IN("red", 0x22d40, tally_battle);

// $22e7a: the enemy group loses its hit and sunk members.
void remove_losses() {
  const int16_t g = war.battle.situation == kSituationGroup0 ? 0 : 1;
  int16_t& count = groups[g].count;
  count -= tally.score;
  if (count <= 0) return;
  int16_t j = 0;
  for (int16_t i = 0; i < war.battle.count; ++i)
    if (war.battle.results[i] == 0) groups[g].members[j++] = war.battle.members[i];
}
ORIGINAL_IN("red", 0x22e7a, remove_losses);

// $22ef2: after the tally: on a text page its number and a wait; in
// the war's end the promotion.
void after_tally() {
  if (no_help_request()) {
    print_text(text_tally.str());
    number_cells(tally.lost_b, 2, ' ');
    print_text(text_press_trigger.str());
    input::wait_key();
  }
  if (turn_flags.war_flag && war.situation != kSingleMission) promotion();
}
ORIGINAL_IN("red", 0x22ef2, after_tally);

}  // namespace game::red
