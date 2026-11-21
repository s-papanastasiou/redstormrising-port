// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Red (the campaign module): the front lines on the strategic map. Each
// front is a list of segments walked by up to 4 walkers that redraw the
// line (a pixel and its neighbours) in the advancing side's colour;
// stages move the fronts' ends.
#include "game/mem/register.hpp"
#include "game/red/calls.hpp"
#include "game/red/vars.hpp"
#include "game/screen/calls.hpp"

#include <algorithm>

namespace game::red {

namespace {
constexpr int16_t kScreenWidth = 320, kScreenHeight = 200;
bool on_screen(int16_t x, int16_t y) { return x >= 0 && x < kScreenWidth && y >= 0 && y < kScreenHeight; }
}  // namespace

// $2dcce: the colour of a map pixel (in the bitmap drawn into), -1 off
// the screen.
int16_t map_pixel(int16_t x, int16_t y) { return on_screen(x, y) ? int16_t(screen::read_pixel(x, y)) : -1; }
ORIGINAL_IN("red", 0x2dcce, map_pixel);

// $2dc4c: plot a map pixel in the map (bitmap 2) and, unless the fronts
// are being settled, in both shown bitmaps.
void plot_all(int16_t x, int16_t y, int16_t colour) {
  if (!on_screen(x, y)) return;
  const int16_t previous = screen::drawing_bitmap();
  screen::draw_to(kMapBitmap);
  screen::set_apen(colour);
  screen::plot(x, y);
  if (!turn_flags.fronts_settling) {
    for (int16_t b = 0; b < 2; ++b) {
      screen::draw_to(b);
      screen::set_apen(colour);
      screen::plot(x, y);
    }
  }
  screen::draw_to(previous);
}
ORIGINAL_IN("red", 0x2dc4c, plot_all);

// $2a866: a pixel and its four neighbours of colour `from` become `to`.
void recolour(int16_t x, int16_t y, int16_t from, int16_t to) {
  static constexpr int8_t kAround[5][2] = {{0, 0}, {0, -1}, {0, 1}, {-1, 0}, {1, 0}};
  for (const auto& d : kAround) {
    const int16_t px = int16_t(x + d[0]), py = int16_t(y + d[1]);
    if (uint16_t(map_pixel(px, py)) == uint16_t(from)) plot_all(px, py, to);
  }
}
ORIGINAL_IN("red", 0x2a866, recolour);

// $2a95c: a front's direction: 1 descending, redrawing colour A as B;
// otherwise ascending, B as A.
void front_direction(int16_t dir, Front& f) {
  if (dir == 1) {
    f.flags &= uint8_t(~kFrontAscending);
    f.colour_from = kFrontColourA;
    f.colour_to = kFrontColourB;
  } else {
    f.flags |= kFrontAscending;
    f.colour_from = kFrontColourB;
    f.colour_to = kFrontColourA;
  }
}
ORIGINAL_INSIDE(front_direction, 0, 0, 0, 0, 0x2a95c);

// $2a6d6: one step of a walker along its segment (the major axis every
// step, the minor axis every `period` steps). Returns the steps left.
int16_t walker_step(FrontWalker& w) {
  const int16_t left = w.steps;
  w.steps = int16_t(left - 1);
  if (left > 0) {
    const bool x_major = w.dir & kXMajor, neg_x = w.dir & kNegX, neg_y = w.dir & kNegY;
    bool minor = false;
    if (w.steps >= w.period) {
      const int16_t c = w.counter;
      w.counter = int16_t(c - 1);
      if (c <= 0) {
        w.counter = w.period;
        minor = true;
      }
    }
    auto move = [](int16_t& v, bool down) { v = int16_t(v + (down ? -1 : 1)); };
    if (minor) {
      if (x_major) move(w.pos.y, neg_y);
      else move(w.pos.x, neg_x);
    }
    if (x_major) move(w.pos.x, neg_x);
    else move(w.pos.y, neg_y);
  }
  return w.steps;
}
ORIGINAL_INSIDE(walker_step, 0, 0, 0, 0, 0x2a6d6);

// $2a76e: start the walker on the front's next segment. Returns 0 when
// the front's walk is over.
int16_t walker_next(Front& f, FrontWalker& w) {
  const bool ascending = f.flags & kFrontAscending;
  if (ascending ? f.next >= f.last : f.next <= f.last) return 0;
  w.segment = f.next;
  f.current = f.next;
  f.next = int16_t(f.next + (ascending ? 1 : -1));
  const MapPoint& a = f.points[2 * w.segment];
  const MapPoint& b = f.points[2 * w.segment + 1];
  const Position start = ascending ? a : b, end = ascending ? b : a;
  map_distance(start, end);
  w.dir = distance.bits;
  const int16_t major = std::max<int16_t>(distance.dx, distance.dy);
  const int16_t minor = std::min<int16_t>(distance.dx, distance.dy);
  w.steps = major;
  w.period = int16_t(major / int16_t(minor + 1));
  w.counter = w.period;
  w.pos = start;
  return 1;
}
ORIGINAL_INSIDE(walker_next, 0, 0, 0, 0, 0x2a76e);

// $2a67c: one move of a walker: recolour where it is, the front waits
// 10-19 frames, the walker steps; at the end of its segment it takes the
// next one, or stops. Returns its segment (-1: stopped).
int16_t walker_move(Front& f, FrontWalker& w) {
  recolour(w.pos.x, w.pos.y, f.colour_from, f.colour_to);
  f.countdown = int16_t(rnd(10) + 10);
  if (walker_step(w) < 0 && !walker_next(f, w)) {
    w.segment = -1;
    f.walking -= 1;
  }
  return w.segment;
}
ORIGINAL_INSIDE(walker_move, 0, 0, 0, 0, 0x2a67c);

// $2a5f0: each walking front, when its countdown runs out, moves one of
// its walkers (picked at random among those still walking).
void fronts_step() {
  for (int16_t i = 0; i < kFronts; ++i) {
    Front& f = fronts[i];
    if (f.walking <= 0) continue;
    const int16_t count = f.countdown;
    f.countdown = int16_t(count - 1);
    if (count > 0) continue;
    int16_t k = -1;
    for (int16_t n = int16_t(rnd(kWalkers) + 1); n > 0;) {
      k = int16_t((k + 1) % kWalkers);
      if (f.walkers[k].segment >= 0) --n;
    }
    walker_move(f, f.walkers[k]);
    f.flags |= kFrontActive;
  }
}
ORIGINAL_IN("red", 0x2a5f0, fronts_step);
ORIGINAL_IN("red", 0x20cd2, fronts_step);  // a jump to it

// $2a99a: keep the fronts' state (between screens).
void save_fronts() {
  for (int16_t i = 0; i < kFronts; ++i) {
    Front& f = fronts[i];
    mission.front_next[i] = int8_t(f.next);
    f.current = int8_t(f.next);
    mission.front_last[i] = int8_t(f.last);
    mission.front_stage[i] = int8_t(f.stage);
  }
}
ORIGINAL_IN("red", 0x2a99a, save_fronts);

// $2a4fa: front i falls back a stage.
void front_back(int16_t i) {
  Front& f = fronts[i];
  front_direction(1, f);
  f.current = f.next;
  f.stage = std::max<int16_t>(int16_t(f.stage - 1), 0);
  f.last = front_stage_ends[i * 7 + f.stage];
  save_fronts();
}
ORIGINAL_IN("red", 0x2a4fa, front_back);

// $2a560: front i advances a stage.
void front_advance(int16_t i) {
  Front& f = fronts[i];
  front_direction(2, f);
  const int16_t step = (f.flags & kFrontAscending) ? -1 : 1;
  f.current = std::max<int16_t>(int16_t(f.next + step), 0);
  f.stage = std::min<int16_t>(int16_t(f.stage + 1), kLastStage);
  f.last = front_stage_ends[i * 7 + f.stage];
  save_fronts();
}
ORIGINAL_IN("red", 0x2a560, front_advance);

// $2a922: fronts 0 and 1 fall back or advance, while the fronts move.
void fronts_turn() {
  if (phase.war_phase == 31 || tally.fronts_moving <= 0) return;
  auto move = war.situation == 0 ? front_back : front_advance;
  move(0);
  move(1);
}
ORIGINAL_IN("red", 0x2a922, fronts_turn);

// $2a446: draw the active fronts to where they are (in the map only).
void settle_fronts() {
  turn_flags.fronts_settling = 1;
  for (int16_t i = 0; i < kFronts; ++i) {
    Front& f = fronts[i];
    if (!(f.flags & kFrontActive) || f.current <= 0) continue;
    const bool ascending = f.flags & kFrontAscending;
    const int16_t last = f.last, current = f.current;
    front_direction(2, f);
    f.last = current;
    f.next = 0;
    if (walker_next(f, f.walkers[0])) {
      f.walking = 1;
      while (walker_move(f, f.walkers[0]) >= 0) {}
    }
    f.last = last;
    f.current = current;
    if (!ascending) front_direction(1, f);
  }
  turn_flags.fronts_settling = 0;
}
ORIGINAL_IN("red", 0x2a446, settle_fronts);

// $2a34a: the fronts from their kept state (drawn to it with `settle`),
// their walkers started.
void init_fronts(int16_t settle) {
  for (int16_t i = 0; i < kFronts; ++i) {
    Front& f = fronts[i];
    f.countdown = 0;
    f.walking = 0;
    f.next = mission.front_next[i];
    f.current = mission.front_next[i];
    f.last = mission.front_last[i];
    f.stage = mission.front_stage[i];
    f.flags = uint8_t(f.next > 0 ? (f.flags | kFrontActive) : (f.flags & ~kFrontActive));
    front_direction(f.next > f.last ? 1 : 2, f);
  }
  if (settle) settle_fronts();
  for (int16_t i = 0; i < kFronts; ++i) {
    Front& f = fronts[i];
    while (f.walking < kWalkers && walker_next(f, f.walkers[int16_t(f.walking)])) f.walking += 1;
  }
}
ORIGINAL_IN("red", 0x2a34a, init_fronts);

// $2a2e4: the fronts at their start (stage 1 for fronts 0 and 1).
void reset_fronts() {
  for (int16_t i = 0; i < kFronts; ++i) {
    Front& f = fronts[i];
    f.flags &= uint8_t(~kFrontActive);
    f.countdown = 0;
    f.walking = 0;
    f.stage = 0;
    f.current = 0;
    f.next = 0;
    f.last = 0;
    front_direction(2, f);
  }
  front_advance(0);
  front_advance(1);
  save_fronts();
}
ORIGINAL_IN("red", 0x2a2e4, reset_fronts);

// $2182c: the 8 tubes free.
void free_tubes() {
  for (int16_t& t : loadout.tubes) t = kTubeFree;
}
ORIGINAL_IN("red", 0x2182c, free_tubes);

}  // namespace game::red
