// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// The C start-up and exit of the game modules (Aztec C's, the same code in
// Storm, Title, RedStart and Red): the module's BSS cleared, dos.library,
// the Workbench start message, main(0, message), and at the end the exit
// routine, the memory blocks freed, the message replied. exit() anywhere
// in the module ends main through ProgramExit, as the original's reset of
// the stack pointer did. Same order and memory effects as the original.
#include "game/common/memory.hpp"
#include "game/common/process.hpp"
#include "game/common/vars.hpp"
#include "game/mem/call.hpp"
#include "game/mem/os.hpp"
#include "game/mem/register.hpp"
#include "game/mem/state.hpp"
#include "game/screen/vars.hpp"
#include "game/ui/state.hpp"
#include "game/common/state.hpp"
#include "game/redstorm/calls.hpp"
#include "game/title/calls.hpp"

namespace game::common {

void storm_entry();  // module.cpp
void redstart_entry();
void red_entry();

namespace {
// The modules' mains (runtime::module_index() order).
void (*const kMains[5])() = {storm_entry, redstorm::redstorm_main, title::title_main, redstart_entry, red_entry};


// $20158: the end of the program (code: its exit code).
void exit_work() {
  // (The original first called exit_routine, the C runtime's atexit
  // hook: no module sets it.)
  free_blocks();
  os::close_library(ui::state().dos_base);
  // (A module started by the loader then replied its start message, in
  // Forbid; the loader calls it, and it returns.)
}
}  // namespace

// $20154: exit(code).
void program_exit(int32_t code) {
  exit_work();
  throw mem::ProgramExit{code};
}
ORIGINAL(program_exit, 0x20154, 0x20154, 0x20154, 0x20154, 0x20154);

// $20008: the module's start: its BSS cleared, dos.library opened, the
// current directory, main, the exit. Returns the exit code.
int32_t module_startup(const StartArgs* loader) {
  mem::ModuleBase base(module_a4.addr());
  clear_bss(bss_start.addr(), int32_t(bss_end.addr() - bss_start.addr()));
  mem::state::reset(runtime::module_index());  // the part of the BSS kept in C++
  state().exec_base_copy = os::exec_base();
  state().wb_message = loader ? 1 : 0;
  os::set_signal(0, kStartSignals);
  ui::state().dos_base = os::open_library(mem::code_address(kDosLibraryName), 0);
  const Process& p = process();
  if (loader) {
    state().stack_bound = p.sp_lower + kStackMargin;
    state().current_dir = loader->lock;
    os::current_dir(state().current_dir);
    state().program_name = loader->name;
  } else {
    // From the CLI (RedStorm): the stack bound from the stack the shell
    // gave; the command line is the program's name (RedStorm's main reads
    // neither argc nor argv: no argv is built).
    state().current_dir = p.current_dir;
    state().stack_bound = p.sp_upper - kCliStartFrame - p.stack_size + kStackMargin;
    state().program_name = p.command_name + 1;
  }
  // Returning from main is exit(0); an exit() inside did the exit work.
  const mem::MainEnd end = mem::call_main(kMains[runtime::module_index()]);
  if (end.exited) return end.exit_code;
  exit_work();
  return 0;
}
ORIGINAL(module_startup, 0x20008, 0x20008, 0x20008, 0x20008, 0x20008);

namespace {
[[maybe_unused]] const bool program_start_set_ = [] {
  amiga::game_hooks.program_start = [] { return module_startup(nullptr); };
  return true;
}();
}  // namespace
ORIGINAL_INSIDE(c_main, 0x33378, 0x22c3c, 0x247c4, 0x27afc, 0x2e2ec);  // _main: argv from the command line

}  // namespace game::common
