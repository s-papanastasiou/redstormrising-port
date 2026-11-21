// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// RedStart (the menus before a game): its state, kept in C++
// (mem/module_state.hpp).
// vars.hpp keeps what is still in the emulated memory (text buffers,
// styles, the save and fame tables, texts).
#pragma once

#include "game/mem/module_state.hpp"
#include "game/mem/types.hpp"
#include "game/redstart/vars.hpp"

namespace game::redstart {

inline constexpr int kFileBufferBytes = 500;  // a saved game (the index: 400, the honor roll: 200)

struct State {
  uint32_t current_save;          // the save entry shown (its number; the original kept its address)
  uint32_t current_fame;          // the honor roll entry (its number, as current_save)
  int16_t start_mode;           // 1 a game is loaded (from the module's argument)
  uint8_t start_mode_byte;    // a4+16247
  uint16_t new_player;      // the player has no record yet
  int16_t era;            // the year chosen (0-3)
  int16_t rank_row, rank_code;  // the row chosen; row * 8 + a free bit
  int16_t difficulty;
  int16_t scenario;
  char commander_name[32];    // " " then the name typed (at most 16)
  uint32_t save_file;             // the file being read or written
  uint8_t file_buffer[kFileBufferBytes];  // the save index, the honor roll or a saved game
  uint8_t unread_a, unread_b;  // cleared at the start (nothing reads them)
  uint16_t unread_c;                   // the same
};

extern State states[5];  // only RedStart's (index 3) is used
State& state();
// The file buffer seen as the save index and as the honor roll.
inline SaveEntry* saves() { return reinterpret_cast<SaveEntry*>(state().file_buffer); }
inline FameEntry* fames() { return reinterpret_cast<FameEntry*>(state().file_buffer); }

inline constexpr int32_t X = mem::module_state::kAbsent;
#define REDSTART_BE(member, size, offset) MODULE_FIELD_BE(State, member, size, X, X, X, offset, X)
#define REDSTART_N(member, size, elem, offset) MODULE_FIELD_N(State, member, size, elem, X, X, X, offset, X)
inline constexpr mem::module_state::FieldAt kFields[] = {
    REDSTART_N(current_save, 4, 4, 16300),
    REDSTART_N(current_fame, 4, 4, 16296),
    REDSTART_N(start_mode, 2, 2, 16228),
    REDSTART_BE(start_mode_byte, 1, 16247),
    REDSTART_N(new_player, 2, 2, 16226),
    REDSTART_N(era, 2, 2, -31190),
    REDSTART_N(rank_row, 2, 2, 16254),
    REDSTART_N(rank_code, 2, 2, 16252),
    REDSTART_N(difficulty, 2, 2, -30508),
    REDSTART_N(scenario, 2, 2, 16250),
    REDSTART_BE(commander_name, 32, 16256),
    REDSTART_N(save_file, 4, 4, 16288),
    REDSTART_BE(file_buffer, kFileBufferBytes, 14008),
    REDSTART_BE(unread_a, 1, 16224),
    REDSTART_BE(unread_b, 1, 16225),
    REDSTART_N(unread_c, 2, 2, 16330),
};
#undef REDSTART_BE

}  // namespace game::redstart
