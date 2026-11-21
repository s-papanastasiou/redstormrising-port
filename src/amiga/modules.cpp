// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-FileCopyrightText: 2026 Ingo Ruhnke
// SPDX-License-Identifier: GPL-3.0-or-later

// The programs as the machine sees them: checking a loaded program file,
// the record of loaded modules (for the test scripts' set/get of a
// variable by its original offset, and RSR_DATA_WRITES), the time an OS
// call takes (os_call, call_timed), and the memory hashes that compare
// runs.
#include "amiga/hle.hpp"
#include "amiga/hunk.hpp"
#include "amiga/machine.hpp"
#include "runtime/runtime.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace amiga {


void Machine::module_loaded(const std::string& module, uint32_t block, uint32_t size,
                                   uint64_t file_size, const std::string& path) {
  runtime::g_machine = this;
  modules_seen_.push_back({module, path, block, size, true, {}});
  if (getenv("RSR_DATA_WRITES")) { const uint8_t* b = bytes(block, size); modules_seen_.back().image.assign(b, b + size); }
  // The port was made for these exact program files (fonts are not checked).
  if (const uint32_t expected = runtime::expected_size(module.c_str()); expected && file_size != expected) {
    log("%s is not the expected build (%llu bytes): this port runs only the WHDLoad data/ files\n",
        module.c_str(), (unsigned long long)file_size);
    std::exit(1);
  }
}

const std::vector<uint8_t>& Machine::image_as_loaded_at(const std::string& name, uint32_t block) {
  auto key = std::make_pair(name, block);
  auto it = images_as_loaded_.find(key);
  if (it != images_as_loaded_.end()) return it->second;
  for (const LoadedModule& l : modules_seen_) {
    if (l.name != name) continue;
    HunkFile f;
    if (!f.load(l.path)) break;
    return images_as_loaded_[key] = f.image_at(block);
  }
  log("image_as_loaded_at(%s): not loaded\n", name.c_str());
  std::exit(1);
}

void Machine::note_data_writes(const LoadedModule& l) {
  if (l.image.empty()) return;
  std::set<uint32_t>& w = data_writes_[l.name];
  for (uint32_t i = 0; i < l.size; ++i)
    if (*bytes(l.block + i, 1) != l.image[i]) w.insert(LOAD_BASE + i);
}

void Machine::write_data_writes() {
  const char* path = getenv("RSR_DATA_WRITES");
  if (!path) return;
  for (const LoadedModule& l : modules_seen_)
    if (l.loaded) note_data_writes(l);
  FILE* f = std::fopen(path, "w");
  if (!f) return;
  for (auto& [name, addrs] : data_writes_)
    for (uint32_t a : addrs) std::fprintf(f, "%s %06x\n", name.c_str(), a);
  std::fclose(f);
}

void Machine::module_unloaded(uint32_t block, uint32_t) {
  for (LoadedModule& l : modules_seen_)
    if (l.block == block && l.loaded) {
      note_data_writes(l);
      l.loaded = false;
    }
}

bool Machine::set_module_var(const std::string& module, const std::string& where, int size,
                             uint32_t value, bool write) {
  for (auto it = modules_seen_.rbegin(); it != modules_seen_.rend(); ++it) {
    if (!it->loaded || it->name != module) continue;
    uint32_t a;
    // "*a4+N+M": the long at a4+N is a pointer (into the heap); M is added.
    const bool deref = !where.empty() && where[0] == '*';
    const char* w = where.c_str() + (deref ? 1 : 0);
    if (std::strncmp(w, "a4", 2) == 0 && !deref && game_hooks.access) {
      uint32_t v = value;
      if (game_hooks.access(module.c_str(), int32_t(std::strtol(w + 2, nullptr, 10)), size, v, write)) {
        log("[script] %s %s %s (native) = %u\n", write ? "set" : "get", module.c_str(), where.c_str(), v);
        return true;
      }
    }
    if (std::strncmp(w, "a4", 2) == 0) {
      // Every module starts with `lea data+32766,a4` at $20010.
      const uint32_t lea = it->block + 0x10;
      if (read16(lea) != 0x49F9) break;
      char* end = nullptr;
      const int32_t off = int32_t(std::strtol(w + 2, &end, 10));
      a = read32(lea + 2) + uint32_t(off);
      if (deref) {
        // The pointer itself may be kept natively.
        uint32_t p = 0;
        if (!(game_hooks.access && game_hooks.access(module.c_str(), off, 4, p, false))) p = read32(a);
        a = p + uint32_t(std::strtol(end, nullptr, 10));
      }
    } else {
      a = it->block + (uint32_t(std::strtoul(w, nullptr, 16)) - LOAD_BASE);
    }
    if (!deref && (a < it->block || a + uint32_t(size) > it->block + it->size)) break;
    if (deref && game_hooks.access_address) {
      uint32_t v = value;
      if (game_hooks.access_address(a, size, v, write)) {
        log("[script] %s %s %s (%06x, native) = %u\n", write ? "set" : "get", module.c_str(), where.c_str(), a, v);
        return true;
      }
    }
    if (!write) {
      log("[script] get %s %s (%06x) = %u\n", module.c_str(), where.c_str(), a,
          size == 1 ? read8(a) : size == 2 ? read16(a) : read32(a));
      return true;
    }
    if (size == 1) write8(a, uint8_t(value));
    else if (size == 2) write16(a, uint16_t(value));
    else write32(a, value);
    log("[script] set %s %s (%06x) = %u\n", module.c_str(), where.c_str(), a, value);
    return true;
  }
  log("[script] set %s %s: module not loaded or address outside it\n", module.c_str(),
      where.c_str());
  return false;
}

void Machine::call_timed(const std::function<void()>& body) {
  auto boundary = [this] {
    while (cpu.cycles >= line_start_ + line_cycles_) {
      if (exit_) throw QuitRequested{};
      run_line();
    }
  };
  cpu.stopped = false;
  boundary();
  body();
  boundary();
  cpu.cycles += 4;  // the return stub's fetch
}


void Machine::os_call(bool waits, const std::function<void()>& body) {
  trampoline([&] {
    if (waits && det_timing_) idle_lines(1);  // as Hle::call does for waiting calls
    body();
  });
}

void Machine::trampoline(const std::function<void()>& body) {
  auto boundary = [this] {
    while (cpu.cycles >= line_start_ + line_cycles_) {
      if (exit_) throw QuitRequested{};
      run_line();
    }
  };
  boundary();
  cpu.cycles += 4;     // the trap's fetch
  body();
  boundary();
  cpu.cycles += 4;     // rts
  boundary();
  cpu.cycles += 4;     // the return stub's fetch
}

void Machine::log_ram_hash() {
  // Low memory, the touched pages of the address space, the modules'
  // images, the arena's blocks (the process stacks hold nothing: no code
  // uses them).
  uint64_t h = 1469598103934665603ull;
  for (uint8_t b : low_) h = (h ^ b) * 1099511628211ull;
  for (const auto& [page, bytes] : stray_)
    for (uint8_t b : bytes) h = (h ^ b) * 1099511628211ull;
  for (const auto& [block, image] : images_)
    for (uint8_t b : image) h = (h ^ b) * 1099511628211ull;
  for (const auto& [at, size] : arena_blocks_)
    for (uint32_t a = at; a < at + size; ++a) h = (h ^ arena_[a]) * 1099511628211ull;
  if (game_hooks.hash) h = game_hooks.hash(h);
  log("[ramhash] frame %llu %016llx\n", (unsigned long long)frame_count_, (unsigned long long)h);
}

}  // namespace amiga
