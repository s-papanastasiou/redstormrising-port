// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// The game's one process: the
// loader's, started from the CLI; the modules run inside it. The fields
// the game reads and writes in its process structure are kept here, taken
// once from the OS's task (os::process_start); the task's memory is no
// longer touched. FindTask still takes its time (find_task).
#pragma once

#include <cstdint>

namespace game::common {

struct Process {
  uint32_t task;         // the OS's task (what FindTask returns)
  uint32_t name;         // tc_Node.ln_Name (an emulated string)
  uint32_t user_data;    // tc_UserData: the module record's handle
  int32_t window_ptr;    // pr_WindowPtr (-1: no requesters)
  uint32_t cli;          // pr_CLI (a BPTR; 0 while a module runs)
  uint32_t current_dir;  // pr_CurrentDir
  uint32_t console;      // pr_ConsoleTask
  uint32_t sp_lower, sp_upper, stack_size;  // tc_SPLower, tc_SPUpper, pr_StackSize
  uint32_t command_name;  // the CLI's cli_CommandName (a BSTR's address)
};

Process& process();
// FindTask(name): the process if `name` is its name (nullptr: the
// calling one, always the process).
uint32_t find_task(const char* name);

}  // namespace game::common
