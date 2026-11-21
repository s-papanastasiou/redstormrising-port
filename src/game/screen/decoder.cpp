// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Screen: the picture decoder, hand-written assembly linked into every
// module. A picture is a header (format flags, width, rows, palette, the
// maximum code size) and LZW codes, read least significant bit first from
// 16-bit words; their bytes are run-length coded pixels, a byte each (or
// two pixels a byte), turned row by row into the bitplanes of a BitMap.
// The state is a Decoder per module in C++ (it was a record in each
// module's data).
#include "game/mem/os.hpp"
#include "game/mem/state.hpp"
#include "game/mem/register.hpp"
#include "game/screen/calls.hpp"
#include "game/screen/vars.hpp"
#include "game/ui/state.hpp"
#include "runtime/runtime.hpp"

namespace game::screen {

namespace {

Decoder g_decoders[5];
Decoder& the_decoder() { return g_decoders[runtime::module_index()]; }

// The moved records (their a4 ranges; the state is hashed, not loaded:
// read_header sets it up before a picture).
bool register_decoders() {
  static mem::state::Range moved[5][1];
  constexpr const char* kNames[5] = {"storm", "redstorm", "title", "redstart", "red"};
  for (int m = 0; m < 5; ++m) {
    moved[m][0] = {kDecoderAt[m], kDecoderBytes};
    mem::state::add({m, kNames[m], nullptr, nullptr, &g_decoders[m], uint32_t(sizeof(Decoder)), moved[m], 1, nullptr, 0});
  }
  return true;
}
[[maybe_unused]] const bool g_registered = register_decoders();

// The input word at `in` (big-endian): in the file buffer, or in memory.
uint16_t word_at(const Decoder& d, uint32_t in) {
  if (d.file) return uint16_t(d.input[in] << 8 | d.input[in + 1]);
  return ptr_to<be16>(in)[0];
}

// $2f580 (all modules: decoder_code - $92): at the end of the input. From
// a file: its next 256 bytes; from memory: an error, and the input goes
// back 20 bytes (the original goes on decoding them). Returns the input.
uint32_t refill(Decoder& d, uint32_t in) {
  if (d.file) {
    os::read(d.file, d.input, kInputBytes);
    return 0;
  }
  d.status = 0;
  return in - kOverrunBack;
}

// $2f5be: the next input word (the header's).
uint16_t next_word(Decoder& d) {
  uint32_t in = d.in;
  if (in >= d.in_end) in = refill(d, in);
  const uint16_t w = word_at(d, in);
  d.in = in + 2;
  return w;
}

// $2f5da: the tables hold the single bytes only. Returns kFirstCodeBits;
// the next code is kFirstCode.
void reset_tables(Decoder& d) {
  for (int c = kTableSize - 1; c >= 0; --c) {
    d.prefix[c] = -1;
    d.suffix[c] = c < 256 ? uint8_t(c) : 0;
  }
  d.code_mask = kFirstMask;
}

// $2f990: the header: the flags (the plane count and palette size; two
// pixels a byte), width, rows, palette, the maximum code size. Returns 0
// for an unknown format.
int16_t read_header(Decoder& d) {
  for (uint8_t& b : d.row) b = 0;
  d.colours = 0;
  const uint16_t flags = next_word(d);
  d.nibbles = (flags & kFlagNibbles) ? 0xff : 0;
  if (flags & kFlag4Planes16) {
    d.depth = 4;
    d.colours = 16;
  } else if (flags & kFlag5Planes32) {
    d.depth = 5;
    d.colours = 32;
  } else if (flags & kFlag6Planes32) {
    d.depth = 6;
    d.colours = 32;
  } else if (flags & kFlag6Planes16) {
    d.depth = 6;
    d.colours = 16;
  } else {
    d.status = 0;
    return 0;
  }
  if (flags & kFlagNoPalette) d.colours = 0;
  d.width = int16_t(next_word(d));
  d.words = int16_t(uint16_t(d.width + 15) >> 4);
  d.rows = int16_t(next_word(d));
  for (int k = 0; k < d.colours; ++k) d.palette[k] = next_word(d);
  d.run_top = 0;
  d.run_count = 0;
  d.run_value = 0;
  d.bit_word = next_word(d);
  d.bits_left = 8;
  const uint8_t max_bits = uint8_t(d.bit_word);  // the first word's low byte
  d.max_bits = int8_t(max_bits) > int8_t(kMaxCodeBits) ? kMaxCodeBits : max_bits;
  reset_tables(d);
  d.code_bits = kFirstCodeBits;
  d.next_code = kFirstCode;
  d.status = 1;
  return 1;
}

// The row decoders' working state (the original's registers), loaded
// from the record at the start of a row and stored at its end.
class RowState {
public:
  explicit RowState(Decoder& d)
      : d_(d), in_(d.in), top_(d.run_top), next_code_(d.next_code), bits_left_(d.bits_left),
        code_bits_(d.code_bits), run_(d.run_count), value_(d.run_value) {}

