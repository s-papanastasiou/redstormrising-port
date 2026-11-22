// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// A module's main and exit; its small-data base; code addresses the
// original stores as data.
#pragma once

#include "runtime/runtime.hpp"


namespace game::mem {

// A program's exit (the C runtime's exit()): thrown by the native exit
// routine, caught by call_main, which returns the code.
struct ProgramExit {
  int32_t code;
};

// A module's main run to its end or to an exit() inside it (then
// `exit_code` gets the code), with the time the original's call took.
struct MainEnd {
  bool exited;
  int32_t exit_code;
};
inline MainEnd call_main(void (*main)()) {
  try {
    runtime::g_machine->call_timed(main);
    return {false, 0};
  } catch (const ProgramExit& e) {
    return {true, e.code};
  }
}

// A module's small-data base (a4) for the lifetime of the object (a
// program's start-up sets it; the caller's comes back afterwards).
class ModuleBase {
public:
  explicit ModuleBase(uint32_t a4) : saved_(runtime::cpu().a[4]) { runtime::cpu().a[4] = a4; }
  ~ModuleBase() { runtime::cpu().a[4] = saved_; }
  ModuleBase(const ModuleBase&) = delete;
  ModuleBase& operator=(const ModuleBase&) = delete;

private:
  uint32_t saved_;
};

// The emulated address of code at `tool_addr` (tools' layout) of the
// running module, for code pointers the original stores.
inline uint32_t code_address(uint32_t tool_addr) { return runtime::maddr(tool_addr); }

// The manual protection is bypassed (the default; --protection keeps
// it). The emulator patches the original's check; the game code that replaces
// the patched routine asks here.
inline bool protection_bypassed() { return runtime::g_machine->bypass_protection; }

// The alternative colours (--alternative-color; the default is the
// original's): where the port offers a calmer colour than the original.
inline bool alternative_color() { return runtime::g_machine->alternative_color; }

// The detailed map (--detailed-map; the default is the original's): the
// naval ports marked on the strategic map (red/map_marks.cpp).
inline bool detailed_map() { return runtime::g_machine->detailed_map; }

}  // namespace game::mem
