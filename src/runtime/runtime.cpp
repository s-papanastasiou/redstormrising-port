// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

#include "runtime/runtime.hpp"

#include <cstdlib>
#include <cstring>
#include <vector>

namespace runtime {

amiga::Machine* g_machine = nullptr;


int module_number(const char* name) {
  static const char* names[] = {"storm", "redstorm", "title", "redstart", "red"};
  for (int i = 0; i < 5; ++i)
    if (std::strcmp(names[i], name) == 0) return i;
  std::abort();
}

int module_index() { return g_machine->module_number(); }

namespace {
// Measured from the copies of getkey, fill_rect, select_entry, ...
// (module_index() order).
const struct { const char* name; LibShift shift; } shifts[] = {
      {"storm", {0, 0}},          {"redstorm", {624, 12492}}, {"title", {-5112, -11880}},
      {"redstart", {19652, -5340}}, {"red", {7324, 21820}},
};
}  // namespace


const LibShift& lib_shift() { return shifts[module_index()].shift; }

uint32_t expected_size(const char* module) {
  // WHDLoad install, data/ directory (README: program structure).
  static const struct { const char* name; uint32_t size; } sizes[] = {
      {"redstorm", 29948}, {"title", 41948}, {"redstart", 83512},
      {"red", 125232},     {"storm", 111208},
  };
  for (auto& s : sizes)
    if (std::strcmp(s.name, module) == 0) return s.size;
  return 0;
}

}  // namespace runtime
