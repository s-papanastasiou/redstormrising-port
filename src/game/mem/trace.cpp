// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// RSR_STATE_TRACE (types.hpp): the game code's accesses to the running module's
// small-data area, counted by (module, a4 offset, size, declaration).
#include "game/mem/globals.hpp"
#include "game/mem/state.hpp"

#include <cstdio>
#include <cstdlib>
#include <map>
#include <tuple>

namespace game::mem::trace {

namespace {
const char* path() {
  static const char* p = std::getenv("RSR_STATE_TRACE");
  return p;
}
using Key = std::tuple<int, int32_t, uint32_t, int32_t>;  // module, offset, size, label
struct Log {
  std::map<Key, uint64_t> counts;
  ~Log() {
    if (!path() || counts.empty()) return;
    FILE* f = std::fopen(path(), "w");
    if (!f) return;
    std::fprintf(f, "# module offset size label count\n");
    for (const auto& [k, n] : counts)
      std::fprintf(f, "%d %d %u %d %llu\n", std::get<0>(k), std::get<1>(k), std::get<2>(k), std::get<3>(k),
                   (unsigned long long)n);
    std::fclose(f);
  }
};
Log& log() {
  static Log l;
  return l;
}
}  // namespace

bool on = path() != nullptr;

void note(uint32_t addr, uint32_t size, int32_t label) {
  if (!runtime::g_machine) return;
  if (state::moved_address(addr, size))
    std::fprintf(stderr, "STATE MOVED: address %06x (%u bytes) reached in emulated memory\n", addr, size);
  const int32_t off = int32_t(addr - a4());
  if (off < -65536 || off >= 32768) return;  // Red's data reaches below a4-32768
  const int module = runtime::module_index();
  ++log().counts[Key{module, off, size, label}];
  // RSR_TRACE_STOP=MODULE:OFF (debugging): stop at the first access that
  // covers a4+OFF of module MODULE (0-4), for a debugger's backtrace.
  static const char* stop = std::getenv("RSR_TRACE_STOP");
  if (stop) {
    int m = -1, o = 0;
    if (std::sscanf(stop, "%d:%d", &m, &o) == 2 && m == module && o >= off && o < off + int32_t(size)) std::abort();
  }
  if (state::moved(module, off, size))
    std::fprintf(stderr, "STATE MOVED: module %d a4%+d (%u bytes) reached in emulated memory\n", module, off, size);
}

}  // namespace game::mem::trace
