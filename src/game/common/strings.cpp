// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// The library's text helpers: numbers right-aligned in fields, the ship
// names, joining two strings.
#include "game/common/math.hpp"
#include "game/common/strings.hpp"
#include "game/common/vars.hpp"
#include "game/common/state.hpp"
#include "game/mem/register.hpp"

#include <cstring>

namespace game::common {

namespace {
// The number of characters of v in decimal (at most 5 counted; one more
// for a sign), from |v| against 10000, 1000, ...
int16_t digit_count(int32_t magnitude, bool negative) {
  int16_t n = kMaxDigits;
  for (int16_t p = kTopPower; p > 1 && magnitude < p; p = int16_t(p / 10)) --n;
  return negative ? int16_t(n + 1) : n;
}

// `digits` right-aligned in `width` characters of blanks, at `out`.
const char* right_align(char* out, const char* digits, int16_t width, int16_t count) {
  std::strcpy(out, blanks.str() + int16_t(kBlanks - width));
  std::strcpy(out + int16_t(width - count), digits);
  return out;
}
}  // namespace

// $32a0e (RedStart $251d6, Red $2917e): v right-aligned in a field of
// `width` characters. Returns the static buffer.
const char* format_number(int16_t v, int16_t width) {
  const int16_t count = digit_count(abs_saturated(v), v < 0);
  format_int(v, state().number_digits);
  return right_align(state().number_text, state().number_digits, width, count);
}
ORIGINAL_INSIDE(format_number, 0x32a0e, 0, 0, 0x251d6, 0x2917e);

// $32a88 (RedStart $25250, Red $291f8): the same for a long.
const char* number_field(int32_t v, int16_t width) {
  const int32_t magnitude = v < 0 ? int32_t(0u - uint32_t(v)) : v;  // labs
  const int16_t count = digit_count(magnitude, v < 0);
  format_long(v, state().long_digits);
  return right_align(state().long_text, state().long_digits, width, count);
}
ORIGINAL_INSIDE(number_field, 0x32a88, 0, 0, 0x25250, 0x291f8);

// $323ec (RedStart $26fc4, Red $284a0): the number of the ship called
// `name`, or kShipNames.
int16_t find_name(const char* name) {
  int16_t i = 0;
  while (i < kShipNames && std::strcmp(const_text(ship_names[i]), name) != 0) ++i;
  return i;
}
ORIGINAL_INSIDE(find_name, 0x323ec, 0, 0, 0x26fc4, 0x284a0);

// $243da (Red $297ca): a's characters up to the first one at or below 16
// (control codes; the terminator), then b over that one. Returns the
// static buffer.
const char* concat(const char* a, const char* b) {
  char* out = state().concat_text;
  const uint8_t* first = reinterpret_cast<const uint8_t*>(a);
  int i = 0;
  while (first[i] > kLastControl) {
    out[i] = char(first[i]);
    ++i;
  }
  std::strcpy(out + i, b);
  return out;
}
ORIGINAL_INSIDE(concat, 0, 0, 0, 0x243da, 0x297ca);

// $329b6 (RedStart $2517e, Red $29126): the angle (1/65536 turns) in
// whole degrees (rounded from 512ths of a turn), three digits with
// leading zeros. Returns the static buffer.
const char* format_degrees(int16_t angle) {
  const int16_t degrees = int16_t((int16_t(((angle >> 7) & 511) * 45) + 32) >> 6);
  std::strcpy(state().degrees_text, degrees_template.str());
  const int16_t zeros = degrees < 10 ? 2 : degrees < 100 ? 1 : 0;
  format_int(degrees, state().degrees_text + zeros);
  return state().degrees_text;
}
ORIGINAL_INSIDE(format_degrees, 0x329b6, 0, 0, 0x2517e, 0x29126);

// $32f2a (RedStart $272ea, Red $2969a): v in decimal with a comma before
// the hundreds when there are thousands. Returns the static buffer.
const char* format_thousands(uint16_t v) {
  uint8_t digits[kThousandsDigits];  // units first
  for (uint8_t& d : digits) {
    d = uint8_t(v % 10);
    v = uint16_t(v / 10);
  }
  int top = kThousandsDigits - 1;
  while (top > 0 && digits[top] == 0) --top;
  char* out = state().thousands_text;
  int n = 0;
  for (int k = top; k >= 0; --k) {
    if (k == 2 && k < top) out[n++] = ',';
    out[n++] = char('0' + digits[k]);
  }
  out[n] = 0;
  return out;
}
ORIGINAL_INSIDE(format_thousands, 0x32f2a, 0, 0, 0x272ea, 0x2969a);

// $249e0 (Red $29dd0): text into a static buffer. Returns it.
const char* copy_static(const char* text) {
  std::strcpy(state().static_text, text);
  return state().static_text;
}
ORIGINAL_INSIDE(copy_static, 0, 0, 0, 0x249e0, 0x29dd0);

// $32b06 (RedStart $252ce, Red $29276): itoa: v in decimal (a '-' first
// when negative; -32768 as -32767). Returns 2 (0 for v = 0), as the
// original's d0.
int16_t format_int(int16_t v, char* out) {
  int at = 0;
  if (v < 0) out[at++] = '-';
  int16_t n = abs_saturated(v);
  if (n == 0) {
    out[at++] = '0';
    out[at] = 0;
    return 0;
  }
  int i = kMaxDigits;
  while (n < int_powers[i]) --i;
  for (; i; --i) {
    const int16_t p = int_powers[i];
    out[at++] = char('0' + n / p);
    n = int16_t(n % p);
  }
  out[at] = 0;
  return 2;
}
ORIGINAL_INSIDE(format_int, 0x32b06, 0, 0, 0x252ce, 0x29276);

// $32b78 (RedStart $25340, Red $292e8): ltoa: v (not negative) in
// decimal.
void format_long(int32_t v, char* out) {
  int at = 0;
  if (v == 0) {
    out[at++] = '0';
  } else {
    int i = kLongDigits;
    while (v < long_powers[i]) --i;
    for (; i; --i) {
      const int32_t p = long_powers[i];
      out[at++] = char('0' + v / p);
      v %= p;
    }
  }
  out[at] = 0;
}
ORIGINAL_INSIDE(format_long, 0x32b78, 0, 0, 0x25340, 0x292e8);

}  // namespace game::common
