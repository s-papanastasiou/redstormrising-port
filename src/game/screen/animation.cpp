// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Screen: animated objects (Storm and Title): drawn with draw_shape, the
// areas they cover queued to be copied back from the other bitmap before
// the next frame, moved every frame and changed by a script of commands.
#include "game/audio/calls.hpp"
#include "game/mem/register.hpp"
#include "game/screen/calls.hpp"
#include "game/screen/vars.hpp"
#include "game/ui/state.hpp"
#include "game/screen/anim_state.hpp"

namespace game::screen {

namespace {
// Queue the area of `shape` at the object's position in region list n.
void queue_region(int16_t n, const AnimObject& o, const BitMap& shape) {
  RegionList& list = anim().region_lists[n];
  Region& r = list.entries[uint16_t(list.count)];
  r.x = o.x;
  r.y = o.y;
  r.w = int16_t(shape.bytes_per_row * 8);
  r.h = int16_t(shape.rows);
  list.count += 1;
}
}  // namespace

// $2e682: copy the queued regions of list n back from bitmap `from` into
// bitmap n, and empty the list.
void flush_regions(int16_t n, int16_t from) {
  RegionList& list = anim().region_lists[n];
  for (int16_t i = 0; i < int16_t(list.count); ++i) {
    const Region& r = list.entries[i];
    copy_region(bitmap_at(from), bitmap_at(n), r.x, r.y, r.w, r.h);
  }
  list.count = 0;
}
ORIGINAL(flush_regions, 0x2e682, 0, 0x20c8a, 0, 0);

// $2e532: draw object n into bitmap `to` (if its frame has a shape) and
// queue its area in both region lists.
void draw_object(int16_t n, int16_t to) {
  AnimObject& o = anim().anim_objects[n];
  const BitMapRef shape = o.shapes[o.frame];
  if (!shape) return;
  draw_shape(*shape, bitmap_at(to), o.x, o.y);
  for (int16_t list = 0; list < 2; ++list) queue_region(list, o, *shape);
}
ORIGINAL(draw_object, 0x2e532, 0, 0x20b3a, 0, 0);

// $2e730: draw the objects into bitmap `to`: each shown one with its
// current shape (its area queued in list `to`), or for frame -1 its fixed
// area restored from the clean background.
void draw_objects(int16_t to) {
  for (int16_t i = 0; i < anim().anim_count; ++i) {
    AnimObject& o = anim().anim_objects[i];
    if (!o.frames) continue;
    if (o.frame != -1) {
      const BitMapRef shape = o.shapes[o.frame];
      if (!shape) continue;
      draw_shape(*shape, bitmap_at(to), o.x, o.y);
      queue_region(to, o, *shape);
    } else {
      blit(bitmap_at(kRestoreBitmap), o.x, o.y, bitmap_at(to), o.x, o.y, o.w, o.h);
    }
  }
}
ORIGINAL(draw_objects, 0x2e730, 0, 0x20d38, 0, 0);

// $2e896: one frame of animation: move the objects and cycle their
// shapes, then run the script's commands for `frame`.
void animate(int16_t frame) {
  for (int16_t i = 0; i < anim().anim_count; ++i) {
    AnimObject& o = anim().anim_objects[i];
    o.x += o.vx;
    o.y += o.vy;
    if (o.frames > 1) {
      o.frame += 1;
      if (uint16_t(o.frame) == uint16_t(o.frames)) o.frame = 0;
    }
  }
  for (;;) {
    AnimCommand& cmd = *anim().anim_script;
    if (cmd.frame != uint16_t(frame)) return;
    AnimObject& o = anim().anim_objects[cmd.object];
    switch (cmd.letter | 0x20) {  // either case
      case 'c':
        anim().anim_c_params[0] = cmd.args[0];
        anim().anim_c_params[1] = cmd.args[1];
        anim().anim_c_params[2] = cmd.args[2];
        anim().anim_c_params[3] = cmd.args[2];
        anim().anim_c_params[4] = cmd.args[3];
        break;
      case 'p':
        o.x = cmd.args[0];
        o.y = cmd.args[1];
        if (cmd.args[2] == 1) o.x += anim_x_offsets[anim().anim_variant];
        if (cmd.args[3] == 1) o.y += anim_y_offsets[anim().anim_variant];
        break;
      case 'm':
        o.vx = cmd.args[0];
        o.vy = cmd.args[1];
        break;
      case 's':
        draw_object(cmd.object, 2);
        break;
      case 'x':
        audio::play_effect(cmd.args[0]);
        break;
      case 'f':
        o.frames = 1;
        o.frame = -1;
        o.x = cmd.args[0];
        o.y = cmd.args[1];
        o.w = cmd.args[2];
        o.h = cmd.args[3];
        break;
      case 'i':
        o.frames = cmd.count;
        o.frame = 0;
        for (int16_t k = 0; k < cmd.count; ++k)
          o.shapes[k] = cmd.args[k] == -1 ? 0u : anim_shapes.addr() + uint32_t(int32_t(cmd.args[k])) * kBitMapBytes;
        break;
      default:
        break;
    }
    anim().anim_script = cmd.next.addr;
  }
}
ORIGINAL(animate, 0x2e896, 0, 0x20e9e, 0, 0);

// $2e63a: both region lists hold the whole screen.
void reset_regions() {
  for (int16_t n = 0; n < 2; ++n) {
    RegionList& list = anim().region_lists[n];
    list.count = 1;
    list.entries[0].x = 0;
    list.entries[0].y = 0;
    list.entries[0].w = 320;
    list.entries[0].h = 200;
  }
}
ORIGINAL(reset_regions, 0x2e63a, 0, 0x20c42, 0, 0);

// $2ebd6: colour cycling (set by the script's C command): every
// `period` calls a range of the screen's colour table rotates by one
// entry, towards its start for direction 1, else towards its end.
void cycle_colours() {
  int16_t& count = anim().anim_c_params[0];
  const int16_t start = anim().anim_c_params[1], period = anim().anim_c_params[2];
  int16_t& countdown = anim().anim_c_params[3];
  const int16_t direction = anim().anim_c_params[4];
  if (count) countdown -= 1;
  if (countdown) return;
  countdown = period;
  uint16_t* t = ui::state().screen->viewport.colormap.table;
  const int16_t n = count;
  if (direction == 1) {
    const uint16_t first = t[start];
    for (int16_t i = 0; i < n - 1; ++i) t[start + i] = t[start + i + 1];
    t[start + n - 1] = first;
  } else {
    const uint16_t last = t[start + n - 1];
    for (int16_t i = int16_t(n - 1); i > 0; --i) t[start + i] = t[start + i - 1];
    t[start] = last;
  }
}
ORIGINAL(cycle_colours, 0x2ebd6, 0, 0x211de, 0, 0);

}  // namespace game::screen
