// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Red (the campaign module): the per-frame update of the strategic map:
// the enemy units' steps (moving, arriving, detecting the player), then
// the player's submarine.
#include "game/audio/calls.hpp"
#include "game/mem/register.hpp"
#include "game/red/calls.hpp"
#include "game/red/vars.hpp"
#include "game/ui/state.hpp"

namespace game::red {

namespace {
bool seen(int16_t unit) { return map_units[unit].status & kSeen; }
bool within_range(int16_t unit, int16_t other) { return unit_distance(unit, other) <= map_units[unit].range; }
bool odd_tick() { return map_ticks() & 1; }

// A unit's move along its route: idle, or counting down its timer, then
// moving towards its destination until it is within 3; returns whether it
// arrived. `after_count`: the original of unit 6 tests the timer after the
// decrement, that of unit 3 before.
enum class Step { Idle, Waiting, Moved, Arrived };
Step route_step(int16_t unit, bool after_count) {
  UnitRecord& r = unit_records[unit];
  if (r.state == kIdle) return Step::Idle;
  if (r.state != kArrivedState) {
    const int16_t old = r.time;
    r.time = int16_t(old - 1);
    if ((after_count ? int16_t(old - 1) : old) > 0) return Step::Waiting;
    r.time = 0;
    if (dest_distance(unit) > 3) {
      move_unit(unit);
      return Step::Moved;
    }
  }
  return Step::Arrived;
}
}  // namespace

// $20338: unit 9 follows the mouse pointer.
void pointer_unit() { map_units[kPointer].pos = position(ui::state().mouse_pos); }
ORIGINAL_IN("red", 0x20338, pointer_unit);

// $22c64: unit `unit` detects units 0 and 1 within its range.
void detect(int16_t unit) {
  for (int16_t k = 0; k < 2; ++k)
    if (!seen(k) && within_range(unit, k)) map_units[k].status |= kSeen;
}
ORIGINAL_IN("red", 0x22c64, detect);

// $24938: whether unit `unit` has the player's submarine in range (it is
// then seen, and the unit's bit is set in seen_by_mask). While any unit
// has it, a unit ready to pursue (record flags) pursues it. Returns
// whether the submarine is seen.
int16_t in_range(int16_t unit) {
  const uint16_t bit = unit < 16 ? uint16_t(1u << unit) : 0;
  if (within_range(unit, kPlayerSub)) {
    map_units[kPlayerSub].status |= kSeen;
    phase.seen_by |= bit;
  } else {
    phase.seen_by &= uint16_t(~bit);
  }
  if (phase.seen_by && !turn_flags.no_pursuit) {
    const uint8_t f = unit_records[unit].flags;
    if ((f & kHunter) && ((f & kAlerted) || !(f & kEvent))) pursue(unit);
  }
  return seen(kPlayerSub) ? 1 : 0;
}
ORIGINAL_IN("red", 0x24938, in_range);

// $24796: unit 2 on patrol: an effect the first time, and it looks for
// the submarine.
void unit2_step() {
  if (!patrol(2)) {
    turn_flags.unit2_warned = 0;
    return;
  }
  if (!turn_flags.unit2_warned) {
    audio::play_effect(kEffectUnit2);
    turn_flags.unit2_warned = 1;
  }
  in_range(2);
}
ORIGINAL_IN("red", 0x24796, unit2_step);

// $247cc: unit 3 (on odd ticks) moves along its route; on arriving it
// picks its next destination. It looks for the submarine (an effect when
// it has it).
void unit3_step() {
  if (!odd_tick()) return;
  const Step s = route_step(3, false);
  if (s == Step::Arrived) {
    map_units[3].status |= kArrived;
    map_units[3].mode = 3;
    unit3_destination();
  }
  if ((s == Step::Moved || s == Step::Arrived) && in_range(3)) audio::play_effect(kEffectUnit3);
  map_units[3].status |= kMoved;
}
ORIGINAL_IN("red", 0x247cc, unit3_step);

// $22914: unit 6 (on even ticks) moves along its route; on arriving it
// picks its next route. It detects units 0 and 1 (an effect when one is
// newly detected).
void unit6_step() {
  if (odd_tick()) return;
  const Step s = route_step(6, true);
  if (s == Step::Arrived) {
    map_units[6].mode = 7;
    map_units[6].status |= kArrived;
    unit6_route();
  }
  if (s == Step::Moved || s == Step::Arrived) {
    const bool before = seen(0) || seen(1);
    detect(6);
    if ((seen(0) || seen(1)) && !before) audio::play_effect(kEffectUnit3);
  }
  map_units[6].status |= kMoved;
}
ORIGINAL_IN("red", 0x22914, unit6_step);

// $22bb6: unit 5 on patrol detects units 0 and 1 (those at sea are seen
// anyway); an effect when that changes, once.
void unit5_step() {
  if (!patrol(5)) {
    turn_flags.unit5_warned = 0;
    return;
  }
  const bool before = seen(0) || seen(1);
  if (unit_records[0].flags & kAtSea) map_units[0].status |= kSeen;
  if (unit_records[1].flags & kAtSea) map_units[1].status |= kSeen;
  detect(5);
  const bool after = seen(0) || seen(1);
  if (after != before && !turn_flags.unit5_warned) audio::play_effect(kEffectUnit5);
  turn_flags.unit5_warned = 1;
}
ORIGINAL_IN("red", 0x22bb6, unit5_step);

// $22b50: unit 7, when the map was redrawn: its step, and it detects
// units 0 and 1.
void unit7_step() {
  if (!map_redrawn()) return;
  UnitRecord& r = unit_records[7];
  if (r.state == kArrivedState) r.state = 0;
  unit7_move();
  map_units[7].status |= kMoved;
  detect(7);
}
ORIGINAL_IN("red", 0x22b50, unit7_step);

// $2458c, $245a0: the enemy's steps on ticks 3 and 5 of every 8.
void enemy_tick3() {
  if ((map_ticks() & 7) == 3) enemy_step(0);
}
ORIGINAL_IN("red", 0x2458c, enemy_tick3);
void enemy_tick5() {
  if ((map_ticks() & 7) == 5) enemy_step(1);
}
ORIGINAL_IN("red", 0x245a0, enemy_tick5);

// $2030e: the map's units, once per frame.
void map_frame() {
  fronts_step();
  unit5_step();
  unit6_step();
  unit7_step();
  unit2_step();
  unit3_step();
  enemy_tick3();
  enemy_tick5();
  scroll_input();
  pointer_unit();
}
ORIGINAL_IN("red", 0x2030e, map_frame);

// $21672: whether the submarine is at its base.
int16_t sub_at_base() { return unit_place_distance(kPlayerSub, kBase) < 3 ? 1 : 0; }
ORIGINAL_IN("red", 0x21672, sub_at_base);

// $2161e: whether the submarine, heading for one of its targets, is near
// enough to it.
int16_t sub_near_target() {
  if (mission_type(phase) != kMissionUnits || loadout.weapons[6] < 10) return 0;
  const int16_t i = mission_target(phase);
  return unit_place_distance(kPlayerSub, sub_targets[i]) < sub_target_limits[i] ? 1 : 0;
}
ORIGINAL_IN("red", 0x2161e, sub_near_target);

// $20694: 10 or 20 when the submarine is within 3 of unit 0 or 1 (in
// play); the menu request is cleared then. 0 otherwise.
int16_t sub_meets_unit() {
  for (int16_t k = 0; k < 2; ++k) {
    if (unit_records[k].state < 0) continue;
    if (unit_distance(kPlayerSub, k) < 3) {
      mission.request = 0;
      return k == 0 ? 10 : 20;
    }
  }
  return 0;
}
ORIGINAL_IN("red", 0x20694, sub_meets_unit);

// $22b78: unit 7 goes on to its next stop whose day timer has run out.
void unit7_move() {
  int16_t i;
  do {
    i = unit_records[7].state;
    map_units[7].pos = unit7_route[i];
    unit_records[7].state = int16_t((i + 1) % kUnit7Stops);
  } while (days.timers[i] != 0);
}
ORIGINAL_IN("red", 0x22b78, unit7_move);

// $22702: battle group `unit` turns to hunt the submarine (unless that
// is not possible now): fast, a wider range, an alarm the first time;
// the place of the sighting is the submarine's position.
void pursue(int16_t unit) {
  const uint16_t bit = unit < 16 ? uint16_t(1u << unit) : 0;
  if (sheltered()) return;
  UnitRecord& r = unit_records[unit];
  r.speed_class = 1;
  r.time = 270;
  set_speed(unit, r.hunts > r.max_speed ? int16_t(r.max_speed) : int16_t(r.hunts));
  map_units[unit].range = 10;
  r.flags = uint8_t((r.flags & ~kAlerted) | kEvent);
  if (!phase.short_ranges) audio::play_effect(kEffectAlert);
  phase.short_ranges |= bit;
  phase.alerted &= uint16_t(~bit);
  places[kSightingPlace].pos = map_units[kPlayerSub].pos;
}
ORIGINAL_IN("red", 0x22702, pursue);

// $2c2ee: whether the submarine is in one of three areas where it is
// not pursued.
int16_t sheltered() {
  const int16_t x = map_units[kPlayerSub].pos.x, y = map_units[kPlayerSub].pos.y;
  if (y >= 110 && x <= 60) return 1;
  if (y >= 158 && x >= 97 && x <= 217) return 1;
  if (y >= 126 && y <= 222 && x >= 164 && x <= 222) return 1;
  return 0;
}
ORIGINAL_IN("red", 0x2c2ee, sheltered);
ORIGINAL_IN("red", 0x227c0, sheltered);  // a jump to it

}  // namespace game::red
