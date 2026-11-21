// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Per-module copies of shared code's state (module_state.hpp).
#include "game/mem/module_state.hpp"

#include "game/mem/state.hpp"

#include <cstring>
#include <vector>

namespace game::mem::module_state {

namespace {
struct Entry {
  uint8_t* states;
  size_t size;
  const FieldAt* fields;
  int count;
  std::vector<state::Range> moved[5];
  std::vector<state::Field> script[5];  // reachable by set/get at the old offsets
};
std::vector<Entry*>& entries() {
  static std::vector<Entry*> e;
  return e;
}
constexpr const char* kNames[5] = {"storm", "redstorm", "title", "redstart", "red"};

bool in_bss(int module, const FieldAt& f) {
  return f.off[module] >= kBss[module].from && f.off[module] < kBss[module].to;
}

// At every start: the fields that were BSS cleared.
template <int M>
void reset() {
  for (const Entry* e : entries())
    for (int i = 0; i < e->count; ++i) {
      const FieldAt& f = e->fields[i];
      if (f.off[M] != kAbsent && in_bss(M, f)) std::memset(e->states + M * e->size + f.member, 0, size_t(f.size));
    }
}
// At the first start after a load: the fields that were initialised data,
// from the module's image (big-endian there).
template <int M>
void load() {
  for (const Entry* e : entries())
    for (int i = 0; i < e->count; ++i) {
      const FieldAt& f = e->fields[i];
      if (f.off[M] == kAbsent || in_bss(M, f)) continue;
      uint8_t* to = e->states + M * e->size + f.member;
      const uint8_t* b = state::loaded_image(f.off[M]);
      const int elem = f.elem ? f.elem : f.size;
      if (f.big || elem == 1) {
        std::memcpy(to, b, size_t(f.size));
        continue;
      }
      for (int k = 0; k < f.size; k += elem, to += elem, b += elem) {
        if (elem == 2) {
          const uint16_t v = uint16_t(b[0] << 8 | b[1]);
          std::memcpy(to, &v, 2);
        } else {
          const uint32_t v = uint32_t(b[0]) << 24 | uint32_t(b[1]) << 16 | uint32_t(b[2]) << 8 | b[3];
          std::memcpy(to, &v, 4);
        }
      }
    }
}
constexpr void (*kReset[5])() = {reset<0>, reset<1>, reset<2>, reset<3>, reset<4>};
constexpr void (*kLoad[5])() = {load<0>, load<1>, load<2>, load<3>, load<4>};
}  // namespace

bool add(const char*, void* states, size_t state_size, const FieldAt* fields, int count) {
  const bool first = entries().empty();
  Entry* e = new Entry{static_cast<uint8_t*>(states), state_size, fields, count, {}, {}};
  entries().push_back(e);
  for (int m = 0; m < 5; ++m) {
    for (int i = 0; i < count; ++i)
      if (fields[i].off[m] != kAbsent) {
        e->moved[m].push_back({fields[i].off[m], fields[i].size});
        if (fields[i].size <= 4)
          e->script[m].push_back({fields[i].off[m], fields[i].size, e->states + m * state_size + fields[i].member, 0,
                                  fields[i].big || fields[i].size == 1});
      }
    // The first registration resets and loads every entry; each one's bytes
    // are hashed.
    state::add({m, kNames[m], first ? kReset[m] : nullptr, first ? kLoad[m] : nullptr, e->states + m * state_size,
                uint32_t(state_size), e->moved[m].data(), int(e->moved[m].size()), e->script[m].data(),
                int(e->script[m].size())});
  }
  return true;
}

}  // namespace game::mem::module_state
