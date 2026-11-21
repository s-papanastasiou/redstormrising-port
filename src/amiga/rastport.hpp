// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-FileCopyrightText: 2026 Ingo Ruhnke
// SPDX-License-Identifier: GPL-3.0-or-later

// graphics.library's RastPort as a host object,
// named by the address of the original's
// 100 bytes (in a Screen, or the Window's): Machine::rastport(address).
// The fields the game and the drawing code use.
#pragma once

#include <cstdint>

namespace amiga {

struct RastPort {
  uint32_t bitmap = 0;     // +4: the BitMap drawn into (its handle)
  uint32_t area_ptrn = 0;  // +8: the area fill pattern (an address; 0: none)
  uint8_t mask = 0;        // +24: the planes written
  uint8_t fg_pen = 0;      // +25
  uint8_t bg_pen = 0;      // +26
  uint8_t aol_pen = 0;     // +27
  uint8_t draw_mode = 0;   // +28
  int8_t area_ptsz = 0;    // +29: log2 of the pattern's rows
  uint16_t line_ptrn = 0;  // +34
  int16_t cp_x = 0, cp_y = 0;  // +36: the pen position
  uint32_t font = 0;       // +52: the TextFont (its address)
  uint16_t tx_height = 0, tx_width = 0, tx_baseline = 0;  // +58
  int16_t tx_spacing = 0;  // +64
};

inline constexpr uint32_t kRastPortBytes = 100;

}  // namespace amiga
