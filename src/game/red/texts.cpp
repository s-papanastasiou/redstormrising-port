// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Red and RedStart: the game's texts, packed five bits a character, in
// lists.
#include "game/mem/register.hpp"
#include "game/red/calls.hpp"
#include "game/red/vars.hpp"
#include "game/ui/state.hpp"

namespace game::red {

namespace {
// Five-bit codes of a packed text (others: a character, base + code).
enum Code : uint8_t { kEnd = 0, kSpace = 27, kLineFeed = 28, kSwapBase = 29, kSwapCase = 30 };
// The bases: capitals (and lower case after kSwapCase), or the
// characters from '!' (digits, punctuation).
inline constexpr uint8_t kCapitals = 64, kSwapBases = 64 ^ 32, kLowerCase = 32;
}  // namespace

// $28d3a (RedStart $255ba): packed_text decoded into unpacked_text, with
// a carriage return at the end if `newline`. The bits are read from the
// most significant one; past the end they are 0 (the end code). A '^'
// comes out as '?'.
void unpack_text(int16_t newline) {
  const int length = ui::state().packed_text[0];
  int at = 0;           // the byte being read
  int bit = 7;          // its bits taken
  uint16_t bits = 0;    // its bits left, shifted up
  uint8_t base = kCapitals, lower = 0;
  int out = 0;
  for (;;) {
    uint8_t code = 0;
    for (int k = 0; k < 5; ++k) {
      if (++bit > 7) {
        bit = 0;
        bits = at + 1 < length ? ui::state().packed_text[++at] : 0;
      }
      bits = uint16_t(bits << 1);
      code = uint8_t(code << 1);
      if (bits > 255) {
        bits &= 255;
        ++code;
      }
    }
    if (code == kEnd) break;
    switch (code) {
      case kSpace: ui::state().unpacked_text[out++] = ' '; break;
      case kLineFeed: ui::state().unpacked_text[out++] = '\n'; break;
      case kSwapBase: base ^= kSwapBases; break;
      case kSwapCase: lower ^= kLowerCase; break;
      default: {
        const uint8_t ch = uint8_t(code + base + (base == kCapitals ? lower : 0));
        ui::state().unpacked_text[out++] = char(ch == '^' ? '?' : ch);
      }
    }
  }
  if (newline) ui::state().unpacked_text[out++] = '\r';
  ui::state().unpacked_text[out] = 0;
}
ORIGINAL(unpack_text, 0, 0, 0, 0x255ba, 0x28d3a);

// $28c94 (RedStart $25514): the packed text at `packed` unpacked (with a
// carriage return). Returns unpacked_text.
const char* copy_unpack(uint32_t packed) {
  const uint8_t* from = constant_array<uint8_t>(packed, constant<uint8_t>(packed));
  const int length = from[0];
  for (int i = 0; i < length; ++i) ui::state().packed_text[i] = from[i];
  ui::state().packed_text[length] = 0;
  unpack_text(1);
  return ui::state().unpacked_text;
}
ORIGINAL_INSIDE(copy_unpack, 0, 0, 0, 0x25514, 0x28c94);

namespace {
const char* message_of(int16_t n, uint32_t offsets, uint32_t texts, const char* none) {
  if (n == 0) return none;
  return copy_unpack(texts + uint32_t(int32_t(constant<sbe16>(offsets + uint32_t(2 * n)))));
}
}  // namespace

// $28d04 (RedStart $25584): message n (0: an empty text), unpacked.
const char* message(int16_t n) { return message_of(n, message_offsets.addr(), message_texts.addr(), no_message.str()); }
ORIGINAL_INSIDE(message, 0, 0, 0, 0x25584, 0x28d04);

// $28cd0: message n of Red's second list (0: an empty text), unpacked.
const char* message2(int16_t n) { return message_of(n, message2_offsets.addr(), message2_texts.addr(), no_message2.str()); }
ORIGINAL_INSIDE(message2, 0, 0, 0, 0, 0x28cd0);

// $2888a (RedStart $24e3e): text n (from 1; 0 is the first too) of
// text_list unpacked into unpacked_text. The original calls the fatal
// error for a length above 256, which a byte cannot hold.
void text_n(int16_t n) {
  const ConstPtr<uint8_t> list = ui::state().text_list;
  int at = 0;
  for (int16_t k = n; --k > 0;) at += list[at];
  const int length = list[at];
  for (int i = 0; i < length; ++i) ui::state().packed_text[i] = list[at + i];
  ui::state().packed_text[length] = 0;
  unpack_text(0);
}
ORIGINAL(text_n, 0, 0, 0, 0x24e3e, 0x2888a);

// $28872 (RedStart $24e26): text n unpacked. Returns unpacked_text.
const char* text_n_ptr(int16_t n) {
  text_n(n);
  return ui::state().unpacked_text;
}
ORIGINAL_INSIDE(text_n_ptr, 0, 0, 0, 0x24e26, 0x28872);

}  // namespace game::red
