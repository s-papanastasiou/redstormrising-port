// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Screen: the display and drawing library linked into every module
// (screen, window, three bitmaps to draw into and show, pens, lines,
// rectangles, blits, the mouse pointer, the palette, masked shapes and
// pictures). Records and variables. Offsets are a4-relative in Storm;
// the other modules add the library block's shift (LibVar).
#pragma once

#include "game/mem/constants.hpp"
#include "game/mem/globals.hpp"
#include "game/screen/bitmap.hpp"

namespace game::screen {

using namespace game::mem;



// A text style: the bitmap drawn into, an area (origin and size), draw
// mode, pens and font. Red's and RedStart's styles also have a text
// cursor and a left margin.
struct TextStyle {
  be16 bitmap;     // +0: bitmap number (draw_to)
  sbe16 x;         // +2: origin, added to the x of draw_text
  sbe16 y;         // +4: added to the y
  sbe16 width;     // +6
  sbe16 height;    // +8
  be16 mode;       // +10: 0 JAM1, 1 JAM2
  be16 fg;         // +12: foreground pen
  be16 bg;         // +14: background pen
  be16 font;       // +16: number in `fonts`
  sbe16 cursor_x;  // +18: relative to the origin
  sbe16 cursor_y;  // +20
  sbe16 margin;    // +22: where new lines start
};

// Library variables (data block).
inline constexpr ConstLib<char> alert_tail{-10610, Block::Bss};  // " -- Click the mouse." (after an alert's text)
// The disks: the volumes (each twice: locked, and named by the requester)
// and the requester's lines: "Please Insert Disk:", the quotes around the
// volume's name, "Into Any Drive.", "Press a Key to Cancel.". Its palette
// is Storm's battle_palette (RedStart's main_palette).
inline constexpr ConstLib<char> volume_a{-10474, Block::Bss}, volume_a_name{-10462, Block::Bss};  // "RedStormA:"
inline constexpr ConstLib<char> volume_b{-10450, Block::Bss}, volume_b_name{-10438, Block::Bss};  // "RedStormB:"
inline constexpr ConstLib<char> insert_disk_text{-10426, Block::Bss}, open_quote{-10406, Block::Bss},
    close_quote{-10404, Block::Bss}, any_drive_text{-10402, Block::Bss}, cancel_text{-10386, Block::Bss};
inline constexpr ConstLib<be16> requester_palette{-11576, Block::Bss};  // [kPaletteWords]
inline constexpr int16_t kRequesterFont = 2;
inline constexpr int16_t kRequesterFrame = 2, kRequesterText = 7;  // pens
inline constexpr int kPaletteWords = 32;
inline constexpr LibVar<BitMapSlot> shape_bitmap{9660, Block::Data};  // the loaded shape (6 planes; its plane 7 is the mask: shape_mask())

// Library variables (BSS block).

inline constexpr ConstLib<char> shape_file{-10542, Block::Bss};       // file names
inline constexpr ConstLib<char> picture_file{-10652, Block::Bss};
inline constexpr ConstLib<char> bad_shape_text{-10572, Block::Bss};   // 24 characters, then the number
inline constexpr ConstLib<char> no_shape_file_text{-10526, Block::Bss};
inline constexpr ConstLib<char> no_picture_file_text{-10636, Block::Bss};

// The opening pictures (RedStorm, Title).
inline constexpr ConstPerModule<char> opening_file{{0, 2130, -22242, 0, 0}};       // "RedStormA:OPENING": an offset table, then pictures
inline constexpr ConstPerModule<char> no_opening_text{{0, 2148, -22224, 0, 0}};   // "ReadOpenPic - Can't open OPENING"
inline constexpr int32_t kOpeningTableBytes = 400;

// dos.library (its variable is outside the library block).

// Storm and Title: the picture behind each screen number (kNoPicture: none).
inline constexpr ConstPerModule<be16> screen_pictures{{-21034, 0, -27506, 0, 0}};

inline constexpr int kBitmaps = 3, kColours = 32;
inline constexpr int16_t kLastShape = 58, kNoPicture = 12, kPictureBitmap = 2;
inline constexpr int kShapePlanes = 6, kShapePlaneBytes = 4500;
inline constexpr int16_t kScreenRight = 319, kScreenBottom = 199;

// The pointer images (module data): 8x8, 7x7 with hot spot (-3, -3), and
// the invisible one.

// RedStart and Red: the palette of the module's screens (32 $0RGB words).

// Storm: the list of images for draw_image (each: width in bytes, height,
// six bitplanes and the mask, all width * height bytes).

// Animated objects (Storm and Title): drawn with draw_shape into a bitmap,
// the areas they cover queued as regions to copy back before the next
// frame (double buffering), moved and changed by a script.

// An area to copy back.
struct Region {
  int16_t x, y, w, h;
};
// The regions queued for one bitmap (2 lists).
struct RegionList {
  uint16_t count;
  Region entries[50];
};
static_assert(sizeof(RegionList) == 402);

// An animated object (80 bytes).
struct AnimObject {
  int16_t x, y;              // +0: position
  int16_t w, h;              // +4: the area restored instead of a shape (frame -1)
  int16_t vx, vy;            // +8: added to the position every frame
  int16_t frame;             // +12: the shape shown; -1: restore the area
  int16_t frames;            // +14: number of shapes, cycled when more than 1; 0: not drawn
  BitMapRef shapes[16];  // +16
};
static_assert(sizeof(AnimObject) == 80);

// A command of the animation script (42 bytes), run when animate reaches
// its frame. Letters (either case): C set the C parameters, P place, M
// set the velocity, S draw, X sound effect, F a fixed area, I shapes.
struct AnimCommand {
  be16 frame;              // +0
  int8_t object;           // +2
  int8_t letter;           // +3
  sbe16 count;             // +4: I: number of shapes
  sbe16 args[16];          // +6
  Ptr<AnimCommand> next;   // +38
};
static_assert(sizeof(AnimCommand) == 42);

inline constexpr PerModule<BitMapSlot> anim_shapes{{6880, 0, 1804, 0, 0}};   // the shapes I refers to
inline constexpr ConstPerModule<sbe16> anim_x_offsets{{-30168, 0, -30248, 0, 0}};  // P: by anim_variant
inline constexpr ConstPerModule<sbe16> anim_y_offsets{{-30036, 0, -30116, 0, 0}};

// The picture decoder's state (decoder.cpp), kept in C++ with a copy per
// module (each module had its record of 0x1cd0 bytes, kDecoderAt). No host
// pointers in it: the state is hashed by the determinism check.
struct Decoder {
  uint8_t input[256];       // the file's next 256 bytes (decoding from a file)
  uint8_t run_stack[256];   // a code's string, last byte first (popped from the top)
  int16_t prefix[2048];     // [code] the code it extends (-1: a single byte)
  uint8_t suffix[2048];     // [code] its last byte
  uint8_t row[320];         // a row, a byte per pixel (bit k: plane k)
  uint32_t planes[6];       // where the next row goes, per plane (emulated addresses)
  int16_t modulo;           // the bitmap's bytes per row beyond the picture's
  uint32_t file;            // the file read (0: decoding from memory)
  uint32_t in, in_end;      // the input: emulated addresses, or indexes into input (from a file)
  int16_t width;            // pixels a row (when decoding: bytes a row - 1)
  int16_t rows;             // rows left
  int16_t words;            // words a plane row (when decoding: - 1)
  uint16_t palette[32];     // $0RGB
  int16_t depth;            // planes
  int16_t colours;          // palette entries
  uint8_t nibbles;          // $ff: two pixels a byte ($90 the run marker)
  uint8_t run_count, run_value;  // the run being expanded
  uint8_t code_bits, max_bits;
  uint16_t code_mask;       // (1 << code_bits) - 1
  uint16_t next_code;       // the next table entry
  uint16_t bit_word;        // the input word being read
  uint8_t bits_left;        // its bits not read yet (the high ones)
  uint8_t first_byte;       // of the last string
  uint16_t old_code, code;
  uint16_t run_top;         // bytes on run_stack
  int16_t status;           // 1 ok, 0 an error
};
// The original record's a4 offsets (storm, redstorm, title, redstart, red)
// and size: moved.
inline constexpr int32_t kDecoderAt[5] = {-20784, 2192, -22188, 6192, -7476};
inline constexpr int32_t kDecoderBytes = 0x1cd0;
inline constexpr uint16_t kFlagNibbles = 0x1, kFlag4Planes16 = 0x100, kFlag5Planes32 = 0x200, kFlag6Planes32 = 0x400,
                          kFlag6Planes16 = 0x800, kFlagNoPalette = 0x1000;
inline constexpr uint8_t kRunMarker = 0x80, kNibbleRunMarker = 0x90, kMaxCodeBits = 11, kFirstCodeBits = 9;
inline constexpr uint16_t kFirstCode = 256, kFirstMask = 511;
inline constexpr int kTableSize = 2048, kInputBytes = 256, kOverrunBack = 20;

// Animation sequences (Storm's event pictures; Title's title sequence,
// number 12, has its own files): a script and the shapes, each from its
// file or from a copy preloaded into memory.
inline constexpr ConstPerModule<sbe16> anim_shape_parts{{-21060, 0, -27532, 0, 0}};   // [sequence]: its part of the file
inline constexpr ConstPerModule<char> script_file{{-20894, 0, -27366, 0, 0}}, title_script_file{{-20914, 0, -27386, 0, 0}};
inline constexpr ConstPerModule<char> anim_shape_file{{-20990, 0, -27462, 0, 0}}, title_shape_file{{-21008, 0, -27480, 0, 0}};
inline constexpr ConstPerModule<char> no_plane_text{{-20974, 0, -27446, 0, 0}}, no_mask_text{{-20946, 0, -27418, 0, 0}},
    no_script_memory_text{{-20878, 0, -27350, 0, 0}};
inline constexpr ConstData<sbe16> variant_shapes{{0x34fe0, 0, 0x26230, 0, 0}};  // [variant]: anim_shapes[0]'s shape
inline constexpr int16_t kTitleSequence = 12, kNoVariant = -1, kAnimPlanes = 6;
inline constexpr int32_t kTitleShapesLength = 35832;  // the whole title shape file

// draw_shape's first/last word masks for a shape clipped on the left /
// right, by shift (16 words each).
inline constexpr ConstPerModule<be16> first_word_masks{{-20848, 0, -27320, -20780, 6380}};
inline constexpr ConstPerModule<be16> last_word_masks{{-20816, 0, -27288, -20748, 6412}};

inline constexpr int kMaskPlane = 7;      // draw_shape: a shape's mask is its plane 7
// The loaded shape's mask plane (the original's shape_mask, a4+9696 in
// Storm, is that field of shape_bitmap).
inline uint32_t& shape_mask() { return bitmap(shape_bitmap.addr()).planes[kMaskPlane]; }
inline constexpr int kRestoreBitmap = 2;  // the clean background of animated objects

// Graphics minterms.
inline constexpr uint32_t kCopy = 0xc0, kCookieCut = 0xe0;
// Blitter minterms (D = B; D = A ? B : C).
inline constexpr uint16_t kMintermCopyB = 0xcc, kMintermCookieCut = 0xca;

}  // namespace game::screen
