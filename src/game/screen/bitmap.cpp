// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// BitMaps and RastPorts kept as host objects (bitmap.hpp): their bytes in
// emulated memory checked as moved.
#include "game/screen/bitmap.hpp"

#include "game/mem/state.hpp"

namespace game::screen {

namespace {
void moved(uint32_t addr, uint32_t size, bool is_moved) {
  if (is_moved) mem::state::move_address_range(addr, size);
  else mem::state::release_address_range(addr, size);
}

[[maybe_unused]] const bool registered_ = [] {
  amiga::game_hooks.object_moved = moved;
  return true;
}();
}  // namespace

}  // namespace game::screen
