// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-FileCopyrightText: 2026 Ingo Ruhnke
// SPDX-License-Identifier: GPL-3.0-or-later

// The launcher process (the only one: the modules run in it): its exit
// ends the program.
#include "amiga/hle.hpp"

#include <algorithm>

namespace amiga {

namespace {
constexpr uint32_t LN_NAME = 10;
}  // namespace

void Hle::process_exit() {
  m.log("process %s exited with code %d\n", m.cstr(m.r32(task_ + LN_NAME)).c_str(), int(D(0)));
  m.exit_ = true;
  m.exit_code_ = int(D(0));
  m.cpu.stopped = true;
}

}  // namespace amiga
