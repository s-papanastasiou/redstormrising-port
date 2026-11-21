// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// The game's process (process.hpp).
#include "game/common/process.hpp"

#include "game/mem/os.hpp"
#include "game/mem/state.hpp"
#include "game/mem/constants.hpp"
#include "game/mem/types.hpp"

#include <cstring>

namespace game::common {

namespace {
Process g_process;
bool g_taken;

[[maybe_unused]] const bool registered_ = [] {
  mem::state::add_hashed(&g_process, sizeof(g_process));
  return true;
}();
}  // namespace

Process& process() {
  if (!g_taken) {
    const os::ProcessStart s = os::process_start();
    g_process = Process{s.task, s.name, 0, 0, s.cli, s.current_dir, 0, s.sp_lower, s.sp_upper, s.stack_size,
                        s.command_name};
    g_taken = true;
  }
  return g_process;
}

namespace {
// The process's name: the OS's string, or after the loader renamed the
// process one of its constants.
const char* process_name() {
  const uint32_t n = process().name;
  return mem::constants::contains(n) ? mem::const_text(n) : mem::host_text(n);
}
}  // namespace

uint32_t find_task(const char* name) {
  os::find_task_time();
  if (!name) return process().task;
  return std::strcmp(name, process_name()) == 0 ? process().task : 0;
}

}  // namespace game::common
