// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// The game's random numbers (a 32-bit shift register), shared by Storm,
// RedStart and Red.
#include "game/common/math.hpp"
#include "game/common/vars.hpp"
#include "game/mem/register.hpp"
#include "game/common/state.hpp"

namespace game::common {

// $34446 (RedStart $2894e, Red $2f15e): one step of the generator (not
// called any more: random_n steps inline).
uint32_t random_step(uint32_t v) {
  const bool out = v >> 31;
  v <<= 1;
  if (out || v == 0) v ^= kRandomFeedback;
  return v;
}
ORIGINAL(random_step, 0x34446, 0, 0, 0x2894e, 0x2f15e);

// $3440c: a random number 0..n-1 (the low word of n; 0: 0..65535). The
// generator steps once per bit of n - 1, at least once.
int16_t random_n(uint32_t n) {
  const uint16_t range = uint16_t(n);
  uint32_t v = state().random_seed;
  uint16_t bits = uint16_t(range - 1);
  do {
    v = random_step(v);
    bits >>= 1;
  } while (bits != 0);
  state().random_seed = v;
  const uint16_t low = uint16_t(v);
  return int16_t(range == 0 ? low : (uint32_t(low) * range) >> 16);
}
ORIGINAL(random_n, 0x3440c, 0, 0, 0x28914, 0x2f124);

// $3335c: random_n for a word.
int16_t rnd(int16_t n) { return random_n(uint32_t(int32_t(n))); }
ORIGINAL(rnd, 0x3335c, 0, 0, 0x269cc, 0x2a2cc);

// $2d3aa (Storm): a random number 0..n.
int16_t rnd_inclusive(int16_t n) { return rnd(int16_t(n + 1)); }
ORIGINAL(rnd_inclusive, 0x2d3aa, 0, 0, 0, 0);

}  // namespace game::common
