// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-FileCopyrightText: 2026 Ingo Ruhnke
// SPDX-License-Identifier: GPL-3.0-or-later

// graphics.library's BitMap as a host object.
// A BitMap is named by its address (where
// the original kept its 40 bytes: in a module's data, in a Screen, on the
// stack); Machine::bitmap(address) is the object. The planes are arena
// addresses (Machine::bytes).
#pragma once

#include <cstdint>

namespace amiga {

struct BitMap {
  uint16_t bytes_per_row = 0;
  uint16_t rows = 0;
  uint8_t flags = 0;
  uint8_t depth = 0;
  uint16_t pad = 0;
  uint32_t planes[8] = {};
};

inline constexpr uint32_t kBitMapBytes = 40;  // its size where the original kept it

}  // namespace amiga
