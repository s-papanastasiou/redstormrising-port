// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-FileCopyrightText: 2026 Ingo Ruhnke
// SPDX-License-Identifier: GPL-3.0-or-later

// Machine: the address space, the program's start, the scanline clock and
// the frame's end (picture, hashes, the host's frame).
// Forked from fa18-interceptor-port src/amiga/machine.cpp (c586ff7).
#include "amiga/machine.hpp"
#include "amiga/hle.hpp"
#include "amiga/hunk.hpp"
#include "runtime/runtime.hpp"

#include <algorithm>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <string>
#include <filesystem>

namespace amiga {

GameHooks game_hooks;

Machine::Machine() : fb_(FB_W * FB_H) {
  cache(0, uint32_t(low_.size()), low_.data());
  hle_ = std::make_unique<Hle>(*this);
}

Machine::~Machine() = default;

void Machine::log(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}

std::string Machine::cstr(uint32_t a, size_t max) {
  std::string s;
  for (size_t i = 0; i < max; ++i) {
    char ch = char(read8(a + uint32_t(i)));
    if (!ch) break;
    s += ch;
  }
  return s;
}

std::string Machine::bstr(uint32_t bptr) {
  uint32_t a = bptr << 2;
  uint8_t n = read8(a);
  std::string s;
  for (int i = 0; i < n; ++i) s += char(read8(a + 1 + uint32_t(i)));
  return s;
}

// ---- heap ------------------------------------------------------------------

uint32_t Machine::alloc(uint32_t size, bool clear) {
  size = (size + 7) & ~7u;
  if (size == 0) size = 8;
  for (auto it = free_list_.begin(); it != free_list_.end(); ++it) {
    if (it->second < size) continue;
    uint32_t a = it->first;
    uint32_t rest = it->second - size;
    free_list_.erase(it);
    if (rest) free_list_[a + size] = rest;
    if (clear) fill_range(a, size, 0);
    heap_hw_ = std::max(heap_hw_, a + size);
    return a;
  }
  return 0;
}

void Machine::free(uint32_t addr, uint32_t size) {
  if (!addr) return;
  forget_bitmaps(addr, size);
  size = (size + 7) & ~7u;
  if (size == 0) size = 8;
  auto [it, ok] = free_list_.emplace(addr, size);
  if (!ok) { log("double free %06x\n", addr); return; }
  auto next = std::next(it);
  if (next != free_list_.end() && it->first + it->second == next->first) {
    it->second += next->second;
    free_list_.erase(next);
  }
  if (it != free_list_.begin()) {
    auto prev = std::prev(it);
    if (prev->first + prev->second == it->first) {
      prev->second += it->second;
      free_list_.erase(it);
    }
  }
}

uint32_t Machine::avail() const {
  uint32_t total = 0;
  for (auto& [a, s] : free_list_) total += s;
  return total;
}

uint32_t Machine::largest_free() const {
  uint32_t best = 0;
  for (auto& [a, s] : free_list_) best = std::max(best, s);
  return best;
}

// ---- chip memory -------------------------------------------------------------

uint32_t Machine::arena_alloc(uint32_t size) {
  const uint32_t reserved = alloc(size, true);  // the place AllocMem gave it (cleared, as then)
  if (!reserved) return 0;
  size = (size + 7) & ~7u;
  if (size == 0) size = 8;
  arena_blocks_[reserved] = size;
  std::fill_n(arena_.begin() + reserved, size, uint8_t(0));
  const uint32_t a = ARENA_BASE + reserved;
  cache(a, size, &arena_[reserved]);
  return a;
}

void Machine::arena_free(uint32_t addr, uint32_t size) {
  if (!addr) return;
  const uint32_t reserved = addr - ARENA_BASE;
  const auto block = arena_blocks_.find(reserved);
  if (addr < ARENA_BASE || block == arena_blocks_.end()) {
    log("arena_free of %06x: not an arena block\n", addr);
    return;
  }
  // Its bytes go back to its place in RAM (what that memory then held).
  copy_range(reserved, arena_.data() + reserved, block->second);
  uncache(addr, block->second);
  arena_blocks_.erase(block);
  free(reserved, size);
}

BitMap& Machine::bitmap(uint32_t handle) {
  if (!handle) {
    log("bitmap(0)\n");
    std::abort();
  }
  auto it = bitmaps_.find(handle);
  if (it != bitmaps_.end()) return it->second;
  BitMap b;  // the original's layout: big-endian fields, as loaded
  const uint8_t* p = bytes(handle, kBitMapBytes);
  auto be16 = [&](int at) { return uint16_t(p[at] << 8 | p[at + 1]); };
  b.bytes_per_row = be16(0);
  b.rows = be16(2);
  b.flags = p[4];
  b.depth = p[5];
  b.pad = be16(6);
  for (int i = 0; i < 8; ++i) b.planes[i] = uint32_t(be16(8 + 4 * i)) << 16 | be16(10 + 4 * i);
  if (game_hooks.object_moved) game_hooks.object_moved(handle, kBitMapBytes, true);
  return bitmaps_.emplace(handle, b).first->second;
}

void Machine::forget_bitmaps(uint32_t addr, uint32_t size) {
  std::erase_if(bitmaps_, [&](const auto& b) {
    const bool gone = b.first >= addr && b.first < addr + size;
    if (gone && game_hooks.object_moved) game_hooks.object_moved(b.first, kBitMapBytes, false);
    return gone;
  });
  std::erase_if(rastports_, [&](const auto& r) {
    const bool gone = r.first >= addr && r.first < addr + size;
    if (gone && game_hooks.object_moved) game_hooks.object_moved(r.first, kRastPortBytes, false);
    return gone;
  });
}

RastPort& Machine::rastport(uint32_t handle) {
  if (!handle) {
    log("rastport(0)\n");
    std::abort();
  }
  auto it = rastports_.find(handle);
  if (it != rastports_.end()) return it->second;
  RastPort r;  // the original's layout: big-endian fields
  const uint8_t* p = bytes(handle, kRastPortBytes);
  auto be16 = [&](int at) { return uint16_t(p[at] << 8 | p[at + 1]); };
  auto be32 = [&](int at) { return uint32_t(be16(at)) << 16 | be16(at + 2); };
  r.bitmap = be32(4);
  r.area_ptrn = be32(8);
  r.mask = p[24];
  r.fg_pen = p[25];
  r.bg_pen = p[26];
  r.aol_pen = p[27];
  r.draw_mode = p[28];
  r.area_ptsz = int8_t(p[29]);
  r.line_ptrn = be16(34);
  r.cp_x = int16_t(be16(36));
  r.cp_y = int16_t(be16(38));
  r.font = be32(52);
  r.tx_height = be16(58);
  r.tx_width = be16(60);
  r.tx_baseline = be16(62);
  r.tx_spacing = int16_t(be16(64));
  if (game_hooks.object_moved) game_hooks.object_moved(handle, kRastPortBytes, true);
  return rastports_.emplace(handle, r).first->second;
}

void Machine::uncache(uint32_t addr, uint32_t size) {
  if (!size) return;
  for (uint32_t g = addr >> kGranuleBits; g <= (addr + size - 1) >> kGranuleBits && g < granules_.size(); ++g)
    granules_[g] = nullptr;
}

void Machine::cache(uint32_t addr, uint32_t size, uint8_t* host) {
  for (uint32_t g = (addr + kGranule - 1) >> kGranuleBits; (g + 1) << kGranuleBits <= addr + size; ++g)
    granules_[g] = host + ((g << kGranuleBits) - addr);
}

void Machine::image_add(uint32_t block, uint32_t size) {
  std::vector<uint8_t>& image = images_[block];
  image.assign(size, 0);
  uncache(block, size);
  cache(block, size, image.data());
}

void Machine::image_remove(uint32_t block) {
  const auto it = images_.find(block);
  if (it == images_.end()) return;
  uncache(block, uint32_t(it->second.size()));
  images_.erase(it);
}

uint8_t* Machine::stray(uint32_t addr, uint32_t n) {
  const uint32_t page = addr >> 12;
  if (n && ((addr + n - 1) >> 12) != page) {
    log("bytes(%06x, %u): across a page of the address space\n", addr, n);
    std::abort();
  }
  std::vector<uint8_t>& p = stray_[page];
  if (p.empty()) p.assign(4096, 0);
  // Cached when no image touches the page.
  const uint32_t lo = page << 12, hi = lo + 4096;
  auto it = images_.lower_bound(lo);
  bool touched = it != images_.end() && it->first < hi;
  if (it != images_.begin()) {
    --it;
    touched = touched || it->first + it->second.size() > lo;
  }
  if (!touched) cache(lo, 4096, p.data());
  else {
    // The granules of the page that no image touches.
    for (uint32_t g = lo; g < hi; g += kGranule) {
      auto in = images_.upper_bound(g);
      bool hit = false;
      if (in != images_.begin()) {
        --in;
        hit = g < in->first + in->second.size() && in->first < g + kGranule;
      }
      auto next = images_.lower_bound(g);
      if (next != images_.end() && next->first < g + kGranule) hit = true;
      if (!hit) granules_[g >> kGranuleBits] = &p[g - lo];
    }
  }
  return &p[addr & 4095];
}

void Machine::fill_range(uint32_t addr, uint32_t n, uint8_t v) {
  while (n) {
    const uint32_t k = std::min(n, 4096 - (addr & 4095));
    std::fill_n(bytes(addr, k), k, v);
    addr += k;
    n -= k;
  }
}

void Machine::copy_range(uint32_t addr, const uint8_t* from, uint32_t n) {
  while (n) {
    const uint32_t k = std::min(n, 4096 - (addr & 4095));
    std::copy_n(from, k, bytes(addr, k));
    addr += k;
    from += k;
    n -= k;
  }
}

uint8_t* Machine::bytes_slow(uint32_t addr, uint32_t n) {
  if (addr + n <= RAM_SIZE) {
    if (addr + n <= low_.size()) return &low_[addr];
    auto it = images_.upper_bound(addr);
    if (it != images_.begin()) {
      --it;
      const uint32_t end = it->first + uint32_t(it->second.size());
      if (addr >= it->first && addr + n <= end) return &it->second[addr - it->first];
      if (addr < end) {
        log("bytes(%06x, %u): across a module image's end\n", addr, n);
        std::abort();
      }
    }
    return stray(addr, n);
  }
  if (addr >= ARENA_BASE && addr + n <= ARENA_END) {
    // An arena address mirrors RAM: a block's bytes are in the arena, the
    // rest is the RAM at the same place (the blitter running past a block
    // reaches what the original had there).
    const uint32_t r = addr - ARENA_BASE;
    const auto next = arena_blocks_.upper_bound(r);  // the first block after r
    if (next != arena_blocks_.begin()) {
      const auto block = std::prev(next);  // the last block at or before r
      if (r < block->first + block->second) {
        if (r + n <= block->first + block->second) return &arena_[r];
        log("bytes(%06x, %u): across an arena block's end\n", addr, n);
        std::abort();
      }
    }
    if (next == arena_blocks_.end() || r + n <= next->first) return bytes(r, n);
    log("bytes(%06x, %u): across an arena block's start\n", addr, n);
    std::abort();
  }
  log("bytes(%06x, %u): neither RAM nor the arena\n", addr, n);
  std::abort();
}

// ---- boot ------------------------------------------------------------------

bool Machine::boot(const std::string& dir, const std::string& exe_name) {
  data_dir = dir;
  if (save_dir.empty()) save_dir = dir;
  files.data_dir = data_dir;
  files.save_dir = save_dir;
  files.verbose = verbose;
  free_list_.clear();
  free_list_[LOAD_BASE] = RAM_SIZE - LOAD_BASE;
  heap_hw_ = LOAD_BASE;
  // The launcher is loaded like any other segment (LoadSeg in hle.cpp);
  // its first hunk lands at LOAD_BASE + 8, the same layout as the tools.
  return hle_->boot(exe_name);
}

void Machine::pointer(int& x, int& y) const {
  x = hle_->pointer_x() + pending_mouse_x_;
  y = hle_->pointer_y() + pending_mouse_y_;
}

// ---- scheduler -------------------------------------------------------------

void Machine::run_line() {
  line_start_ += line_cycles_;
  ++total_lines_;
  audio_line();
  ++vpos_;
  if (vpos_ >= lines_per_frame()) {
    vpos_ = 0;
    end_of_frame();
    raise(INT_VERTB);
  }
  check_interrupts();
}

void Machine::idle_lines(int n) {
  for (int i = 0; i < n; ++i) {
    if (det_timing_)
      line_start_ = cpu.cycles - line_cycles_;  // run_line() adds line_cycles_
    else
      cpu.cycles = line_start_ + line_cycles_;
    run_line();
  }
}

void Machine::idle_until(const std::function<bool()>& done) {
  while (!done()) {
    if (exit_) throw QuitRequested{};
    idle_lines(1);
  }
}

void Machine::end_of_frame() {
  mouse_frame();
  render_frame();
  if (game_hooks.frame_overlay) game_hooks.frame_overlay(fb_.data());
  hle_->draw_pointer(fb_.data());
  ++frame_count_;
  if (ram_hash_every > 0 && frame_count_ % uint64_t(ram_hash_every) == 0) {
    log_ram_hash();
    // The rendered picture too (with the pointer), to check changes to the
    // display emulation (tests/compare_frames.sh).
    uint64_t h = 1469598103934665603ull;
    for (uint32_t px : fb_) h = (h ^ px) * 1099511628211ull;
    log("[fbhash] frame %llu %016llx\n", (unsigned long long)frame_count_, (unsigned long long)h);
  }
  // Debugging aid: RSR_RAM_DUMP=frame,file writes RAM (up to the heap's
  // high-water mark) at the end of that frame.
  if (const char* dump = getenv("RSR_RAM_DUMP")) {
    const char* comma = std::strchr(dump, ',');
    if (comma && uint64_t(std::strtoull(dump, nullptr, 10)) == frame_count_) {
      if (FILE* f = std::fopen(comma + 1, "wb")) {
        for (uint32_t a = 0; a < heap_hw_; a += 4096) {
          const uint32_t k = std::min<uint32_t>(4096 - (a & 4095), heap_hw_ - a);
          std::fwrite(bytes(a, k), 1, k, f);
        }
        std::fclose(f);
      }
    }
  }
  if (av_hash_every > 0) {
    for (uint32_t px : fb_) av_video_ = (av_video_ ^ px) * 1099511628211ull;
    for (int16_t s : audio_buf_) av_audio_ = (av_audio_ ^ uint16_t(s)) * 1099511628211ull;
    av_audio_ = (av_audio_ ^ audio_buf_.size()) * 1099511628211ull;  // the sample count too
    if (frame_count_ % uint64_t(av_hash_every) == 0) {
      log("[avhash] frame %llu video %016llx audio %016llx\n", (unsigned long long)frame_count_,
          (unsigned long long)av_video_, (unsigned long long)av_audio_);
      av_video_ = av_audio_ = 1469598103934665603ull;
    }
  }
  if (host_) {
    host_->frame(fb_.data());
    host_->audio(audio_buf_.data(), audio_buf_.size() / 2);
  }
  audio_buf_.clear();
}

void Machine::check_interrupts() {
  if (!(cs_.intena_ & INT_INTEN)) return;
  uint16_t pending = cs_.intena_ & cs_.intreq_ & 0x3FFF;
  if (!pending) return;
  if (hle_->interrupts_disabled()) return;
  if (pending & (INT_EXTER)) dispatch_interrupt(6);
  if (pending & (INT_AUD0 | INT_AUD1 | INT_AUD2 | INT_AUD3)) dispatch_interrupt(4);
  if (pending & (INT_COPER | INT_VERTB | INT_BLIT)) dispatch_interrupt(3);
  if (pending & (INT_PORTS)) dispatch_interrupt(2);
  if (pending & (INT_TBE | INT_DSKBLK | INT_SOFT)) dispatch_interrupt(1);
}

void Machine::dispatch_interrupt(int level) { hle_->interrupt(level); }

// The program: the launcher's start-up (the game's C++) run to its end, then the process's exit. Emulated time passes inside it,
// in the waits of the OS calls it makes.
int Machine::run() {
  try {
    // The program (RedStorm) starts at its first code, 8 bytes into its
    // segment: the game's start-up runs in its context.
    if (modules_seen_.empty() || !game_hooks.program_start) {
      log("no program loaded\n");
      return 1;
    }
    with_module(runtime::module_number(modules_seen_.front().name.c_str()), cpu.pc - 8,
                [this] { cpu.d[0] = uint32_t(game_hooks.program_start()); });
    hle_->process_exit();
  } catch (QuitRequested&) {
  }
  return exit_code_;
}

}  // namespace amiga
