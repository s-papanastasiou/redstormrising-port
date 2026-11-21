// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Red (the campaign module): moving the units on the strategic map:
// distances, velocities towards a place, headings, following the orders
// of their scripts, the player's speed and the detection ranges.
#include "game/common/math.hpp"
#include "game/mem/register.hpp"
#include "game/red/calls.hpp"
#include "game/red/vars.hpp"
#include "game/screen/calls.hpp"
#include "game/ui/state.hpp"

namespace game::red {

namespace {
const Position& pos_of_unit(int16_t unit) { return map_units[unit].pos; }
const Position& pos_of_place(int16_t place) { return places[place].pos; }
void set_bit(uint32_t& v, uint32_t bit, bool on) { v = on ? (v | bit) : (v & ~bit); }
}  // namespace

// $2cb32: an approximate distance between two points, max + min / 2. The
// signs and sizes of the differences are kept (distance_*).
int16_t map_distance(const Position& p, const Position& q) {
  int16_t dx = int16_t(q.x - p.x);
  set_bit(distance.signs, kSignX, dx < 0);
  set_bit(distance.bits, kNegX, dx < 0);
  dx = common::abs_saturated(dx);
  distance.dx = dx;
  int16_t dy = int16_t(q.y - p.y);
  set_bit(distance.signs, kSignY, dy < 0);
  set_bit(distance.bits, kNegY, dy < 0);
  dy = common::abs_saturated(dy);
  distance.dy = dy;
  set_bit(distance.bits, kXMajor, dx >= dy);
  set_bit(distance.bits, kYMajor, dx < dy);
  const int16_t d = dy <= dx ? int16_t(dx + (dy >> 1)) : int16_t((dx >> 1) + dy);
  distance.result = d;
  return d;
}
ORIGINAL_INSIDE(map_distance, 0, 0, 0, 0, 0x2cb32);

// $2ca3c, $2ca76, $2cab4, $2caec: map_distance between places and units.
int16_t place_distance(int16_t a, int16_t b) { return map_distance(pos_of_place(a), pos_of_place(b)); }
ORIGINAL_IN("red", 0x2ca3c, place_distance);
int16_t unit_place_distance(int16_t unit, int16_t place) {
  return map_distance(pos_of_unit(unit), pos_of_place(place));
}
ORIGINAL_IN("red", 0x2ca76, unit_place_distance);
int16_t unit_distance(int16_t a, int16_t b) { return map_distance(pos_of_unit(a), pos_of_unit(b)); }
ORIGINAL_IN("red", 0x2cab4, unit_distance);
// The distance from a unit to its destination.
int16_t dest_distance(int16_t unit) { return unit_place_distance(unit, unit_records[unit].dest); }
ORIGINAL_IN("red", 0x2caec, dest_distance);

// $2c4ee: whether place `to` lies beyond place `from` along the unit's
// heading bits.
int16_t passed(int16_t unit, int16_t from, int16_t to) {
  const uint8_t h = unit_records[unit].heading;
  const Position &f = places[from].pos, &t = places[to].pos;
  if ((h & kHeadingRight) && t.x >= f.x) return 1;
  if ((h & kHeadingLeft) && t.x <= f.x) return 1;
  if ((h & kHeadingDown) && t.y >= f.y) return 1;
  if ((h & kHeadingUp) && t.y <= f.y) return 1;
  return 0;
}
ORIGINAL_IN("red", 0x2c4ee, passed);

// $2e0b6: the unit's compass heading 0-15 from its velocity.
int16_t unit_heading(int16_t unit) {
  const UnitRecord& r = unit_records[unit];
  const int16_t vx = r.vx, vy = r.vy;
  const bool left = r.flags & kMovingLeft, up = r.flags & kMovingUp;
  constexpr int16_t kDiagonal = 181, kShallow = 106;  // 256 cos 45, 256 tan 22.5
  int code;
  if (vx >= kDiagonal && vy >= kDiagonal) code = !left ? (up ? 2 : 6) : (up ? 14 : 10);
  else if (vx > kDiagonal) code = vy >= kShallow ? (!left ? (up ? 3 : 5) : (up ? 13 : 11)) : (left ? 12 : 4);
  else if (vx < kShallow) code = up ? 0 : 8;
  else code = !up ? (left ? 9 : 7) : (left ? 15 : 1);
  return int16_t((code + 14) & 15);
}
ORIGINAL_IN("red", 0x2e0b6, unit_heading);

// $2c6ca: the unit's velocity towards its destination: 256 along the
// main axis, in proportion along the other (181 each on a diagonal).
void set_velocity(int16_t unit) {
  UnitRecord& r = unit_records[unit];
  r.frac_x = 0;
  r.frac_y = 0;
  r.vx = 0;
  r.vy = 0;
  dest_distance(unit);
  const int16_t dx = distance.dx, dy = distance.dy;
  // The original shifts within 16 bits before dividing.
  auto ratio = [](int16_t a, int16_t b) { return int16_t(int32_t(int16_t(uint16_t(a << 8))) / b); };
  if (dx == dy) {
    r.vx = 181;
    r.vy = 181;
  } else if (dx < dy) {
    r.vx = ratio(dx, dy);
    r.vy = 256;
  } else {
    r.vy = ratio(dy, dx);
    r.vx = 256;
  }
  r.flags = uint8_t((r.flags & ~(kMovingUp | kMovingLeft)) | ((distance.signs & kSignY) ? kMovingUp : 0) |
                    ((distance.signs & kSignX) ? kMovingLeft : 0));
}
ORIGINAL_IN("red", 0x2c6ca, set_velocity);
ORIGINAL_IN("red", 0x24b2e, set_velocity);  // a jump to it

// $2c7f4: one step of the unit along its velocity (whole pixels when a
// fraction carries past 256).
void move_unit(int16_t unit) {
  MapUnit& u = map_units[unit];
  UnitRecord& r = unit_records[unit];
  u.status |= kMoved;
  auto axis = [](uint16_t& frac, int16_t v, bool negative, int16_t& pos) {
    frac += uint16_t(v);
    if (frac < 256) return;  // no whole pixel yet
    const int16_t whole = int16_t(int16_t(frac) >> 8);
    pos = int16_t(pos + (negative ? -whole : whole));
  };
  axis(r.frac_y, r.vy, r.flags & kMovingUp, u.pos.y);
  axis(r.frac_x, r.vx, r.flags & kMovingLeft, u.pos.x);
  r.frac_y &= 0xff;
  r.frac_x &= 0xff;
}
ORIGINAL_IN("red", 0x2c7f4, move_unit);

// $2c33c: the unit's heading bits: the main axis of the way from its
// destination to its target.
void set_heading_bits(int16_t unit) {
  UnitRecord& r = unit_records[unit];
  place_distance(r.dest, r.target);
  r.heading &= 0x0f;
  if (distance.dy > distance.dx) r.heading |= (distance.signs & kSignY) ? kHeadingUp : kHeadingDown;
  else r.heading |= (distance.signs & kSignX) ? kHeadingLeft : kHeadingRight;
}
ORIGINAL_IN("red", 0x2c33c, set_heading_bits);
ORIGINAL_IN("red", 0x24b1c, set_heading_bits);  // a jump to it

// $2c9c4: the unit's speed (and its step; the accumulator is primed).
void set_speed(int16_t unit, int16_t speed) {
  UnitRecord& r = unit_records[unit];
  r.speed = speed;
  r.step = uint16_t(speed << 3);
  r.progress = 255;
}
ORIGINAL_IN("red", 0x2c9c4, set_speed);
ORIGINAL_IN("red", 0x227cc, set_speed);  // a jump to it

namespace {
// The rest of taking an order: target, timer (in frames: script time
// * 60 / 8), speed (at most the unit's maximum), heading bits.
void take_order(int16_t unit, uint8_t target, uint8_t fallback, uint8_t time, int speed_at, int16_t order_phase) {
  UnitRecord& r = unit_records[unit];
  constexpr uint8_t kNone = 255;
  r.target = target == kNone ? fallback : target;
  r.speed_class = target == kNone ? 2 : 3;
  r.time = time == kNone ? int16_t(INT16_MAX) : int16_t(int16_t(time * 60) >> 3);
  const uint8_t script_speed = r.script[speed_at];
  set_speed(unit, r.max_speed > script_speed ? int16_t(script_speed) : int16_t(r.max_speed));
  set_heading_bits(unit);
  if (unit == 0) phase.order_phase = order_phase;
}
}  // namespace

// $22374: the unit's first order (script bytes 0-3: place, target,
// speed, time); a unit without a destination yet is put on the place.
void order_start(int16_t unit) {
  UnitRecord& r = unit_records[unit];
  const ConstPtr<uint8_t> s = r.script;
  const uint8_t place = s[0], target = s[1], time = s[3];
  if (r.dest == -1) {
    r.dest = place;
    map_units[unit].pos = places[place].pos;
  }
  set_velocity(unit);
  take_order(unit, target, place, time, 2, 1);
}
ORIGINAL_IN("red", 0x22374, order_start);

// $22498: the unit's next order (script bytes 4-7; place 250: keep the
// destination).
void order_next(int16_t unit) {
  UnitRecord& r = unit_records[unit];
  r.flags |= kOrderAdvanced;
  const uint8_t place = r.script[4];
  if (place != 250) r.dest = place;
  set_velocity(unit);
  take_order(unit, r.script[5], place, r.script[7], 6, 2);
}
ORIGINAL_IN("red", 0x22498, order_next);

// $2e03a: the headings of the player's submarine, the enemy and unit 8
// (which heads for the place that follows the submarine).
void init_headings() {
  const int16_t enemy = war.battle.situation == kSituationGroup0 ? 0 : 1;
  war.heading_player = unit_heading(kPlayerSub);
  war.heading_enemy = unit_heading(enemy);
  map_units[kUnit8].pos = map_units[enemy].pos;
  places[kPlayerPlace].pos = map_units[kPlayerSub].pos;
  unit_records[kUnit8].dest = kPlayerPlace;
  set_velocity(kUnit8);
  war.heading_unit8 = unit_heading(kUnit8);
}
ORIGINAL_IN("red", 0x2e03a, init_headings);

// $2c616: of the places in `list` (bytes, ended by -1) whose bit in
// `mask` is set, the nearest to place `from`; sometimes (rnd(8) > 5) the
// second nearest.
int16_t nearest_of(int16_t from, ConstPtr<int8_t> list, int16_t mask) {
  int16_t best = -1, second = -1, best_d = INT16_MAX, second_d = INT16_MAX;
  uint16_t bit = 1;
  for (int i = 0;; ++i, bit = uint16_t(bit << 1)) {
    const int16_t place = list[i];
    if (place == -1) break;
    if (!(mask & bit)) continue;
    const int16_t d = place_distance(place, from);
    if (d < best_d) {
      second = best;
      second_d = best_d;
      best = place;
      best_d = d;
    } else if (d < second_d) {
      second = place;
      second_d = d;
    }
  }
  if (second >= 0) return rnd(8) > 5 ? second : best;
  return best;
}
ORIGINAL_IN("red", 0x2c616, nearest_of);

// $2da54: restore the 16x8 cell where the unit was drawn in bitmap
// `frame` from the map (bitmap 2), while the map is shown.
void unit_frame(int16_t unit, int16_t frame) {
  if (!map_active()) return;
  const Position& at = map_units[unit].drawn[frame];
  screen::copy_region(screen::bitmap_at(kMapBitmap), screen::bitmap_at(frame), at.x, at.y, 16, 8);
}
ORIGINAL_IN("red", 0x2da54, unit_frame);

// $2c996: the detection ranges: the player's submarine, and the enemy's
// units 0, 1 and 3 (10 while short_ranges is set).
void set_ranges(int16_t player, int16_t enemy) {
  map_units[kPlayerSub].range = uint16_t(player);
  const int16_t r = phase.short_ranges ? 10 : enemy;
  map_units[3].range = uint16_t(r);
  map_units[1].range = uint16_t(r);
  map_units[0].range = uint16_t(r);
}
ORIGINAL_IN("red", 0x2c996, set_ranges);

// $2c922: the player's speed from the controls: still (the cursor stays
// on the submarine), fast while fire is held (unless not allowed), else
// normal; with the detection ranges that go with it.
void scroll_speed() {
  int16_t speed, own_range, enemy_range;
  if ((menu_dirs_red() & 15) == 0 || unit_records[kPlayerSub].speed == -1) {
    map_cursor() = ui_point(map_units[kPlayerSub].pos);
    speed = 5, own_range = 15, enemy_range = 3;
  } else if ((ui::state().menu_flags & kMenuFire) && !(loadout.unit_bits & 8)) {
    if (loadout.player_speed == 30) return;
    speed = 30, own_range = 5, enemy_range = 10;
  } else {
    if (loadout.player_speed == 15) return;
    speed = 15, own_range = 10, enemy_range = 5;
  }
  set_ranges(own_range, enemy_range);
  loadout.player_speed = speed;
  set_speed(kPlayerSub, speed);
}
ORIGINAL_IN("red", 0x2c922, scroll_speed);

}  // namespace game::red
