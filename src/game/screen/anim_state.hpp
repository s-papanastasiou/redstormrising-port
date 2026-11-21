// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// The animation player's state (Storm's event pictures, Title's sequence),
// kept in C++ with a copy per module (mem/module_state.hpp). The shapes'
// BitMaps (anim_shapes) stay in memory: the blitter reads them, the objects
// point at them.
#pragma once

#include "game/mem/module_state.hpp"
#include "game/screen/vars.hpp"

namespace game::screen {

struct Anim {
  Ptr<AnimCommand> anim_commands;  // a header, then the script
  Ptr<AnimCommand> anim_script;    // the next command
  uint32_t anim_buffer;            // the script read from disk (allocated)
  int16_t anim_frames;             // the sequence's length
  int16_t anim_frame_ticks;        // timer ticks a frame
  int16_t anim_count;              // objects in use
  int16_t anim_shape_count;        // shapes loaded + 1 (from anim_shapes[1])
  int16_t anim_variant;            // index into the P offsets
  int16_t anim_c_params[5];        // set by C (read elsewhere)
  AnimObject anim_objects[20];
  RegionList region_lists[2];      // by bitmap
};

extern Anim anims[5];  // Storm's and Title's are used
Anim& anim();

#define ANIM_BE(member, size, storm, title) \
  MODULE_FIELD_BE(Anim, member, size, storm, mem::module_state::kAbsent, title, mem::module_state::kAbsent, mem::module_state::kAbsent)
#define ANIM_N(member, size, elem, storm, title) \
  MODULE_FIELD_N(Anim, member, size, elem, storm, mem::module_state::kAbsent, title, mem::module_state::kAbsent, mem::module_state::kAbsent)
// (anim_objects and region_lists: in the BSS, cleared at the start; their
// fields are native.)
inline constexpr mem::module_state::FieldAt kAnimFields[] = {
    ANIM_BE(anim_commands, 4, 5248, 172),
    ANIM_BE(anim_script, 4, 5252, 176),
    ANIM_N(anim_buffer, 4, 4, 5256, 180),
    ANIM_N(anim_frames, 2, 2, 5260, 184),
    ANIM_N(anim_frame_ticks, 2, 2, 5262, 186),
    ANIM_N(anim_count, 2, 2, 5264, 188),
    ANIM_N(anim_shape_count, 2, 2, 5266, 190),
    ANIM_N(anim_variant, 2, 2, 5268, 192),
    ANIM_N(anim_c_params, 10, 2, 5270, 194),
    ANIM_BE(anim_objects, 1600, 5280, 204),
    ANIM_BE(region_lists, 804, 8160, 3084),
};
#undef ANIM_BE
#undef ANIM_N

}  // namespace game::screen
