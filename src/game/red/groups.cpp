// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Red (the campaign module): forming and starting the enemy's battle
// groups, and drawing the units on the map.
#include "game/common/math.hpp"
#include "game/mem/register.hpp"
#include "game/red/calls.hpp"
#include "game/red/vars.hpp"
#include "game/screen/calls.hpp"
#include "game/ui/state.hpp"

namespace game::red {

namespace {
uint16_t bit16(int n) { return n >= 0 && n < 16 ? uint16_t(1u << n) : 0; }
}  // namespace

// $21f52: group g's members from a list of types (up to 5; a negative
// type only by reinforce_chance): for each a random available unit of
// the type. Returns the member count, 0 when a type has none.
int16_t form_group(const int8_t* list, int16_t g) {
  groups[g] = Group{};
  groups[g].situation = g == 0 ? kSituationGroup0 : kSituationGroup1;
  int16_t count = 0;
  for (int idx = 0; idx < kGroupMembers; ++idx) {
    const int8_t t = list[idx];
    if (t == 0) break;
    if (t < 0 && !reinforce_chance()) continue;
    const int8_t type = int8_t(common::abs_saturated(t));
    const uint32_t mask = type_masks[type];
    uint32_t available = 0;
    int16_t n = 0;
    for (int u = 0; u < 32; ++u) {
      const uint32_t b = kUnit0Bit >> u;
      if ((mask & b) && units_available[u]) {
        available |= b;
        ++n;
      }
    }
    if (n == 0) return 0;
    n = int16_t(n + rnd(n));
    int u = -1;
    do {
      u = (u + 1) % 32;
      if (available & (kUnit0Bit >> u)) --n;
    } while (n > 0);
    groups[g].members[count++] = int16_t(u);
    if (units_available[u] > 0) units_available[u] -= 1;
  }
  groups[g].count = count;
  return count;
}
ORIGINAL_INSIDE(form_group, 0, 0, 0, 0, 0x21f52);

// $22116: battle group g starts: its record cleared; without
// members it stays off the map. Otherwise its mission (the phase's),
// script, hunting, speed, destination and first order, and where it is
// drawn.
void start_group(int16_t g) {
  UnitRecord& r = unit_records[g];
  MapUnit& u = map_units[g];
  r = UnitRecord{};
  r.from = -1;
  if (groups[g].count <= 0) {
    u.status &= uint8_t(~kArrived);
    r.state = kIdle;
    r.speed_class = 0;
    return;
  }
  u.range = 5;
  const int16_t mission = g == 0 ? phase.war_phase : phase.war_phase_b;
  r.state = mission;
  r.script = (g == 0 ? group_scripts_0 : group_scripts_1).addr() + uint32_t(int32_t(mission) * kScriptBytes);
  r.hunts = group_hunts[g * kHuntRow + r.state];
  if (r.hunts) {
    r.flags |= kHunter;
    phase.hunters |= bit16(g);
  }
  group_speed(g);
  r.dest = g == 0 ? phase.target_a : phase.target_b;
  order(g);
  const int16_t x = int16_t(u.pos.x - u.offset.x), y = int16_t(u.pos.y - u.offset.y);
  u.drawn[1].x = x;
  u.drawn[0].x = x;
  u.drawn[1].y = y;
  u.drawn[0].y = y;
  u.shape = uint16_t((r.flags & kAtSea) ? kGroupShapeAtSea : kGroupShape);
}
ORIGINAL_IN("red", 0x22116, start_group);

// $2409c: a unit's symbol (one character) at (x, y) in colour `colour`,
// into the map bitmap being drawn.
void draw_symbol(int16_t ch, int16_t x, int16_t y, int16_t colour) {
  ui::state().symbol_text[0] = char(ch);
  symbol_style->fg = uint16_t(colour);
  symbol_style->bitmap = uint16_t(map_draw_bitmap());
  screen::draw_text(*symbol_style, x, y, ui::state().symbol_text);
}
ORIGINAL_IN("red", 0x2409c, draw_symbol);

// $20346: when the map was redrawn, the units on it: those drawn in this
// bitmap before are erased, then each visible one is drawn at its
// position (frozen ones where they were), in the colour of its mode
// (units 0 and 1 blink); unit 8 is never drawn.
void draw_units() {
  if (quit_key_pressed() || !map_redrawn()) return;
  map_redrawn() = 0;
  const int16_t f = map_draw_bitmap();
  for (int16_t n = 0; n < kMapUnits; ++n) {
    if (map_units[n].shown[f]) unit_frame(n, f);
    map_units[n].shown[f] = 0;
  }
  for (int16_t n = 0; n < kMapUnits; ++n) {
    MapUnit& u = map_units[n];
    Position& at = u.drawn[f];
    if (!(u.status & kFrozen)) {
      at.x = int16_t(u.pos.x - u.offset.x);
      at.y = int16_t(u.pos.y - u.offset.y);
    }
    if (n != kNotDrawn) {
      int16_t colour = u.mode;
      if (n <= 1 && colour == 2 && (map_count() & 8)) colour = 1;
      if (u.status & kArrived) {
        draw_symbol(int16_t(uint8_t(u.shape)), at.x, at.y, colour);
        u.shown[f] = 1;
      }
    }
    u.status &= uint8_t(~kMoved);
  }
}
ORIGINAL_IN("red", 0x20346, draw_units);

// $20b22: the ten map units cleared, then each one's symbol, drawing
// offset, mode (colour) and timer.
void init_map_units() {
  for (int16_t u = 0; u < kUnitCount; ++u) {
    map_units[u] = MapUnit{};
    map_units[u].bit = uint16_t(1u << u);
  }
  struct Setup {
    int16_t unit, shape, offset, mode, timer;  // shape 0: unchanged; timer -1: unchanged
  };
  static constexpr Setup kSetups[] = {
      {4, 33, 0, 7, -1}, {0, 0, 1, 2, 24}, {1, 0, 2, 2, 24}, {5, 36, 3, 7, -1}, {2, 36, 3, 3, -1},
      {6, 37, 4, 7, -1}, {3, 38, 5, 2, -1}, {7, 39, 6, 7, -1}, {9, 64, 1, 1, -1},
  };
  for (const Setup& s : kSetups) {
    MapUnit& u = map_units[s.unit];
    if (s.shape) u.shape = uint16_t(s.shape);
    u.offset = unit_offsets[s.offset];
    u.mode = s.mode;
    if (s.timer >= 0) u.anim_timer = s.timer;
  }
}
ORIGINAL_IN("red", 0x20b22, init_map_units);

}  // namespace game::red
