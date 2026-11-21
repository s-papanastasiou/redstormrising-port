// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-FileCopyrightText: 2026 Ingo Ruhnke
// SPDX-License-Identifier: GPL-3.0-or-later

// intuition.library's Screen and Window (with the Screen's ViewPort,
// RasInfo and ColorMap) as host objects,
// named by the addresses the original's
// structures had (their memory is still allocated: handles and the heap's
// layout). The fields the game and the emulation use.
#pragma once

#include <cstdint>
#include <vector>

namespace amiga {

struct ColorMap {
  uint16_t count = 32;     // entries
  uint16_t table[32] = {};  // $0RGB
};

struct ViewPort {
  int16_t dwidth = 0, dheight = 0, dxoffset = 0, dyoffset = 0;
  uint16_t modes = 0;      // HIRES $8000, HAM $0800, DUALPF $0400, LACE 4, VP_HIDE $2000
  uint32_t bitmap = 0;     // its RasInfo's BitMap (a handle)
  int16_t rx = 0, ry = 0;  // its RasInfo's scroll offsets
  ColorMap colormap;
  uint32_t colormap_handle = 0, rasinfo_handle = 0;  // where the original's were
  uint32_t dsp = 0;        // its DspIns token (LoadRGB4 updates the bands of this one)
};

struct Screen {
  uint32_t handle = 0;     // its address
  int16_t left = 0, top = 0, width = 0, height = 0;
  uint16_t flags = 0;      // NewScreen.Type
  uint32_t title = 0;
  uint8_t detail_pen = 0, block_pen = 0;
  ViewPort viewport;       // at handle + 44 (the address LoadRGB4 and SetRGB4 get)
  uint32_t rastport = 0;   // handle + 84
  uint32_t bitmap = 0;     // handle + 184
};

struct Window {
  uint32_t handle = 0;
  int16_t left = 0, top = 0, width = 0, height = 0;
  uint8_t detail_pen = 0, block_pen = 0;
  uint32_t flags = 0, idcmp = 0, title = 0;
  uint32_t screen = 0;     // its Screen (a handle)
  uint32_t rastport = 0;   // its own RastPort (a handle)
  bool user_port = false;  // IDCMP messages go to it
  int signal = 0;          // its UserPort's signal bit
  uint32_t pointer = 0;    // SetPointer's image (an address: its name)
  std::vector<uint16_t> pointer_words;  // the image (two control words, then two per row)
  uint8_t ptr_height = 0, ptr_width = 0;
  int8_t x_offset = 0, y_offset = 0;
  int16_t mouse_x = 0, mouse_y = 0;  // relative to its top-left corner
};

// OpenScreen's and OpenWindow's arguments (the fields used).
struct NewScreen {
  int16_t left = 0, top = 0, width = 0, height = 0, depth = 0;
  uint8_t detail_pen = 0, block_pen = 0;
  uint16_t modes = 0, type = 0;
  uint32_t title = 0, custom_bitmap = 0;
};
struct NewWindow {
  int16_t left = 0, top = 0, width = 0, height = 0;
  uint8_t detail_pen = 0, block_pen = 0;
  uint32_t idcmp = 0, flags = 0, title = 0;
  uint32_t screen = 0;  // CUSTOMSCREEN: its Screen (a handle)
};

inline constexpr uint32_t kScreenBytes = 346, kWindowBytes = 132, kScreenViewPort = 44;

}  // namespace amiga
