// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

#include "game/mem/os.hpp"

#include "game/mem/constants.hpp"

#include "amiga/gfx_draw.hpp"
#include "amiga/hle.hpp"
#include "game/platform/platform.hpp"
#include "runtime/runtime.hpp"

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

namespace game::os {

uint32_t exec_base() { return runtime::rd32(4); }

// Memory: the machine's allocator directly; AllocMem always
// clears. Chip memory (MEMF_CHIP) is in the machine's host arena (item
// 4e.1: reached through Machine::bytes).
uint32_t alloc_mem(uint32_t size, uint32_t requirements) {
  if (!size) return 0;
  return requirements & MEMF_CHIP ? runtime::g_machine->arena_alloc(size) : runtime::g_machine->alloc(size, true);
}
uint32_t alloc_data(uint32_t size) { return size ? runtime::g_machine->arena_alloc(size) : 0; }
void free_mem(uint32_t mem, uint32_t size) {
  if (mem >= amiga::Machine::ARENA_BASE) runtime::g_machine->arena_free(mem, size);
  else runtime::g_machine->free(mem, size);
}

// Tasks, signals, ports, libraries and devices: exec's code directly
// (amiga::Hle's typed functions), each taking the time its call
// took (GetMsg and WaitPort are waiting calls).
namespace {
amiga::Hle& exec() { return runtime::g_machine->hle(); }
template <typename F>
auto timed(bool waits, F body) {
  decltype(body()) result{};
  runtime::g_machine->os_call(waits, [&] { result = body(); });
  return result;
}
void timed_void(bool waits, const std::function<void()>& body) { runtime::g_machine->os_call(waits, body); }
std::string name_at(uint32_t addr) { return std::string(mem::host_text(addr)); }
}  // namespace
void put_msg_time() { timed_void(false, [] {}); }
void get_msg_time() { timed_void(true, [] {}); }
void wait_port_time() { timed_void(true, [] {}); }
void reply_msg_time() { timed_void(false, [] {}); }
void open_device_time() { timed_void(false, [] {}); }
void close_device_time() { timed_void(false, [] {}); }
void copy_mem(uint32_t src, uint32_t dst, uint32_t size) {
  if (!size) return;
  const uint8_t* from = runtime::g_machine->bytes(src, size);
  uint8_t* to = runtime::g_machine->bytes(dst, size);
  for (uint32_t i = 0; i < size; ++i) to[i] = from[i];
}

// One task runs: Forbid and Permit only take their time; public ports
// are not listed (nobody looks them up).
void forbid() { timed_void(false, [] {}); }
void permit() { timed_void(false, [] {}); }
uint32_t set_signal(uint32_t signals, uint32_t mask) { return timed(false, [&] { return exec().set_signal(signals, mask); }); }
void find_task_time() { timed_void(false, [] {}); }
ProcessStart process_start() {
  // exec's ThisTask; the task's ln_Name, tc_SPLower, tc_SPUpper; the
  // process's pr_StackSize, pr_CurrentDir, pr_CLI; the CLI's
  // cli_CommandName.
  constexpr uint32_t kThisTask = 276, kName = 10, kSpLower = 58, kSpUpper = 62, kStackSize = 132,
                     kCurrentDir = 152, kCli = 172, kCommandName = 16;
  const uint32_t task = runtime::rd32(exec_base() + kThisTask), cli = runtime::rd32(task + kCli);
  return {task,
          runtime::rd32(task + kName),
          cli,
          runtime::rd32(task + kCurrentDir),
          runtime::rd32(task + kSpLower),
          runtime::rd32(task + kSpUpper),
          runtime::rd32(task + kStackSize),
          cli ? runtime::rd32((cli << 2) + kCommandName) << 2 : 0};
}
uint32_t avail_mem(uint32_t requirements) {
  constexpr uint32_t kLargest = 0x20000;  // MEMF_LARGEST
  return requirements & kLargest ? runtime::g_machine->largest_free() : runtime::g_machine->avail();
}
uint32_t open_library(uint32_t name, uint32_t v) { return open_library(name_at(name).c_str(), v); }
uint32_t open_library(const char* name, uint32_t) { return timed(false, [&] { return exec().open_library(name); }); }
void close_library(uint32_t) { timed_void(false, [] {}); }  // libraries stay

// Files: the host file service directly (step 4; the `dos` base is not
// needed any more).
namespace {
host::Files& files() { return runtime::g_machine->files; }
}  // namespace
uint32_t open(const char* name, int32_t mode) { return files().open(name, mode); }
void close(uint32_t file) { files().close(file); }
int32_t read(uint32_t file, uint32_t buf, int32_t len) {
  return files().read(file, runtime::g_machine->bytes(buf, len > 0 ? uint32_t(len) : 0), len);
}
int32_t read(uint32_t file, uint8_t* buf, int32_t len) { return files().read(file, buf, len); }
int32_t write(uint32_t file, uint32_t buf, int32_t len) {
  return files().write(file, runtime::g_machine->bytes(buf, len > 0 ? uint32_t(len) : 0), len);
}
int32_t write(uint32_t file, const uint8_t* buf, int32_t len) { return files().write(file, buf, len); }
void delay(int32_t ticks) { platform::delay(ticks); }
uint32_t io_err() { return files().ioerr(); }
int32_t seek(uint32_t file, int32_t pos, int32_t mode) { return files().seek(file, pos, mode); }
uint32_t lock(const char* name, int32_t) { return files().lock(name); }
void unlock(uint32_t l) { files().unlock(l); }
// No current directory is kept: the files come from the data directory.
uint32_t current_dir(uint32_t) { return timed(false, [] { return uint32_t(0); }); }
uint32_t load_seg(const char* name) { return timed(false, [&] { return exec().load_seg(name); }); }
void unload_seg(uint32_t seglist) { timed_void(false, [&] { exec().unload_seg(seglist); }); }

// Drawing: graphics.library's code directly (amiga::Drawing).
namespace {
amiga::Drawing& gfx() { return runtime::g_machine->drawing; }
}  // namespace
uint32_t text_length(uint32_t rp, const char* str, int32_t n) {
  return uint32_t(gfx().text_length(rp, reinterpret_cast<const uint8_t*>(str), n));
}
void text(uint32_t rp, const char* str, int32_t n) { gfx().text(rp, reinterpret_cast<const uint8_t*>(str), n); }
void set_font(uint32_t rp, uint32_t font) { gfx().set_font(rp, font); }
void set_rast(uint32_t rp, int32_t pen) { gfx().set_rast(rp, pen); }
void move(uint32_t rp, int32_t x, int32_t y) { gfx().move(rp, x, y); }
void draw(uint32_t rp, int32_t x, int32_t y) { gfx().draw(rp, x, y); }
void wait_tof() { platform::wait_frame(); }
// The display: the screen, window, palette and pointer calls directly
// (amiga::Hle's typed display functions).
namespace {
amiga::Hle& display() { return runtime::g_machine->hle(); }
}  // namespace
void set_rgb4(uint32_t vp, int32_t n, int32_t r, int32_t g, int32_t b) {
  display().set_rgb4(vp, uint32_t(n), uint32_t(r), uint32_t(g), uint32_t(b));
}
void rect_fill(uint32_t rp, int32_t x0, int32_t y0, int32_t x1, int32_t y1) { gfx().rect_fill(rp, x0, y0, x1, y1); }
int32_t read_pixel(uint32_t rp, int32_t x, int32_t y) { return gfx().read_pixel(rp, int16_t(x), int16_t(y)); }
void set_apen(uint32_t rp, int32_t pen) { gfx().set_apen(rp, pen); }
void set_bpen(uint32_t rp, int32_t pen) { gfx().set_bpen(rp, pen); }
void set_drmd(uint32_t rp, int32_t mode) { gfx().set_drmd(rp, mode); }
void init_bitmap(uint32_t bm, int32_t depth, int32_t width, int32_t height) {
  display().init_bitmap(bm, uint32_t(depth), uint32_t(width), uint32_t(height));
}
uint32_t raster_bytes(int32_t width, int32_t height) { return ((uint32_t(width) + 15) >> 3 & ~1u) * uint32_t(height); }
uint32_t alloc_raster(int32_t width, int32_t height) { return runtime::g_machine->arena_alloc(raster_bytes(width, height)); }
void free_raster(uint32_t plane, int32_t width, int32_t height) {
  runtime::g_machine->arena_free(plane, raster_bytes(width, height));
}
void load_rgb4(uint32_t viewport, const mem_be16* colours, int32_t count) {
  std::vector<uint16_t> c(size_t(std::max(count & 0xFFFF, 0)));
  for (size_t i = 0; i < c.size(); ++i) c[i] = colours[i];
  display().load_rgb4(viewport, c.data(), int(c.size()));
}
void own_blitter() {}  // one user
void disown_blitter() {}
int32_t get_rgb4(uint32_t colormap, int32_t n) { return runtime::g_machine->hle().get_rgb4(colormap, uint32_t(n)); }
int32_t blt_bitmap(const amiga::BitMap& src, int32_t sx, int32_t sy, const amiga::BitMap& dst,
                   int32_t dx, int32_t dy, int32_t w, int32_t h, uint32_t minterm, uint32_t mask, uint32_t) {
  return gfx().blt_bitmap(src, int16_t(sx), int16_t(sy), dst, int16_t(dx), int16_t(dy), int16_t(w), int16_t(h),
                          uint8_t(minterm), uint8_t(mask));
}

void blt_mask_bitmap_rastport(const amiga::BitMap& src, int32_t sx, int32_t sy, uint32_t rp, int32_t dx,
                              int32_t dy, int32_t w, int32_t h, uint32_t minterm, uint32_t mask) {
  gfx().blt_mask_bitmap_rastport(src, int16_t(sx), int16_t(sy), rp, int16_t(dx), int16_t(dy), int16_t(w), int16_t(h),
                                 uint8_t(minterm), mask);
}

int32_t display_alert(uint32_t number, const char* text, int32_t) {
  return timed(false, [&] { return display().display_alert(number, text); });
}
void screen_to_front(uint32_t screen) { display().screen_to_front(screen); }
void set_pointer(uint32_t window, uint32_t image, int32_t height, int32_t width, int32_t x_offset,
                 int32_t y_offset) {
  const mem::be16* words = mem::constant_array<mem::be16>(image, 2 + 2 * height);
  display().set_pointer(window, image, std::vector<uint16_t>(words, words + 2 + 2 * height), height, width, x_offset,
                        y_offset);
}
uint32_t lock_ibase(uint32_t) { return 0; }  // one task: nothing to lock
void unlock_ibase(uint32_t) {}
void make_screen(uint32_t) { display().rethink_display(); }
void rethink_display() { display().rethink_display(); }
uint32_t open_screen(const amiga::NewScreen& new_screen) { return display().open_screen(new_screen); }
void close_screen(uint32_t screen) { display().close_screen(screen); }
uint32_t open_window(const amiga::NewWindow& new_window) { return display().open_window(new_window); }
void close_window(uint32_t window) { display().close_window(window); }
void show_title(uint32_t, int32_t) {}  // no title bars
uint32_t active_screen() { return display().active_screen(); }
bool pal_display() { return runtime::g_machine->pal; }
int32_t close_workbench() { return 1; }  // no Workbench
void open_workbench() {}
void activate_window(uint32_t window) { display().activate_window(window); }

}  // namespace game::os
