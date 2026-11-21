// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// The Amiga hardware the game code drives directly: the blitter (the
// screen library's blitter routines bypass graphics.library: a typed
// blitter object, the blit done at once).
#pragma once

#include "runtime/runtime.hpp"

namespace game::hw {

// The time a waiting OS call took where code that made one is replaced
// by code that makes none: one scanline under --det-timing (where only
// waiting calls take time), none otherwise. Keeps the reference
// recordings (tests/reference) comparable.
inline void os_wait_time() {
  if (runtime::g_machine->det_timing()) runtime::g_machine->idle_lines(1);
}

// A busy wait on memory that the interrupts change (the frame timer):
// scanlines pass until `done` holds.
template <typename F>
void wait_until(F done) {
  runtime::g_machine->idle_until(done);
}

// The blitter's registers, and a blit of `size` (height << 6 | words) with
// them. Blits are done at once (the original waited for the blitter
// before each one, and held the bus during them: nothing to do here).
using amiga::Blitter;
inline Blitter& blitter() { return runtime::g_machine->blitter(); }
inline void blit(uint16_t size) { runtime::g_machine->blit(size); }

inline constexpr uint16_t BLTCON0_USEA = 0x0800, BLTCON0_USEB = 0x0400, BLTCON0_USEC = 0x0200,
                          BLTCON0_USED = 0x0100;
inline constexpr uint16_t kAllBits = 0xffff;

}  // namespace game::hw
