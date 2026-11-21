// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-FileCopyrightText: 2026 Ingo Ruhnke
// SPDX-License-Identifier: GPL-3.0-or-later

// What is left of the 68000: the registers the game's conventions still
// use (a4, the running module's small-data base; d0, a result) and the
// emulated time, counted in 68000 clock cycles (the OS calls take it, the
// scanlines and frames are measured in it).
#pragma once

#include <cstdint>

namespace amiga {

struct Cpu {
  uint32_t d[8] = {};   // d0: a routine's result
  uint32_t a[8] = {};   // a4: the running module's small-data base; a7: the stack
  uint32_t pc = 0;      // the program's first code (where LoadSeg put it)
  uint8_t imask = 7;    // interrupts at or below this level are held
  bool stopped = false; // the program has ended

  int64_t cycles = 0;   // emulated time in 68000 clock cycles
};

}  // namespace amiga
