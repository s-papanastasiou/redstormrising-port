// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Small arithmetic routines of the game's library and its random numbers,
// shared by the modules.
#pragma once

#include <cstdint>

namespace game::common {

int16_t abs_saturated(int16_t v);  // |v|, with abs(-32768) = 32767
int16_t sign(int16_t v);           // -1, 0 or 1
int16_t clamp(int16_t v, int16_t lo, int16_t hi);  // lo if below, else hi if above
int16_t sin_mul(int16_t angle, int16_t r);  // r * sin(angle), angle in 1/65536 turns
int16_t cos_mul(int16_t angle, int16_t r);
int16_t log_scale(int16_t v);       // 16 steps per power of two
int16_t bearing(int16_t dx, int16_t dy);  // the direction of (dx, dy), 0 = +y, in 1/65536 turns
void course_to(int16_t x1, int16_t y1, int16_t x2, int16_t y2);  // into course_result_*
int16_t random_n(uint32_t n);      // 0..n-1 (random.cpp)
int16_t rnd(int16_t n);            // 0..n-1
int16_t rnd_inclusive(int16_t n);  // 0..n

}  // namespace game::common
