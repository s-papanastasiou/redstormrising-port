// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// The running program's context for the game's C++ code (src/game): the
// machine, the running module (its number, its load address, the shifts
// of the shared library's variables in it), and ORIGINAL_IN, which records
// where a function was in the original.
//
// Addresses in the tools' layout (first hunk's code at $20008, as in the
// disassembly and the notes) are converted to where the module is loaded:
// block + (address - LOAD_BASE).
#pragma once

#include "amiga/hunk.hpp"
#include "amiga/machine.hpp"

#include <cstdint>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace runtime {

template <typename T> struct FnArgs;
template <typename R, typename... A> struct FnArgs<R (*)(A...)> {
  using ret = R;
  using type = std::tuple<A...>;
};

// ---- machine access -------------------------------------------------------

extern amiga::Machine* g_machine;
inline amiga::Machine& mach() { return *g_machine; }
inline amiga::Cpu& cpu() { return g_machine->cpu; }

inline uint8_t rd8(uint32_t a) { return g_machine->read8(a); }
inline uint16_t rd16(uint32_t a) { return g_machine->read16(a); }
inline uint32_t rd32(uint32_t a) { return g_machine->read32(a); }
inline void wr8(uint32_t a, uint32_t v) { g_machine->write8(a, uint8_t(v)); }
inline void wr16(uint32_t a, uint32_t v) { g_machine->write16(a, uint16_t(v)); }
inline void wr32(uint32_t a, uint32_t v) { g_machine->write32(a, v); }

namespace detail {
template <typename T> T load(uint32_t a) {
  if constexpr (sizeof(T) == 1) return T(rd8(a));
  else if constexpr (sizeof(T) == 2) return T(rd16(a));
  else return T(rd32(a));
}
template <typename T> void store(uint32_t a, T v) {
  if constexpr (sizeof(T) == 1) wr8(a, uint32_t(v));
  else if constexpr (sizeof(T) == 2) wr16(a, uint32_t(v));
  else wr32(a, uint32_t(v));
}
}  // namespace detail

// A run-time address in the running native routine's module, from its
// address in the tools' layout (e.g. data referenced absolutely, $34c70).
inline uint32_t maddr(uint32_t tool_addr) {
  return g_machine->module_block() + (tool_addr - amiga::LOAD_BASE);
}

// The screen/input library linked into every module (getkey, fill_rect,
// ...) keeps its variables in two blocks of the small-data area whose a4
// offsets differ between modules by a constant per block. LibVar takes
// the offset in Storm and adds the running module's shift.
enum class Block { Data, Bss };
struct LibShift {
  int32_t data, bss;
};
const LibShift& lib_shift();  // of the running routine's module

// Index of the running routine's module: 0 storm, 1 redstorm, 2 title,
// 3 redstart, 4 red. by_module picks the constant for it (for addresses
// outside the shared library chunk, which differ irregularly). Addresses
// given this way are in the tools' layout: use maddr() for a pointer
// value (call/call_raw convert their arguments themselves).
// The game's programs, in the order of module_index() and of every
// per-module table.
enum ModuleNumber : int { kStormModule = 0, kRedStormModule = 1, kTitleModule = 2, kRedStartModule = 3, kRedModule = 4 };
int module_index();
int module_number(const char* name);  // the module_index() of a module's file name
inline uint32_t by_module(const uint32_t (&v)[5]) { return v[module_index()]; }
// Expected size of each module file this port was made for (the WHDLoad
// data/ build): another build is refused when it is loaded.
uint32_t expected_size(const char* module);

}  // namespace runtime

// Where a function was in the original: the routine's address in the
// tools' layout of a module, for comparing with the disassembly
// (documentation only; tools/analysis/registrations.py reads them):
//   ORIGINAL_IN("red", 0x220ec, phase_values);
//   ORIGINAL_IN("title", 0x226bc, set_tone);  (another module's copy)
#define ORIGINAL_IN(module, addr, fn)
