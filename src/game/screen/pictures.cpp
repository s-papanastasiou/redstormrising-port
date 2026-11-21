// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Screen: loading shapes (masked images, drawn by draw_masked) and
// pictures (full-screen images with their palette) from their files or
// from the copy preloaded into memory.
#include "game/common/strings.hpp"
#include "game/mem/os.hpp"
#include "game/mem/register.hpp"
#include "game/screen/calls.hpp"
#include "game/screen/vars.hpp"
#include "game/ui/state.hpp"

#include <cstring>

namespace game::screen {

namespace {
// Where item n of a file is (from its offset table) and how long it is.
struct Extent {
  int32_t offset, length;
};
Extent extent(const Ptr<sbe32>& offsets, int16_t n) { return {offsets[n], offsets[n + 1] - offsets[n]}; }

uint32_t read_from_disk(const char* name, const char* missing, Extent e) {
  return read_file_part(name, missing, e.offset, e.length);
}
}  // namespace

// Read `length` bytes at `offset` of the file `name` into the file buffer
// (after asking for the game disk); `missing` is the fatal error when it
// cannot be opened. Returns the buffer.
uint32_t read_file_part(const char* name, const char* missing, int32_t offset, int32_t length) {
  need_disk();
  const uint32_t file = os::open(name, os::MODE_OLDFILE);
  if (!file) fatal_error(missing);
  os::seek(file, offset, os::OFFSET_BEGINNING);
  os::read(file, ui::state().file_buffer, length);
  os::close(file);
  return ui::state().file_buffer;
}

// Unpack a shape (packed data with its size at +2/+4) into shape_bitmap
// and build its mask (a pixel is in the mask if it is not colour 0).
void build_shape(uint32_t data, int32_t length) {
  const Ptr<be16> header = ptr_to<be16>(data);
  const uint32_t handle = shape_bitmap.addr();
  os::init_bitmap(handle, kShapePlanes, int16_t(header[1]), int16_t(header[2]));
  BitMap& bm = bitmap(handle);
  for (int16_t p = 0; p < kShapePlanes; ++p) bm.planes[p] = ui::state().shape_memory + uint32_t(p * kShapePlaneBytes);
  shape_mask() = ui::state().shape_memory + uint32_t(kShapePlanes * kShapePlaneBytes);
  unpack(handle, data, length);

  const uint32_t size = uint32_t(bm.rows) * bm.bytes_per_row;
  if (!size) return;
  uint8_t* mask = host_bytes(shape_mask(), size);
  const uint8_t* plane0 = host_bytes(bm.planes[0], size);
  for (uint32_t i = 0; i < size; ++i) mask[i] = plane0[i];
  for (int p = 1; p < kShapePlanes; ++p) {
    const uint8_t* plane = host_bytes(bm.planes[p], size);
    for (uint32_t i = 0; i < size; ++i) mask[i] |= plane[i];
  }
}

// $31b68: load shape n (0..kLastShape) into shape_bitmap and build its
// mask (a pixel is in the mask if it is not colour 0).
void load_shape(int16_t n) {
  if (n > kLastShape || n < 0) {
    char text[66];
    std::strcpy(text, bad_shape_text.str());
    common::format_int(n, text + 24);  // the original: sprintf "%d"
    fatal_error(text);
  }
  const Extent e = extent(ui::state().shape_offsets, n);
  const uint32_t data = ui::state().shapes_preloaded ? ui::state().preloaded_shapes + uint32_t(e.offset)
                                          : read_from_disk(shape_file.str(), no_shape_file_text.str(), e);
  build_shape(data, e.length);
  ui::state().current_shape = n;
}
ORIGINAL(load_shape, 0x31b68, 0x21c78, 0x239b0, 0x23ca8, 0x27c10);

// $318f2: load picture n into bitmap number `bitmap`; its palette is
// loaded at the next show_bitmap.
void load_picture(int16_t n, int16_t bitmap) {
  const Extent e = extent(ui::state().picture_offsets, n);
  const uint32_t data = ui::state().pictures_preloaded
                            ? ui::state().preloaded_pictures + uint32_t(e.offset)
                            : read_from_disk(picture_file.str(), no_picture_file_text.str(), e);
  unpack(ui::state().bitmaps[bitmap].addr, data, e.length);
  be16 colours[64] = {};  // the size of the original's buffer
  picture_palette(colours);
  set_palette(colours);
}
ORIGINAL(load_picture, 0x318f2, 0x21a02, 0x2373a, 0x23a32, 0x2799a);

// $2e4f4 (Storm, Title): screen n: bitmap 2 cleared and drawn into, with
// the screen's picture (if it has one). Returns 1.
int16_t show_screen(int16_t n) {
  draw_to(kPictureBitmap);
  clear_screen();
  const int16_t picture = int16_t(screen_pictures[n]);
  if (picture != kNoPicture) load_picture(picture, kPictureBitmap);
  return 1;
}
ORIGINAL(show_screen, 0x2e4f4, 0, 0x20afc, 0, 0);

// $221e8 (Title $23f20): opening picture n from RedStormA:OPENING (always from disk)
// unpacked into bitmap number `bitmap`, its palette set.
void opening_picture(int16_t n, int16_t bitmap) {
  need_disk_a();
  const uint32_t file = os::open(opening_file.str(), os::MODE_OLDFILE);
  if (!file) fatal_error(no_opening_text.str());
  const uint32_t buffer = ui::state().file_buffer;
  os::read(file, buffer, kOpeningTableBytes);
  const Ptr<sbe32> offsets = ptr_to<sbe32>(buffer);
  const int32_t offset = offsets[n], length = offsets[n + 1] - offset;
  os::seek(file, offset + kOpeningTableBytes, os::OFFSET_BEGINNING);
  os::read(file, buffer, length);
  os::close(file);
  unpack(ui::state().bitmaps[bitmap].addr, buffer, length);
  be16 colours[64] = {};  // the size of the original's buffer
  picture_palette(colours);
  set_palette(colours);
}
ORIGINAL(opening_picture, 0, 0x221e8, 0x23f20, 0, 0);

}  // namespace game::screen
