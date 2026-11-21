// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Small arithmetic routines of the game's library.
#include "game/common/math.hpp"
#include "game/common/vars.hpp"
#include "game/mem/register.hpp"

namespace game::common {

// $3297e: |v|, saturating.
int16_t abs_saturated(int16_t v) {
  if (v == INT16_MIN) return INT16_MAX;
  return v < 0 ? int16_t(-v) : v;
}
ORIGINAL(abs_saturated, 0x3297e, 0, 0, 0x25146, 0x290ee);

// $33130: -1, 0 or 1.
int16_t sign(int16_t v) { return v == 0 ? 0 : v > 0 ? 1 : -1; }
ORIGINAL(sign, 0x33130, 0, 0, 0x267a0, 0x2a0a0);

// $3314c: v limited to lo..hi (lo wins when they cross).
int16_t clamp(int16_t v, int16_t lo, int16_t hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}
ORIGINAL(clamp, 0x3314c, 0, 0, 0x267bc, 0x2a0bc);

// $33086: r * sin(angle), rounded, from the table of a quarter wave in
// 256 steps per turn (angle in 1/65536 turns).
int16_t sin_mul(int16_t angle, int16_t r) {
  const int16_t a = int16_t(angle >> 8);
  int16_t s;
  if (a >= 0) s = a < kQuarterTurn ? quarter_sines[a] : quarter_sines[2 * kQuarterTurn - a];
  else s = int16_t(-(a > -kQuarterTurn ? quarter_sines[-a] : quarter_sines[2 * kQuarterTurn + a]));
  return int16_t((int32_t(s) * r + 128) >> 8);
}
ORIGINAL(sin_mul, 0x33086, 0, 0, 0x266f6, 0x29ff6);

// $3310e: r * cos(angle).
int16_t cos_mul(int16_t angle, int16_t r) { return sin_mul(int16_t(angle + kQuarterTurn * 256), r); }
ORIGINAL(cos_mul, 0x3310e, 0, 0, 0x2677e, 0x2a07e);

// $33050: a logarithm in 16 steps per power of two: 16 * the highest
// bit set + the 3 bits below it (with the top bit, 8-15); 0 for 0.
int16_t log_scale(int16_t v) {
  uint16_t bits = uint16_t(v);
  if (bits == 0) return 0;
  int16_t top = 15;
  while (int16_t(bits) >= 0) {  // until the top bit is set
    bits = uint16_t(bits << 1);
    --top;
  }
  return int16_t(top * 16 + (bits >> 12));
}
ORIGINAL(log_scale, 0x33050, 0, 0, 0, 0);

// $331e8: the direction of (dx, dy): 0 along +y, a quarter turn along +x;
// within each octant an arctangent of min / max by a polynomial.
int16_t bearing(int16_t dx, int16_t dy) {
  if (dx == 0) return dy > 0 ? int16_t(0) : kAngleHalf;
  if (dy == 0) return dx > 0 ? kAngleQuarter : kAngleThreeQuarters;
  const int16_t ax = abs_saturated(dx), ay = abs_saturated(dy);
  const bool x_major = ax > ay;
  const int16_t ratio =
      int16_t((int32_t(x_major ? ay : ax) << 14) / int32_t(x_major ? ax : ay));
  const int32_t bend = (int32_t(abs_saturated(int16_t(kAtanCentre - ratio))) * kAtanSlope) >> 14;
  const int16_t a = int16_t((int32_t(ratio) * (kAtanBase - bend)) >> 14);
  if (dx > 0 && dy > 0) return x_major ? int16_t(kAngleQuarter - a) : a;
  if (dx > 0) return x_major ? int16_t(kAngleQuarter + a) : int16_t(kAngleHalf - a);
  if (dy > 0) return x_major ? int16_t(kAngleThreeQuarters + a) : int16_t(-a);
  return x_major ? int16_t(kAngleThreeQuarters - a) : int16_t(a + kAngleHalf);
}
ORIGINAL(bearing, 0x331e8, 0, 0, 0x26858, 0x2a158);

// $33174: the bearing from (x1, y1) to (x2, y2) (y grows southwards) and
// an approximate distance (the larger difference plus half the smaller).
void course_to(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
  const int16_t dx = int16_t(x2 - x1), dy = int16_t(y2 - y1);
  *course_result_bearing = bearing(dx, int16_t(-dy));
  const int16_t ax = abs_saturated(dx), ay = abs_saturated(dy);
  *course_result_range = ax > ay ? int16_t(ax + (ay >> 1)) : int16_t(ay + (ax >> 1));
}
ORIGINAL(course_to, 0x33174, 0, 0, 0x267e4, 0x2a0e4);

}  // namespace game::common