  void store() {
    d_.run_top = top_;
    d_.in = in_;
    d_.run_count = run_;
    d_.run_value = value_;
    d_.code_bits = code_bits_;
    d_.bits_left = bits_left_;
    d_.next_code = next_code_;
  }

  // The next byte: the rest of the last string, or a new code's.
  uint8_t next_byte() {
    if (top_ != 0) return d_.run_stack[--top_];
    return lzw_code();
  }

  uint8_t& run() { return run_; }
  uint8_t& value() { return value_; }

private:
  void push(uint8_t b) { d_.run_stack[top_++] = b; }

  // $2f612: the next code: its string goes onto the stack (but its first
  // byte, returned), the table gets the last string plus that byte; the
  // codes grow a bit when the table needs it, past the maximum the tables
  // start again.
  uint8_t lzw_code() {
    uint16_t bits = uint16_t(uint16_t(d_.bit_word) >> (16 - bits_left_));
    if (int8_t(bits_left_) < int8_t(code_bits_)) {
      if (in_ >= d_.in_end) in_ = refill(d_, in_);
      const uint16_t w = word_at(d_, in_);
      in_ += 2;
      d_.bit_word = w;
      bits = uint16_t(bits | uint16_t(w << bits_left_));
      bits_left_ = uint8_t(bits_left_ + 16);
    }
    bits_left_ = uint8_t(bits_left_ - code_bits_);
    bits &= d_.code_mask;
    d_.code = bits;
    uint16_t c = bits;
    if (int16_t(c) >= int16_t(next_code_)) {  // the code being defined: the last string and its first byte
      d_.code = next_code_;
      c = d_.old_code;
      push(d_.first_byte);
    }
    while (d_.prefix[int16_t(c)] >= 0) {
      push(d_.suffix[int16_t(c)]);
      c = uint16_t(int16_t(d_.prefix[int16_t(c)]));
    }
    const uint8_t first = d_.suffix[int16_t(c)];
    d_.first_byte = first;
    d_.suffix[int16_t(next_code_)] = first;
    d_.prefix[int16_t(next_code_)] = int16_t(uint16_t(d_.old_code));
    ++next_code_;
    if (int16_t(next_code_) > int16_t(uint16_t(d_.code_mask))) {
      ++code_bits_;
      d_.code_mask = uint16_t(d_.code_mask * 2 + 1);
    }
    if (int8_t(code_bits_) > int8_t(d_.max_bits)) {
      reset_tables(d_);
      code_bits_ = kFirstCodeBits;
      next_code_ = kFirstCode;
    }
    d_.old_code = d_.code;
    return first;
  }

