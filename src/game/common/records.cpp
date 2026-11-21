// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// The player's record and the module record kept in C++ (record.hpp).
#include "amiga/machine.hpp"
#include "game/common/record.hpp"
#include "game/mem/state.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace game::common {

namespace {
uint8_t g_bytes[kPlayerRecordBytes];
uint32_t g_handle;  // its emulated address (0: none made yet)
ModuleRecord g_module;
uint32_t g_module_handle;  // the module record's emulated address

// Big-endian bytes b[0..size) read into or written from `value`.
void access_bytes(uint8_t* b, int size, uint32_t& value, bool write) {
  if (!write) {
    value = 0;
    for (int k = 0; k < size; ++k) value = value << 8 | b[k];
  } else {
    for (int k = size - 1, v = int(value); k >= 0; --k, v >>= 8) b[k] = uint8_t(v);
  }
}

// A script's set/get through a record's pointer (`*a4+N+M`): the bytes
// here, big-endian as in memory.
bool access(uint32_t addr, int size, uint32_t& value, bool write) {
  if (g_handle && addr >= g_handle && addr + uint32_t(size) <= g_handle + kPlayerRecordBytes) {
    access_bytes(g_bytes + (addr - g_handle), size, value, write);
    return true;
  }
  if (g_module_handle && addr >= g_module_handle && addr + uint32_t(size) <= g_module_handle + sizeof(ModuleRecord)) {
    access_bytes(reinterpret_cast<uint8_t*>(&g_module) + (addr - g_module_handle), size, value, write);
    return true;
  }
  return false;
}

[[maybe_unused]] const bool registered_ = [] {
  mem::state::add_hashed(g_bytes, sizeof(g_bytes));
  mem::state::add_hashed(&g_module, sizeof(g_module));
  amiga::game_hooks.access_address = access;
  return true;
}();
}  // namespace

uint8_t* player_record_bytes() { return g_bytes; }

void new_player_record(uint32_t handle) {
  g_handle = handle;
  std::memset(g_bytes, 0, sizeof(g_bytes));
  mem::state::move_address_range(handle, kPlayerAllocated);
}

ModuleRecord& module_record(uint32_t handle) {
  if (handle != g_module_handle) {
    std::fprintf(stderr, "module record %06x is not the loader's (%06x)\n", handle, g_module_handle);
    std::abort();
  }
  return g_module;
}

void new_module_record(uint32_t handle) {
  g_module_handle = handle;
  g_module = {};
  mem::state::move_address_range(handle, sizeof(ModuleRecord));
}

}  // namespace game::common
