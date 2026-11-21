// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-FileCopyrightText: 2026 Ingo Ruhnke
// SPDX-License-Identifier: GPL-3.0-or-later

// The blitter: its registers
// as a typed object the game's blitter routines set, and a blit that runs
// at once when started (Machine::blit). There is no register file: the
// values stay from one blit to the next, as the chip's registers did.
#pragma once

#include <cstdint>

namespace amiga {

struct Blitter {
  enum Channel { A, B, C, D };
  uint16_t con0 = 0, con1 = 0;            // BLTCON0, BLTCON1
  uint16_t afwm = 0xFFFF, alwm = 0xFFFF;  // A's first and last word masks
  uint32_t pt[4] = {};                    // channel pointers (A B C D)
  int16_t mod[4] = {};                    // modulos (A B C D)
  uint16_t dat[3] = {};                   // A, B, C data
  bool zero = true;                       // the last blit wrote only zeros (DMACONR)
};

}  // namespace amiga