  Decoder& d_;
  uint32_t in_;
  uint16_t top_;
  uint16_t next_code_;
  uint8_t bits_left_, code_bits_, run_, value_;
};

// $2f6c8: a row of a pixel a byte. A byte with bit 7 set starts a run:
// the next byte n repeats the last pixel n - 1 times in all.
void decode_bytes(Decoder& d) {
  RowState s(d);
  uint8_t* out = d.row;
  for (uint16_t k = uint16_t(d.width);; --k) {
    if (s.run()) {
      --s.run();
    } else {
      const uint8_t b = s.next_byte();
      if (!(b & kRunMarker)) s.value() = b;
      else s.run() = uint8_t(s.next_byte() - 2);
    }
    *out++ = s.value();
    if (k == 0) break;
  }
  s.store();
}

// $2f77c: a row of two pixels a byte (high nibble first). $90 starts a
// run (the next byte n repeats the last byte n - 1 times in all); $90 0
// is the byte $90.
void decode_nibbles(Decoder& d) {
  RowState s(d);
  uint8_t* out = d.row;
  for (uint16_t k = uint16_t(d.width);; --k) {
    if (s.run()) {
      --s.run();
    } else {
      const uint8_t b = s.next_byte();
      if (b != kNibbleRunMarker) {
        s.value() = b;
      } else {
        const uint8_t n = s.next_byte();
        if (n == 0) s.value() = kNibbleRunMarker;
        else s.run() = uint8_t(n - 2);
      }
    }
    *out++ = uint8_t(s.value() >> 4);
    *out++ = s.value();
    if (k == 0) break;
  }
  s.store();
}

// $2f84a / $2f8a8 / $2f912: the row into `depth` planes (bit k of a pixel
// into plane k, 16 pixels a word); the plane pointers move on by the
// modulo.
void write_planes(Decoder& d, int depth) {
  const uint8_t* px = d.row;
  for (uint16_t w = uint16_t(d.words);; --w) {
    uint16_t bits[6] = {};
    for (int k = 0; k < 16; ++k, ++px)
      for (int p = 0; p < depth; ++p) bits[p] = uint16_t((bits[p] << 1) | ((*px >> p) & 1));
    for (int p = 0; p < depth; ++p) {
      uint8_t* to = host_bytes(d.planes[p], 2);  // the bitmap's plane (chip memory)
      to[0] = uint8_t(bits[p] >> 8);
      to[1] = uint8_t(bits[p]);
      d.planes[p] = d.planes[p] + 2;
    }
    if (w == 0) break;
  }
  for (int p = 0; p < depth; ++p) d.planes[p] = d.planes[p] + uint32_t(int32_t(d.modulo));
}

// $2faba: the picture into the BitMap at `bitmap`, if it has the planes,
// rows and width for it. Returns the status (0: an error).
int16_t decode(uint32_t bitmap) {
  Decoder& d = the_decoder();
  if (!read_header(d)) return 0;
  const BitMap& bm = screen::bitmap(bitmap);
  auto fail = [&] {
    d.status = 0;
    return int16_t(0);
  };
  const int16_t depth = int8_t(bm.depth);
  if (depth < d.depth) return fail();
  // The bitmap's planes, all of them (the original's planes table has six
  // entries; a deeper bitmap's further planes ran into the next field).
  for (int p = 0; p < depth && p < 6; ++p) d.planes[p] = bm.planes[p];
  if (int16_t(bm.rows) < d.rows) return fail();
  const int16_t modulo = int16_t(bm.bytes_per_row - d.words * 2);
  if (modulo < 0) return fail();
  d.modulo = modulo;
  d.words = int16_t(d.words - 1);
  if (d.nibbles) d.width = int16_t((uint16_t(d.width + 1) >> 1) - 1);
  else d.width = int16_t(d.width - 1);
  const int planes_used = d.depth == 4 ? 4 : d.depth == 5 ? 5 : 6;
  do {
    if (d.nibbles) decode_nibbles(d);
    else decode_bytes(d);
    write_planes(d, planes_used);
    d.rows = int16_t(d.rows - 1);
  } while (d.rows != 0);
  return d.status;
}

}  // namespace

// The decoder's inner routines (register arguments; entered only from
// unpack and unpack_file), done by the functions above.
ORIGINAL_INSIDE(refill, 0x2f580, 0x22550, 0x24000, 0x27410, 0x25614);
ORIGINAL_INSIDE(next_word, 0x2f5be, 0x2258e, 0x2403e, 0x2744e, 0x25652);
ORIGINAL_INSIDE(reset_tables, 0x2f5da, 0x225aa, 0x2405a, 0x2746a, 0x2566e);
ORIGINAL_INSIDE(lzw_code, 0x2f612, 0x225e2, 0x24092, 0x274a2, 0x256a6);
ORIGINAL_INSIDE(decode_bytes, 0x2f6c8, 0x22698, 0x24148, 0x27558, 0x2575c);
ORIGINAL_INSIDE(decode_nibbles, 0x2f77c, 0x2274c, 0x241fc, 0x2760c, 0x25810);
ORIGINAL_INSIDE(write_planes4, 0x2f84a, 0x2281a, 0x242ca, 0x276da, 0x258de);
ORIGINAL_INSIDE(write_planes5, 0x2f8a8, 0x22878, 0x24328, 0x27738, 0x2593c);
ORIGINAL_INSIDE(write_planes6, 0x2f912, 0x228e2, 0x24392, 0x277a2, 0x259a6);
ORIGINAL_INSIDE(read_header, 0x2f990, 0x22960, 0x24410, 0x27820, 0x25a24);
ORIGINAL_INSIDE(decode, 0x2faba, 0x22a8a, 0x2453a, 0x2794a, 0x25b4e);

// $2fc16: the picture at `data` (`length` bytes) into the BitMap at
// `bitmap`. Returns 1, or 0 if it does not fit or is broken.
int16_t unpack(uint32_t bitmap, uint32_t data, int32_t length) {
  Decoder& d = the_decoder();
  d.file = 0;
  d.in = data;
  d.in_end = data + uint32_t(length);
  return decode(bitmap);
}
ORIGINAL(unpack, 0x2fc16, 0x22be6, 0x24696, 0x27aa6, 0x25caa);

// ($2fbac, unpack_file, the same from a file: not reached by the game.)

// $2fc48: the palette of the picture decoded last into `colours`.
// Returns its size.
int16_t picture_palette(be16* colours) {
  const Decoder& d = the_decoder();
  for (int i = 0; i < (uint16_t(d.colours) >> 1) * 2; ++i) colours[i] = d.palette[i];
  return d.colours;
}
ORIGINAL_INSIDE(picture_palette, 0x2fc48, 0x22c18, 0x246c8, 0x27ad8, 0x25cdc);

}  // namespace game::screen
