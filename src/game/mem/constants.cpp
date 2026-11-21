// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// The modules' constant data (constants.hpp).
#include "game/mem/constants.hpp"

#include "amiga/machine.hpp"
#include "game/mem/state.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace game::mem::constants {

namespace {
struct Image {
  uint32_t block, size;
  std::vector<uint8_t> bytes;
};
std::vector<Image>& images() {
  static std::vector<Image> v;
  return v;
}
constexpr const char* kModules[5] = {"storm", "redstorm", "title", "redstart", "red"};

void unloaded(uint32_t block, uint32_t size) {
  std::erase_if(images(), [&](const Image& i) { return i.block < block + size && block < i.block + i.size; });
}

void loaded(const char* name, uint32_t block, uint32_t size) {
  unloaded(block, size);
  for (const char* m : kModules)
    if (!std::strcmp(name, m)) {
      const uint8_t* bytes = runtime::g_machine->bytes(block, size);
      images().push_back({block, size, std::vector<uint8_t>(bytes, bytes + size)});
    }
}

[[maybe_unused]] const bool registered_ = [] {
  amiga::game_hooks.image_loaded = loaded;
  amiga::game_hooks.image_unloaded = unloaded;
  return true;
}();

const Image& image_at(uint32_t addr, uint32_t size) {
  for (const Image& i : images())
    if (addr >= i.block && addr + size <= i.block + i.size) return i;
  std::fprintf(stderr, "constant %06x (%u bytes) is in no loaded module\n", addr, size);
  std::abort();
}

// The trace run: the emulated memory must still hold what the copy holds.
void check(const Image& i, uint32_t addr, uint32_t size) {
  if (!trace::on) return;
  // A constant overlapping state that moved into C++ (the original read
  // the state's current value there).
  const int32_t off = int32_t(addr - a4());
  if (state::moved(runtime::module_index(), off, size) || state::moved_address(addr, size))
    std::fprintf(stderr, "STATE MOVED: constant %06x (%u bytes) overlaps moved state\n", addr, size);
  if (std::memcmp(runtime::g_machine->bytes(addr, size), i.bytes.data() + (addr - i.block), size) != 0)
    std::fprintf(stderr, "CONST CHANGED: %06x (%u bytes) differs from the loaded image\n", addr, size);
}
}  // namespace

const uint8_t* bytes(uint32_t addr, uint32_t size) {
  const Image& i = image_at(addr, size);
  check(i, addr, size);
  return i.bytes.data() + (addr - i.block);
}

bool contains(uint32_t addr) {
  for (const Image& i : images())
    if (addr >= i.block && addr < i.block + i.size) return true;
  return false;
}

const char* text(uint32_t addr) {
  const Image& i = image_at(addr, 1);
  const char* t = reinterpret_cast<const char*>(i.bytes.data() + (addr - i.block));
  check(i, addr, uint32_t(std::strlen(t) + 1));
  return t;
}

}  // namespace game::mem::constants
