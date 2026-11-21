// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// State of code linked into several modules (the screen/input library, the
// sound code, the menus), kept in C++ with one copy per module, as each
// module had its own in its small-data area.
//
// Each field was a variable at an a4 offset of each module that has it
// (a FieldAt table). Where that was the C start-up's BSS it is cleared when
// the module starts; where it was initialised data it is taken from the
// image when the module is loaded and kept while it stays loaded. The
// a4 ranges are registered as moved (mem/state.hpp: the trace check).
#pragma once

#include <climits>
#include <cstddef>
#include <cstdint>

namespace game::mem::module_state {

inline constexpr int32_t kAbsent = INT32_MIN;  // the module has no such variable

// A field: its a4 offset in storm, redstorm, title, redstart, red, its
// size, its place in the state struct, and whether it is kept big-endian
// (copied from the image as it is) or as native integers of `elem` bytes
// (an array when size > elem; 0: one of `size` bytes).
struct FieldAt {
  int32_t off[5];
  int size;
  size_t member;
  bool big = false;
  int elem = 0;
};

// Each module's BSS as a4 offsets [from, to) (bss_start and bss_end minus
// the module's a4): what the C start-up clears. (The libraries' blocks
// called "Bss" in their declarations are initialised data.)
struct A4Range {
  int32_t from, to;
};
inline constexpr A4Range kBss[5] = {{-9388, 10216}, {9932, 10708}, {-14452, 5004}, {13908, 30948}, {14288, 18584}};

// Registers the five copies `states` (each `state_size` bytes) of a state
// struct described by `fields`.
bool add(const char* tag, void* states, size_t state_size, const FieldAt* fields, int count);

}  // namespace game::mem::module_state

// A field of `State` as native integer / kept big-endian.
#define MODULE_FIELD(State, member, size, storm, redstorm, title, redstart, red) \
  ::game::mem::module_state::FieldAt{{storm, redstorm, title, redstart, red}, size, offsetof(State, member)}
#define MODULE_FIELD_N(State, member, size, elem, storm, redstorm, title, redstart, red) \
  ::game::mem::module_state::FieldAt{{storm, redstorm, title, redstart, red}, size, offsetof(State, member), false, elem}
#define MODULE_FIELD_BE(State, member, size, storm, redstorm, title, redstart, red) \
  ::game::mem::module_state::FieldAt{{storm, redstorm, title, redstart, red}, size, offsetof(State, member), true}
// Registers `states` (State[5]) with `fields` (a constexpr array).
#define MODULE_STATE(tag, states, fields)                                                             \
  [[maybe_unused]] static const bool module_state_registered_ = ::game::mem::module_state::add(       \
      tag, states, sizeof(states[0]), fields, int(sizeof(fields) / sizeof(fields[0])))
