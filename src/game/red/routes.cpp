// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Red (the campaign module): where the units go: patrols, the next place
// on a route, the destinations of units 3 and 6, wandering, orders.
#include "game/common/math.hpp"
#include "game/mem/register.hpp"
#include "game/red/calls.hpp"
#include "game/red/vars.hpp"

namespace game::red {

namespace {
uint16_t bit16(int n) { return n >= 0 && n < 16 ? uint16_t(1u << n) : 0; }
}  // namespace

// $22b1c: whether a place is at sea (not drawn in the land colour).
int16_t at_sea(int16_t place) {
  const Position p = place_pos(place);
  return map_pixel(p.x, p.y) != kLand ? 1 : 0;
}
ORIGINAL_IN("red", 0x22b1c, at_sea);

// $2bfc2: a patrolling unit's step: a new patrol starts at a random
// point; the patrol bounces between y 0 and 300 (moving x by 910 at each
// bounce) and cycles x through 0-1919. The unit is in the view or not.
// Returns whether it is.
int16_t patrol(int16_t u) {
  UnitRecord& r = unit_records[u];
  MapUnit& unit = map_units[u];
  if (r.state == -1) {
    r.state = 0;
    unit.range = 4;
    const bool left = rnd(2) != 0;
    r.flags = uint8_t((r.flags & ~(kMovingLeft | kMovingUp)) | (left ? kMovingLeft : kMovingUp));
    const int16_t base = int16_t(u << 5);
    r.patrol_y = int16_t(int16_t(base + rnd(250)) % 250);
    r.patrol_x = int16_t(int16_t(base + rnd(320)) % 320);
  }
  const int16_t y = int16_t(r.patrol_y + ((r.flags & kMovingUp) ? -1 : 1));
  r.patrol_y = y;
  if (y > 300 || y < 0) {
    r.flags ^= kMovingUp | kMovingLeft;
    r.patrol_x = int16_t(r.patrol_x + 910);
  }
  r.patrol_x = int16_t(int16_t(r.patrol_x + ((r.flags & kMovingLeft) ? -1 : 1)) % kPatrolLength);
  unit.pos.x = r.patrol_x;
  unit.pos.y = y;
  const bool was_in = unit.status & kArrived;
  const screen::TextStyle& sv = *symbol_style;  // the map view: the symbols' style's area
  const View v{sv.x, sv.y, sv.width, sv.height};
  const bool in = y >= v.y && y <= int16_t(v.y + v.h) && unit.pos.x >= v.x && unit.pos.x <= int16_t(v.x + v.w);
  unit.status = uint8_t((unit.status & ~(kArrived | kMoved)) | (in ? kArrived : 0) | (in || was_in ? kMoved : 0));
  return in ? 1 : 0;
}
ORIGINAL_IN("red", 0x2bfc2, patrol);
ORIGINAL_IN("red", 0x24b34, patrol);  // a jump to it

// $2c5d4: one of the set bits of `mask` among bits 0..n-1, at random.
int16_t pick_bit(int16_t mask, int16_t n) {
  int16_t bit = -1;
  for (uint16_t left = uint16_t(rnd(n) + 1); left != 0;) {
    bit = int16_t(int16_t(bit + 1) % n);
    if (mask & bit16(bit)) --left;
  }
  return bit;
}
ORIGINAL_IN("red", 0x2c5d4, pick_bit);

// $2bcf2: a unit that has reached its route's end goes on 1-7 places
// along the route from there (places near land do not count for units at
// sea; others are skipped); otherwise it heads for the route's end.
void next_target(int16_t g) {
  UnitRecord& r = unit_records[g];
  int16_t place = r.target;
  if (place == r.dest) {
    ConstPtr<int8_t> route = places[r.dest].route;
    for (uint16_t n = uint16_t(rnd(7) + 1); n != 0;) {
      place = *route;
      route = route.addr + 1;
      // At the end the route starts again; the original still looks at
      // the kind of "place -1" (the bytes before the table).
      if (place == -1) route = places[r.dest].route;
      const int8_t kind = place_kind(place);
      if (kind == 0 || (kind == 1 && !(r.flags & kAtSea))) --n;
    }
  }
  r.dest = place;
  set_velocity(g);
}
ORIGINAL_IN("red", 0x2bcf2, next_target);

// $2bdb6: the next place on a unit's route towards its target: the
// target if the route reaches it, else one of the places beyond the
// current one (not back, and only sea places for units at sea): the only
// one, a random one when the target is far along the unit's heading, or
// the nearest to the target. A unit that cannot go on is lost. Places
// 58-62 lead on by fixed steps.
void next_place(int16_t g) {
  UnitRecord& r = unit_records[g];
  const int16_t from = r.from, here = r.dest, target = r.target;
  int16_t choice = -1;
  auto finish = [&] {
    r.from = here;
    r.dest = choice;
    set_velocity(g);
  };
  if (here == 62) {
    choice = 29;
    return finish();
  }
  if (here >= 58 && here < 62) {
    choice = int16_t(here - 1);
    return finish();
  }
  if (from != -1 && !passed(g, from, target)) {
    r.state = kLost;
    return;
  }
  const ConstPtr<int8_t> list = places[here].route;
  uint16_t mask = 0;
  int16_t count = 0, last = -1;
  for (int16_t i = 0;; ++i) {
    const int16_t p = list[i];
    if (p == -1) break;
    if (p == target) {
      choice = p;
      break;
    }
    if (p == from) continue;
    const int8_t kind = places[p].kind;
    if (kind != 0 && (kind != 1 || (r.flags & kAtSea))) continue;
    if (!passed(g, here, p)) continue;
    last = p;
    mask |= bit16(i);
    ++count;
  }
  if (choice >= 0) return finish();
  if (count == 0) {
    r.state = kLost;
    return;
  }
  if (count == 1) {
    choice = last;
    return finish();
  }
  place_distance(here, target);
  const bool far = ((r.heading & (kHeadingRight | kHeadingLeft)) && distance.dx > 120) ||
                   ((r.heading & (kHeadingDown | kHeadingUp)) && distance.dy > 75);
  choice = far ? int16_t(list[pick_bit(int16_t(mask), 8)]) : nearest_of(target, list, int16_t(mask));
  finish();
}
ORIGINAL_IN("red", 0x2bdb6, next_place);

// $2483e: unit 3's destination (kept in its state word): reached, it is
// cleared; none, a new one is chosen (35, 39, 52 or 61, then 1-8 places
// along that one's list; 61 means place 54) and the unit is put there
// with a timer of 45; else it heads for it.
void unit3_destination() {
  UnitRecord& r = unit_records[3];
  if (r.state == r.dest) {
    r.state = -1;
    return;
  }
  if (r.state != -1) {
    r.dest = r.state;
  } else {
    int16_t pick;
    if (rnd(10) < 5) pick = fronts[kFront35].last ? 35 : fronts[kFront39].last ? 39 : 52;
    else pick = rnd(10) < 3 ? 52 : 61;
    r.state = pick;
    int16_t place = 54;
    if (pick != 61) {
      const Const<int8_t>& list = pick == 52 ? unit3_route_52 : pick == 39 ? unit3_route_39 : unit3_route_35;
      int i = 0;
      for (uint16_t n = uint16_t(rnd(8) + 1); n != 0;) {
        place = list[i++];
        if (place == -1) i = 0;
        else --n;
      }
    }
    map_units[3].pos = places[pick].pos;
    r.dest = place;
    r.time = 45;
  }
  set_velocity(3);
}
ORIGINAL_IN("red", 0x2483e, unit3_destination);

// $229e4: unit 6's route: without one, it starts from its last place if
// that is at sea, else from one of its starts; with
// one, a new route is picked (1-16 accepted starts on) and a position on
// it, the unit is put there with a timer of 30. Then it heads for it.
void unit6_route() {
  UnitRecord& r = unit_records[6];
  int16_t place;
  if (!(uint16_t(r.state) & kRouteChosen)) {
    // As the original: half the time the old route's next place is
    // checked first; only if it is at sea is the last place tried.
    const bool route_blocked = rnd(2) && !at_sea(unit6_routes[r.route_pos + 8]);
    place = r.from;
    if (route_blocked || !at_sea(place)) {
      for (int i = 5;; --i) {
        place = unit6_starts[i];
        if (at_sea(place)) break;
      }
    }
    r.state = int16_t(uint16_t(r.state) | kRouteChosen);
  } else {
    int16_t i = 0;
    place = -1;  // (the loop runs at least once)
    for (int16_t n = int16_t(rnd(16) + 1); n > 0;) {
      place = unit6_starts[i++];
      if (place == -1) i = 0;
      else if (at_sea(place)) --n;
    }
    const int16_t route = int16_t(i - 1);
    r.state = route;
    r.from = place;
    map_units[6].pos = places[place].pos;
    r.time = 30;
    const int16_t start = int16_t(int16_t(route << 4) & ~15);
    int16_t pos = start;
    for (int16_t n = int16_t(rnd(16) + 1); n > 0;) {
      place = unit6_routes[pos++];
      if (place == -1) pos = start;
      else --n;
    }
    r.route_pos = int16_t(pos - 1);
  }
  r.dest = place;
  set_velocity(6);
}
ORIGINAL_IN("red", 0x229e4, unit6_route);

// $21a00: a compass direction turned at random by up to the spread.
int16_t wander(int16_t dir) {
  const int16_t turn = rnd(int16_t(2 * (player.spread + 1)));
  const int16_t sign = int16_t(2 * rnd(2) - 1);
  return int16_t(common::abs_saturated(int16_t(dir + turn * sign)) & 15);
}
ORIGINAL_IN("red", 0x21a00, wander);

// $22346: a unit's order by the phase: start (-1, 1) or next (2).
void order(int16_t unit) {
  if (phase.order_phase == -1 || phase.order_phase == 1) order_start(unit);
  else if (phase.order_phase == 2) order_next(unit);
}
ORIGINAL_IN("red", 0x22346, order);

}  // namespace game::red
