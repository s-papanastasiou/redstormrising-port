// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// The registry of the modules' native state (state.hpp).
#include "game/mem/state.hpp"

#include "amiga/machine.hpp"
#include "game/mem/globals.hpp"

#include <cctype>

#include <cstring>
#include <vector>

namespace game::mem::state {

namespace {
std::vector<Module>& modules() {
  static std::vector<Module> m;
  return m;
}
bool g_loaded[5] = {true, true, true, true, true};  // loaded since its last start
constexpr const char* kNames[5] = {"storm", "redstorm", "title", "redstart", "red"};

struct Bytes {
  const void* bytes;
  uint32_t size;
};
std::vector<Bytes>& hashed() {
  static std::vector<Bytes> b;
  return b;
}
struct AddressRange {
  uint32_t addr, size;
};
std::vector<AddressRange>& moved_addresses() {
  static std::vector<AddressRange> r;
  return r;
}

uint64_t hash(uint64_t h) {
  for (const Module& m : modules())
    for (uint32_t i = 0; i < m.size; ++i) h = (h ^ static_cast<const uint8_t*>(m.bytes)[i]) * 1099511628211ull;
  for (const Bytes& b : hashed())
    for (uint32_t i = 0; i < b.size; ++i) h = (h ^ static_cast<const uint8_t*>(b.bytes)[i]) * 1099511628211ull;
  return h;
}

// Big-endian like the memory the field replaces: a script writes the
// value, a native int of the field's size.
bool access(const char* module, int32_t off, int size, uint32_t& value, bool write) {
  for (const Module& m : modules()) {
    if (std::strcmp(m.name, module) != 0) continue;
    for (int i = 0; i < m.field_count; ++i) {
      const Field& f = m.fields[i];
      if (f.off != off || f.size != size) continue;
      if (f.big) {  // big-endian bytes, as the script sees memory
        uint8_t* b = static_cast<uint8_t*>(f.where);
        if (!write) {
          value = 0;
          for (int k = 0; k < size; ++k) value = value << 8 | b[k];
        } else {
          for (int k = size - 1, v = int(value); k >= 0; --k, v >>= 8) b[k] = uint8_t(v);
        }
        return true;
      }
      const int width = f.width ? f.width : size;
      const int shift = 8 * (width - size);
      const uint32_t mask = (size == 4 ? 0xffffffffu : (1u << (8 * size)) - 1) << shift;
      uint32_t field = width == 1 ? *static_cast<uint8_t*>(f.where)
                     : width == 2 ? *static_cast<uint16_t*>(f.where)
                                  : *static_cast<uint32_t*>(f.where);
      if (!write) {
        value = (field & mask) >> shift;
        return true;
      }
      field = (field & ~mask) | ((value << shift) & mask);
      if (width == 1) *static_cast<uint8_t*>(f.where) = uint8_t(field);
      else if (width == 2) *static_cast<uint16_t*>(f.where) = uint16_t(field);
      else *static_cast<uint32_t*>(f.where) = field;
      return true;
    }
  }
  return false;
}
}  // namespace

bool add(const Module& m) {
  modules().push_back(m);
  amiga::game_hooks.hash = hash;
  amiga::game_hooks.access = access;
  return true;
}

bool moved(int module, int32_t off, uint32_t size) {
  for (const Module& m : modules()) {
    if (m.module != module) continue;
    for (int i = 0; i < m.moved_count; ++i)
      if (off < m.moved[i].off + m.moved[i].size && m.moved[i].off < off + int32_t(size)) return true;
  }
  return false;
}

void move_address_range(uint32_t addr, uint32_t size) {
  for (const AddressRange& r : moved_addresses())
    if (r.addr == addr && r.size == size) return;
  moved_addresses().push_back({addr, size});
}

void release_address_range(uint32_t addr, uint32_t size) {
  std::erase_if(moved_addresses(), [&](const AddressRange& r) { return r.addr == addr && r.size == size; });
}

bool moved_address(uint32_t addr, uint32_t size) {
  for (const AddressRange& r : moved_addresses())
    if (addr < r.addr + r.size && r.addr < addr + size) return true;
  return false;
}

void add_hashed(const void* bytes, uint32_t size) {
  hashed().push_back({bytes, size});
  amiga::game_hooks.hash = hash;
}

void reset(int module) {
  const bool load = g_loaded[module];
  g_loaded[module] = false;
  for (const Module& m : modules()) {
    if (m.module != module) continue;
    if (m.reset) m.reset();
    if (m.load && load) m.load();
  }
}

void loaded(const char* path) {
  const char* name = path;
  for (const char* c = path; *c; ++c)
    if (*c == ':' || *c == '/') name = c + 1;
  for (int i = 0; i < 5; ++i) {
    const char* a = name;
    const char* b = kNames[i];
    while (*a && *b && std::tolower(uint8_t(*a)) == *b) ++a, ++b;
    if (!*a && !*b) g_loaded[i] = true;
  }
}

const uint8_t* loaded_image(int32_t off) { return host_bytes(uint32_t(a4() + uint32_t(off)), 1); }

}  // namespace game::mem::state
