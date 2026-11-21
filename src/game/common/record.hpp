// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// The record the loader (RedStorm) shares with every module.
#pragma once

#include "game/mem/types.hpp"

namespace game::common {

using namespace game::mem;

// The record the loader shares with the modules (164 bytes; a module finds
// it in the loader task's tc_UserData and copies the display and the
// preloaded files into its own copy of the screen library: load_view).
struct ModuleRecord {
  be32 gfx_base, intuition_base, diskfont_base;  // +0
  be16 drawing_to, showing;                      // +12: bitmap numbers
  be32 bitmaps[3];                               // +16
  be32 rastport, screen, window;                 // +28
  be32 fonts[4];                                 // +40
  be32 planes_buffer;                            // +56: chip memory for shapes
  be32 port, keyboard_io;                        // +60
  uint8_t unknown_68[4];
  // The preloaded files: per file its offset table (400 bytes), whether it
  // is in memory, and where.
  be16 scripts_preloaded;                        // +72: ATSCR
  be32 scripts;
  be32 anim_shape_offsets;                       // +78: ATBMS
  be16 anim_shapes_preloaded;
  be32 anim_shapes;
  be32 sample_offsets;                           // +88: SOUNDS
  be16 samples_preloaded;
  be32 samples;
  be32 shape_offsets;                            // +98: ships
  be16 shapes_preloaded;
  be32 shapes;
  be32 picture_offsets;                          // +108: pics
  be16 pictures_preloaded;
  be32 pictures;
  be32 osg_offsets;                              // +118: OSG
  be16 osg_preloaded;
  be32 osg;
  be32 shape_memory;                             // +128: the shape planes' chip memory (ui::State::shape_memory)
  be32 file_buffer;                              // +132
  be32 images;                                   // +136: Storm's image list (RedStorm's data)
  be32 frame_timer;                              // +140
  be32 pointer_8x8, pointer_7x7, pointer_none;   // +144: RedStorm's pointer images
  be32 player;                                   // +156: the player's record (148 bytes)
  be16 quit;                                     // +160: the quit key was pressed
  be16 mode;                                     // +162: in: RedStart's start mode; out: the module's result
};
static_assert(sizeof(ModuleRecord) == 164 && alignof(ModuleRecord) == 1);
// There is one module record: the loader makes it, its emulated
// address is the handle it puts in its task's tc_UserData for the modules;
// its fields are kept here. module_record(handle) is it (aborts on any
// other address).
ModuleRecord& module_record(uint32_t handle);
// The loader made a new record (cleared) at emulated address `handle`.
void new_module_record(uint32_t handle);

// The player's record: there
// is one, made by the loader and named by its emulated address
// (ModuleRecord::player, State::player_record: the handle); its bytes are
// kept here in the original's layout (big-endian fields, as the save files
// hold them). Each module reads it through its own view (Red's and
// RedStart's PlayerRecord, Storm's BattleStart and BattleRecord).
// 502 bytes: the loader allocated 148, the campaign's state follows (the
// original wrote it past the allocation, over the heap's free piece after
// it), and a save file's 500 bytes go in from +2.
inline constexpr int kPlayerRecordBytes = 502, kPlayerAllocated = 148;
uint8_t* player_record_bytes();
// The loader made a new record (cleared) at emulated address `handle`.
void new_player_record(uint32_t handle);
// The record seen as `T` (a struct of big-endian fields: alignment 1).
template <typename T>
T* player_record_as() {
  static_assert(sizeof(T) <= kPlayerRecordBytes && alignof(T) == 1);
  return reinterpret_cast<T*>(player_record_bytes());
}

}  // namespace game::common
