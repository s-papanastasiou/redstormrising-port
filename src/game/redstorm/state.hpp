// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// RedStorm (the loader): its state, kept in C++ (mem/module_state.hpp).
// vars.hpp keeps what is still in the emulated memory (the
// NewScreen/NewWindow it passes to Intuition, texts).
#pragma once

#include "game/common/record.hpp"
#include "game/mem/module_state.hpp"
#include "game/mem/types.hpp"

namespace game::redstorm {

using mem::be16;
using mem::be32;

struct State {
  uint16_t more_memory;          // over 645000 bytes free: the modules stay loaded
  uint16_t lots_memory;          // over 900000: pics, ships, SOUNDS preloaded
  uint16_t most_memory;          // over 1000000: OSG, ATBMS, ATSCR too
  uint16_t workbench_closed;
  uint16_t mode;                 // what the last module asked for
  uint32_t record;  // the record given to the modules (its handle: common::module_record)
  uint32_t player;               // the player's record
  uint32_t module_handles[3];    // [Module]: kRed, kStorm, kRedStart
  uint32_t font_segments[4];     // [1-3]: the fonts' segments (LoadSeg)
  uint16_t quit;                 // the quit key was pressed (the record's quit, back from a module)
};

extern State states[5];  // only RedStorm's (index 1) is used
State& state();

inline constexpr int32_t X = mem::module_state::kAbsent;
#define LOADER_BE(member, size, offset) MODULE_FIELD_BE(State, member, size, X, offset, X, X, X)
#define LOADER_N(member, size, elem, offset) MODULE_FIELD_N(State, member, size, elem, X, offset, X, X, X)
inline constexpr mem::module_state::FieldAt kFields[] = {
    LOADER_N(more_memory, 2, 2, 0),
    LOADER_N(lots_memory, 2, 2, 2),
    LOADER_N(most_memory, 2, 2, 4),
    LOADER_N(workbench_closed, 2, 2, 1498),
    LOADER_N(mode, 2, 2, 10024),
    LOADER_N(record, 4, 4, 10026),
    LOADER_N(player, 4, 4, 10030),
    LOADER_N(module_handles[0], 4, 4, 10034),
    LOADER_N(module_handles[1], 4, 4, 10038),
    LOADER_N(module_handles[2], 4, 4, 10042),
    LOADER_N(font_segments, 16, 4, 10100),
    LOADER_N(quit, 2, 2, 6),
};
#undef LOADER_BE

}  // namespace game::redstorm
