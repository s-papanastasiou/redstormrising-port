// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Screen: animation sequences (Storm's event pictures, Title's title
// sequence): the script and the shapes loaded from their files (or the
// copies preloaded into memory), then played frame by frame until the end
// or a key.
#include "game/audio/calls.hpp"
#include "game/common/memory.hpp"
#include "game/input/calls.hpp"
#include "game/input/vars.hpp"
#include "game/mem/hardware.hpp"
#include "game/mem/os.hpp"
#include "game/mem/register.hpp"
#include "game/screen/calls.hpp"
#include "game/screen/vars.hpp"
#include "game/ui/state.hpp"
#include "game/screen/anim_state.hpp"

namespace game::screen {

namespace {
// A sequence's script file: per sequence a word (its number of commands),
// then the commands.
inline constexpr int32_t kCommandBytes = sizeof(AnimCommand);

int32_t plane_bytes(uint8_t width, uint8_t height) { return int32_t(width) * height; }
}  // namespace

// $2e1d0 (Title $207d8): the planes of the loaded shapes freed.
void free_anim_shapes() {
  for (int16_t s = 1; s < anim().anim_shape_count; ++s) {
    BitMap& bm = bitmap(anim_shapes.addr() + uint32_t(s) * kBitMapBytes);
    const int32_t width = bm.bytes_per_row * 8, height = bm.rows;
    for (int p = 0; p < kAnimPlanes; ++p) {
      if (!bm.planes[p]) continue;
      os::free_raster(bm.planes[p], width, height);
      bm.planes[p] = 0;
    }
    if (bm.planes[kMaskPlane]) {
      os::free_raster(bm.planes[kMaskPlane], width, height);
      bm.planes[kMaskPlane] = 0;
    }
  }
}
ORIGINAL(free_anim_shapes, 0x2e1d0, 0, 0x207d8, 0, 0);

// $2e3fa (Title $20a02): the script read from disk freed.
void free_anim_script() {
  if (ui::state().scripts_preloaded) return;
  if (anim().anim_buffer) common::c_free(anim().anim_buffer);
  anim().anim_buffer = 0;
}
ORIGINAL(free_anim_script, 0x2e3fa, 0, 0x20a02, 0, 0);

// $2ebb8 (Title $211c0): the alert `text` (if any), then the sequence's
// shapes and script freed.
void anim_error(const char* text) {
  if (text) alert(text);
  free_anim_shapes();
  free_anim_script();
}
ORIGINAL_INSIDE(anim_error, 0x2ebb8, 0, 0x211c0, 0, 0);

// $2e2a0 (Title $208a8): the script of sequence n into anim_commands (from
// disk: allocated, the commands linked in order). Returns 0 on an error.
int16_t load_anim_script(int16_t n) {
  const char* name;
  if (n == kTitleSequence) {
    name = title_script_file.str();
    n = 0;
  } else {
    name = script_file.str();
    if (!ui::state().scripts_preloaded) need_disk();
  }
  if (ui::state().scripts_preloaded) {
    uint32_t at = ui::state().preloaded_scripts;
    int16_t count = ptr_to<sbe16>(at)[0];
    at += 2;
    for (int16_t k = 0; k < n; ++k) {
      at += uint32_t(int32_t(count) * kCommandBytes);
      count = ptr_to<sbe16>(at)[0];
      at += 2;
    }
    anim().anim_commands = at;
    return 1;
  }
  const uint32_t file = os::open(name, os::MODE_OLDFILE);
  if (!file) {
    anim_error(name);
    return 0;
  }
  be16 count_word;
  be16* count = &count_word;
  os::read(file, count_word.b, 2);
  for (int16_t k = 0; k < n; ++k) {
    os::seek(file, int32_t(uint16_t(*count * kCommandBytes)), os::OFFSET_CURRENT);
    os::read(file, count_word.b, 2);
  }
  const uint16_t bytes = uint16_t(*count * kCommandBytes);
  anim().anim_buffer = common::c_malloc(bytes);
  if (!anim().anim_buffer) {
    anim_error(no_script_memory_text.str());
    return 0;
  }
  anim().anim_commands = anim().anim_buffer;
  os::read(file, anim().anim_buffer, bytes);
  os::close(file);
  const Ptr<AnimCommand> commands = anim().anim_commands;
  for (int16_t k = 0; k < int16_t(*count) - 1; ++k) commands[k].next = (anim().anim_commands).addr + uint32_t((k + 1) * kCommandBytes);
  return 1;
}
ORIGINAL(load_anim_script, 0x2e2a0, 0, 0x208a8, 0, 0);

// $2dea4 (Title $204ac): the shapes of sequence n into anim_shapes from 1
// on: per shape its width (bytes) and height, then six planes and the
// mask, each copied into chip memory. Returns 0 on an error (the shapes
// counted so far are not recorded, so they are not freed).
int16_t load_anim_shapes(int16_t n) {
  const char* name;
  int32_t length, offset;
  if (n == kTitleSequence) {
    name = title_shape_file.str();
    length = kTitleShapesLength;
    offset = 0;
  } else {
    const Ptr<sbe32> offsets = ui::state().anim_shape_offsets;
    const int16_t part = anim_shape_parts[n];
    offset = offsets[part];
    length = offsets[part + 1] - offset;
    name = anim_shape_file.str();
  }
  const bool preloaded = ui::state().anim_shapes_preloaded != 0;
  uint32_t from = 0, file = 0;
  if (preloaded) {
    from = ui::state().preloaded_anim_shapes + uint32_t(offset);
  } else {
    file = os::open(name, os::MODE_OLDFILE);
    if (!file) {
      anim_error(name);
      return 0;
    }
    os::seek(file, offset, os::OFFSET_BEGINNING);
  }
  // The next `bytes` of the shapes into emulated memory at `to`.
  auto take = [&](uint32_t to, int32_t bytes) {
    if (preloaded) {
      os::copy_mem(from, to, uint32_t(bytes));
      from += uint32_t(bytes);
    } else {
      os::read(file, to, bytes);
    }
  };
  uint8_t size[2] = {};  // the shape's width and height
  int16_t count = 1;
  int16_t s = 1;
  for (int32_t at = 0; at < length; ++s) {
    if (preloaded) {
      size[0] = ptr_to<uint8_t>(from)[0];
      size[1] = ptr_to<uint8_t>(from)[1];
      from += 2;
    } else {
      os::read(file, &size[0], 1);
      os::read(file, &size[1], 1);
    }
    at += 2;
    ++count;
    const int32_t width = size[0] * 8, height = size[1], bytes = plane_bytes(size[0], size[1]);
    const uint32_t handle = anim_shapes.addr() + uint32_t(s) * kBitMapBytes;
    os::init_bitmap(handle, kAnimPlanes, width, height);
    BitMap& bm = bitmap(handle);
    for (int p = 0; p < kAnimPlanes; ++p) {
      bm.planes[p] = os::alloc_raster(width, height);
      if (!bm.planes[p]) {
        anim_error(no_plane_text.str());
        return 0;
      }
      take(bm.planes[p], bytes);
      at += bytes;
    }
    bm.planes[kMaskPlane] = os::alloc_raster(width, height);
    if (!bm.planes[kMaskPlane]) {
      anim_error(no_mask_text.str());
      return 0;
    }
    take(bm.planes[kMaskPlane], bytes);
    at += bytes;
  }
  anim().anim_shape_count = count;
  if (!preloaded) os::close(file);
  return 1;
}
ORIGINAL(load_anim_shapes, 0x2dea4, 0, 0x204ac, 0, 0);

// $2e416 (Title $20a1e): sequence n set up: its script (the header
// gives the objects, frames and ticks a frame), the shape of `variant` (if
// any) as anim_shapes[0], its shapes, its screen in bitmap 2; bitmaps 0
// and 1 cleared. Returns 0 on an error.
int16_t start_sequence(int16_t n, int16_t variant) {
  anim().anim_frames = 0;
  anim().anim_frame_ticks = 0;
  anim().anim_count = 0;
  anim().anim_shape_count = 0;
  anim().anim_variant = 0;
  if (!load_anim_script(n)) return 0;
  const AnimCommand& header = *anim().anim_commands;
  anim().anim_count = header.args[0];
  anim().anim_frames = header.args[1];
  anim().anim_frame_ticks = header.args[2];
  anim().anim_script = header.next;
  anim().anim_variant = variant;
  if (variant != kNoVariant) {
    load_shape(variant_shapes[variant]);
    bitmap(anim_shapes.addr()) = bitmap(shape_bitmap.addr());
  }
  if (!load_anim_shapes(n)) return 0;
  if (!show_screen(n)) return 0;
  for (int16_t k = 0; k < anim().anim_count; ++k) anim().anim_objects[k].frames = 0;
  draw_to(0);
  clear_screen();
  draw_to(1);
  clear_screen();
  reset_regions();
  return 1;
}
ORIGINAL(start_sequence, 0x2e416, 0, 0x20a1e, 0, 0);

// $2de04 (Title $2040c): sequence n played (with the shape of `variant`):
// a frame every anim_frame_ticks timer ticks, drawn into the hidden bitmap
// and then shown, until its end, a key or the select button. Then it is
// freed.
void play_sequence(int16_t n, int16_t variant) {
  if (!start_sequence(n, variant)) return;
  hide_pointer();
  int16_t bitmap = 0;
  input::flush_keys();
  for (int16_t frame = 0; frame < anim().anim_frames; ++frame) {
    if (input::getkey() | ui::state().select_button) break;
    const uint32_t deadline = audio::frames() + uint32_t(int32_t(anim().anim_frame_ticks));
    flush_regions(bitmap, kRestoreBitmap);
    animate(frame);
    cycle_colours();
    draw_objects(bitmap);
    show_bitmap(bitmap);
    bitmap = bitmap == 0 ? int16_t(1) : int16_t(0);
    hw::wait_until([&] { return audio::frames() >= deadline; });
  }
  anim_error(nullptr);
  input::flush_keys();
}
ORIGINAL(play_sequence, 0x2de04, 0, 0x2040c, 0, 0);

}  // namespace game::screen
