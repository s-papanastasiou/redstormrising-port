// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Game state kept in C++ instead of the emulated memory.
//
// A module's state moves out of its small-data area into a plain struct
// with native types, block by block. Each module registers its state once:
//
//   GAME_STATE(4, "red", state);
//
// The struct is reset to its initial value when the module starts (the C
// start-up clears the BSS it replaces), it is part of the RAM hash
// (--ram-hash, the determinism check), and the fields listed with their
// former a4 offsets stay reachable by the scenario scripts' set/get.
#pragma once

#include <cstdint>
#include <cstring>
#include <type_traits>

namespace game::mem::state {

// A field that a script may set or get by its former a4 offset.
struct Field {
  int32_t off;    // the a4 offset it had
  int size;       // 1, 2 or 4 bytes, as the script accesses it
  void* where;    // the native field (int8/16/32 or uint8/16/32)
  int width = 0;  // the field's size when wider: the script reaches its top bytes
  bool big = false;  // kept big-endian (as in the image)
};

// A range of a4 offsets whose variables moved here: nothing may reach them
// in the emulated memory any more (checked in the RSR_STATE_TRACE run).
struct Range {
  int32_t off;
  int32_t size;
};

struct Module {
  int module;               // runtime::module_index() order: storm, redstorm, title, redstart, red
  const char* name;
  void (*reset)();          // at every start (BSS); or
  void (*load)();           // at the first start after the module was loaded (initialised data)
  const void* bytes;        // the state struct, for the hash
  uint32_t size;
  const Range* moved;
  int moved_count;
  const Field* fields;
  int field_count;
};

// Registers a module's state (from a static initialiser).
bool add(const Module& m);
// The module starts: its BSS state back to the initial value; after a
// load, its data state taken from the loaded image.
void reset(int module);
// The loader loaded the module at `path` ("RedStormB:red", ...).
void loaded(const char* path);
// The image of a module's initialised data at a4 offset `off` (not
// traced: for `load` functions).
const uint8_t* loaded_image(int32_t off);
// a4 offsets [off, off + size) of `module` overlap a moved range.
bool moved(int module, int32_t off, uint32_t size);
// Emulated memory [addr, addr + size) of a heap object whose bytes moved
// into C++ (the player's record): nothing may reach it there any more.
void move_address_range(uint32_t addr, uint32_t size);
bool moved_address(uint32_t addr, uint32_t size);
// The range given back (its object freed: the memory may be reused).
void release_address_range(uint32_t addr, uint32_t size);
// Native bytes that are part of the RAM hash without a module (shared
// objects).
void add_hashed(const void* bytes, uint32_t size);

}  // namespace game::mem::state

#define GAME_STATE_CAT2(a, b) a##b
#define GAME_STATE_CAT(a, b) GAME_STATE_CAT2(a, b)
// GAME_STATE(module_index, "name", object, moved): `object` is reset to a
// value-initialised copy of its type; `moved` lists the a4 ranges it
// replaces. GAME_STATE_FIELDS(..., moved, fields): with the fields scripts
// may reach.
#define GAME_STATE_ADD(index, name, object, moved, fields, count)                                     \
  static_assert(std::is_trivially_copyable_v<std::remove_reference_t<decltype(object)>>);            \
  [[maybe_unused]] static const bool GAME_STATE_CAT(state_registered_, __LINE__) =                    \
      ::game::mem::state::add({index, name, [] { object = std::remove_reference_t<decltype(object)>{}; }, \
                               nullptr, &object, uint32_t(sizeof(object)), moved,                     \
                               int(sizeof(moved) / sizeof(moved[0])), fields, count})
#define GAME_STATE(index, name, object, moved) GAME_STATE_ADD(index, name, object, moved, nullptr, 0)
// GAME_STATE_DATA(module_index, "name", object, moved, load): state that
// replaces initialised data, kept while the module stays loaded; `load`
// takes it from the loaded image.
#define GAME_STATE_DATA(index, name, object, moved, load)                                             \
  static_assert(std::is_trivially_copyable_v<std::remove_reference_t<decltype(object)>>);            \
  [[maybe_unused]] static const bool GAME_STATE_CAT(state_registered_, __LINE__) =                    \
      ::game::mem::state::add({index, name, nullptr, load, &object, uint32_t(sizeof(object)), moved,  \
                               int(sizeof(moved) / sizeof(moved[0])), nullptr, 0})
#define GAME_STATE_FIELDS(index, name, object, moved, fields) \
  GAME_STATE_ADD(index, name, object, moved, fields, int(sizeof(fields) / sizeof(fields[0])))
