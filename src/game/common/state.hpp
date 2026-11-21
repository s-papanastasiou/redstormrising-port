// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// The C runtime's and the module hand-over's state (every module), kept in
// C++ with a copy per module (mem/module_state.hpp). vars.hpp keeps what is
// still in the emulated memory: Red's and Storm's own text buffers, the
// stdio file list and handle table (their addresses link them), constant
// tables.
#pragma once

#include "game/mem/module_state.hpp"
#include "game/mem/types.hpp"

namespace game::common {

using mem::be16;
using mem::be32;
using mem::sbe16;
using mem::sbe32;

struct State {
  // The text helpers' result buffers (strings.cpp).
  char degrees_text[5], number_digits[8], number_text[8], long_digits[8], long_text[9];
  char thousands_text[20], concat_text[100], static_text[100];
  uint32_t random_seed;  // the random generator
  uint32_t block_list;  // the C allocator's blocks: first
  uint32_t block_list_tail;  // last
  uint32_t first_block;
  int32_t free_bytes;
  uint32_t deferred_free;  // freed at the next malloc
  uint32_t exec_base_copy;
  uint32_t wb_message;  // started by the loader (the original: its WBStartup message; 0 from the CLI)
  uint32_t current_dir;
  uint32_t stack_bound;
  uint32_t program_name;
  uint32_t tool_window;  // its console, if any
  uint32_t exit_routine;  // called first at exit
  int16_t fd_count;
  uint32_t break_handler;  // asked on Ctrl-C/D
  int16_t dos_error;  // IoErr of the last call
  int16_t c_errno;
  uint32_t module_record;  // from the loader's task
  uint32_t player_record;
  uint16_t start_mode;  // the record's mode, as loaded
};

// Each module's copy, and the running module's.
extern State states[5];
State& state();

inline constexpr int32_t X = mem::module_state::kAbsent;
#define COMMON_BE(member, size, ...) MODULE_FIELD_BE(State, member, size, __VA_ARGS__)
#define COMMON_N(member, size, elem, ...) MODULE_FIELD_N(State, member, size, elem, __VA_ARGS__)
inline constexpr mem::module_state::FieldAt kFields[] = {
    COMMON_BE(degrees_text, 5, 9820, X, X, 29772, 17920),
    COMMON_BE(number_digits, 8, 9825, X, X, 29777, 17925),
    COMMON_BE(number_text, 8, 9833, X, X, 29785, 17933),
    COMMON_BE(long_digits, 8, 9841, X, X, 29793, 17941),
    COMMON_BE(long_text, 9, 9849, X, X, 29801, 17949),
    COMMON_BE(thousands_text, 20, 9924, X, X, 30660, 18024),
    COMMON_BE(concat_text, 100, X, X, X, 29496, 18044),
    COMMON_BE(static_text, 100, X, X, X, 29596, 18144),
    COMMON_N(random_seed, 4, 4, -9392, X, X, 13904, 18592),
    COMMON_N(block_list, 4, 4, 10200, 10692, 4988, 30936, 18572),
    COMMON_N(block_list_tail, 4, 4, 10204, 10696, 4992, 30940, 18576),
    COMMON_N(first_block, 4, 4, -9748, 9576, -14808, X, X),
    COMMON_N(free_bytes, 4, 4, -9734, 9590, -14794, X, X),
    COMMON_N(deferred_free, 4, 4, 10212, 10704, 5000, X, X),
    COMMON_N(exec_base_copy, 4, 4, -9332, 9988, -14396, 13964, 14344),
    COMMON_N(wb_message, 4, 4, -9328, 9992, -14392, 13968, 14348),
    COMMON_N(current_dir, 4, 4, -9336, 9984, -14400, 13960, 14340),
    COMMON_N(stack_bound, 4, 4, -9384, 9936, -14448, 13912, 14292),
    COMMON_N(program_name, 4, 4, -9316, 10004, -14380, 13980, 14360),
    COMMON_N(tool_window, 4, 4, -9320, 10000, -14384, 13976, 14356),
    COMMON_N(exit_routine, 4, 4, -9344, 9976, -14408, 13952, 14332),
    COMMON_N(fd_count, 2, 2, -9752, 9568, -14812, 13568, 13952),
    COMMON_N(break_handler, 4, 4, -9340, 9980, -14404, 13956, 14336),
    COMMON_N(dos_error, 2, 2, -9364, 9956, -14428, 13932, 14312),
    COMMON_N(c_errno, 2, 2, 10192, 10684, 4980, 30928, 18564),
    COMMON_N(module_record, 4, 4, 5230, X, 162, 28642, 16278),
    COMMON_N(player_record, 4, 4, 5234, X, 166, 28646, 16282),
    COMMON_N(start_mode, 2, 2, 5228, X, 160, 28640, 16276),
};
#undef COMMON_BE

}  // namespace game::common
