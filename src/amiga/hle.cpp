// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-FileCopyrightText: 2026 Ingo Ruhnke
// SPDX-License-Identifier: GPL-3.0-or-later

// The AmigaOS calls the game makes, as typed functions: exec (tasks,
// signals, libraries, interrupt servers), dos (LoadSeg), graphics
// (palettes, the screen bands), and the keyboard and timer devices.
// Intuition is intuition.cpp, drawing gfx_draw.cpp, audio.device
// audio_device.cpp. Forked from fa18-interceptor-port src/amiga/hle.cpp
// (c586ff7): boot creates the "RedStormRising" process.
#include "amiga/hle.hpp"
#include "amiga/hunk.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace amiga {

namespace {
// Exec structure offsets
constexpr uint32_t LN_SUCC = 0, LN_PRED = 4, LN_TYPE = 8, LN_PRI = 9, LN_NAME = 10;
constexpr uint32_t LH_HEAD = 0, LH_TAIL = 4, LH_TAILPRED = 8;
constexpr uint32_t MP_FLAGS = 14, MP_SIGBIT = 15, MP_SIGTASK = 16, MP_MSGLIST = 20;
constexpr uint32_t MN_REPLYPORT = 14;
constexpr uint32_t IO_DEVICE = 20, IO_UNIT = 24, IO_COMMAND = 28, IO_FLAGS = 30, IO_ERROR = 31;
constexpr uint32_t IO_ACTUAL = 32, IO_LENGTH = 36, IO_DATA = 40;
constexpr uint8_t NT_TASK = 1, NT_INTERRUPT = 2, NT_DEVICE = 3, NT_MSGPORT = 4, NT_MESSAGE = 5,
                  NT_REPLYMSG = 7, NT_LIBRARY = 9, NT_PROCESS = 13;
constexpr uint8_t IOF_QUICK = 1;
constexpr uint16_t CMD_READ = 2, CMD_WRITE = 3, CMD_UPDATE = 4, CMD_CLEAR = 5, CMD_STOP = 6,
                   CMD_START = 7, CMD_FLUSH = 8;
constexpr int8_t IOERR_OPENFAIL = -1, IOERR_ABORTED = -2, IOERR_NOCMD = -3;

constexpr uint32_t LOW_TOP = 0xF000;    // the system structures below this (the old supervisor stack's top)
constexpr uint32_t USP_TOP = 0x1FFF0;
constexpr uint32_t STACK_SIZE = 0x10000 - 0x10;

}  // namespace

// ---- setup ------------------------------------------------------------------

uint32_t Hle::low_alloc(uint32_t size) {
  uint32_t a = (low_next_ + 3) & ~3u;
  low_next_ = a + size;
  if (low_next_ > LOW_TOP - 0x1000) m.log("low memory exhausted\n");
  std::memset(m.bytes(a, size), 0, size);
  return a;
}

uint32_t Hle::make_library(int lib, const char* name, int nvec, uint32_t possize) {
  uint32_t neg = uint32_t(nvec) * 6;
  uint32_t block = low_alloc(neg + possize + 64);
  uint32_t base = block + neg;
  for (int i = 1; i <= nvec; ++i) {
    uint32_t v = base - uint32_t(i) * 6;
    m.w16(v, uint16_t(0xA000 | (lib << 8) | i));
    m.w16(v + 2, 0x4E75);  // rts
    m.w16(v + 4, 0x4E71);
  }
  uint32_t name_addr = base + possize;
  for (size_t i = 0; name[i]; ++i) m.w8(name_addr + uint32_t(i), uint8_t(name[i]));
  m.w8(base + LN_TYPE, lib >= LIB_INPUT && lib <= LIB_AUDIO ? NT_DEVICE : NT_LIBRARY);
  m.w32(base + LN_NAME, name_addr);
  m.w16(base + 16, uint16_t(neg));
  m.w16(base + 18, uint16_t(possize));
  m.w16(base + 20, 34);  // version (Kickstart 1.3)
  m.w16(base + 22, 5);
  m.w32(base + 24, name_addr);
  libs_[lib] = base;
  return base;
}

bool Hle::boot(const std::string& exe_name) {
  // Host trap stubs
  ret_stub = low_alloc(4);
  m.w16(ret_stub, uint16_t(0xA000 | (HOST_LIB << 8) | TRAP_RETURN));
  exit_stub = low_alloc(4);
  m.w16(exit_stub, uint16_t(0xA000 | (HOST_LIB << 8) | TRAP_EXIT));

  uint32_t exec = make_library(LIB_EXEC, "exec.library", 130, 640);
  m.w32(4, exec);
  m.w8(exec + 530, m.pal ? 50 : 60);  // VBlankFrequency
  m.w8(exec + 531, m.pal ? 50 : 60);  // PowerSupplyFrequency
  m.w16(exec + 296, 0);  // AttnFlags: plain 68000
  make_library(LIB_DOS, "dos.library", 40, 64);
  uint32_t gfx = make_library(LIB_GFX, "graphics.library", 110, 0x200);
  m.w16(gfx + 206, m.pal ? 4 : 1);  // DisplayFlags: PAL / NTSC
  m.w16(gfx + 212, uint16_t(m.lines_per_frame() - 1));  // MaxDisplayRow
  m.w16(gfx + 214, 0x1C7);
  m.w16(gfx + 216, m.pal ? 256 : 200);  // NormalDisplayRows
  m.w16(gfx + 218, 640);  // NormalDisplayColumns
  m.w16(gfx + 232, m.pal ? 64 : 63);  // MicrosPerLine
  m.w32(gfx + 42, 0xBFE001);  // cia
  m.w32(gfx + 46, 0xDFF000);  // blitter
  make_library(LIB_INTUITION, "intuition.library", 70, 0x60);
  make_library(LIB_INPUT, "input.device", 8, 64);
  make_library(LIB_KEYBOARD, "keyboard.device", 8, 64);
  make_library(LIB_TIMER, "timer.device", 12, 64);
  make_library(LIB_GAMEPORT, "gameport.device", 8, 64);
  make_library(LIB_AUDIO, "audio.device", 8, 64);
  make_library(LIB_RESOURCE, "generic.resource", 8, 64);
  // Opened by the game but never called: it LoadSegs its fonts itself.
  make_library(LIB_DISKFONT, "diskfont.library", 8, 64);
  intuition_init();

  uint32_t seglist_bptr = load_seg(exe_name);
  if (!seglist_bptr) {
    m.log("cannot load %s from %s\n", exe_name.c_str(), m.data_dir.c_str());
    return false;
  }
  uint32_t entry = (seglist_bptr << 2) + 4;

  procs_.assign(1, Proc{});
  // Process structure, named after the command as AmigaDOS does. RedStorm
  // refuses to start if a task "RedStormRising" exists, then renames itself
  // to that name (ln_Name), which is how the modules find it.
  task_ = low_alloc(256);
  uint32_t tname = low_alloc(32);
  const char* nm = "RedStorm";
  for (size_t i = 0; nm[i]; ++i) m.w8(tname + uint32_t(i), uint8_t(nm[i]));
  m.w8(task_ + LN_TYPE, NT_PROCESS);
  m.w32(task_ + LN_NAME, tname);
  m.w8(task_ + 15, 2);  // TS_RUN
  procs_[0].task = task_;
  m.w32(task_ + 18, cur().sig_alloc);
  m.w32(task_ + 58, USP_TOP - STACK_SIZE);
  m.w32(task_ + 62, USP_TOP);
  // pr_MsgPort
  uint32_t port = task_ + 92;
  m.w8(port + LN_TYPE, NT_MSGPORT);
  m.w8(port + MP_SIGBIT, 8);
  m.w32(port + MP_SIGTASK, task_);
  new_list(port + MP_MSGLIST);
  m.w32(task_ + 128, seglist_bptr);
  m.w32(task_ + 132, STACK_SIZE);
  m.w32(task_ + 152, 0);  // pr_CurrentDir
  // CLI structure so the startup code takes the CLI path
  cli_ = low_alloc(64);
  uint32_t cmdname = low_alloc(40);
  m.w8(cmdname, uint8_t(std::strlen(nm)));
  for (size_t i = 0; nm[i]; ++i) m.w8(cmdname + 1 + uint32_t(i), uint8_t(nm[i]));
  m.w32(cli_ + 16, cmdname >> 2);
  m.w32(cli_ + 60, seglist_bptr);
  m.w32(task_ + 172, cli_ >> 2);
  m.w32(exec + 276, task_);  // ThisTask

  input_task_ = low_alloc(96);
  uint32_t iname = low_alloc(16);
  const char* in = "input.device";
  for (size_t i = 0; in[i]; ++i) m.w8(iname + uint32_t(i), uint8_t(in[i]));
  m.w8(input_task_ + LN_TYPE, NT_TASK);
  m.w8(input_task_ + LN_PRI, 20);
  m.w32(input_task_ + LN_NAME, iname);

  // Command line arguments: "\n"
  uint32_t args = low_alloc(4);
  m.w8(args, '\n');

  // The entry's state: the process stack, the command line.
  auto& c = m.cpu;
  c.a[7] = USP_TOP;
  c.a[7] -= 4;
  m.w32(c.a[7], STACK_SIZE);
  c.a[7] -= 4;
  m.w32(c.a[7], exit_stub);
  c.d[0] = 1;
  c.a[0] = args;
  c.pc = entry;
  c.imask = 0;
  return true;
}

// ---- exec lists / messages ------------------------------------------------

void Hle::new_list(uint32_t l) {
  m.w32(l + LH_HEAD, l + LH_TAIL);
  m.w32(l + LH_TAIL, 0);
  m.w32(l + LH_TAILPRED, l + LH_HEAD);
}

// ---- interrupts --------------------------------------------------------------

bool Hle::interrupts_disabled() const { return disable_ > 0 || m.cpu.imask >= 3; }

void Hle::add_host_server(int n, uint32_t node, int8_t pri, std::function<int32_t()> server) {
  host_servers_[node] = HostServer{pri, std::move(server)};
  auto& v = servers_[n];
  auto it = v.begin();
  while (it != v.end() && host_servers_.at(*it).pri >= pri) ++it;
  v.insert(it, node);
  m.cs_.intena_ |= uint16_t((1 << n) | INT_INTEN);
}

void Hle::rem_server(int n, uint32_t node) {
  auto& v = servers_[n];
  v.erase(std::remove(v.begin(), v.end(), node), v.end());
  host_servers_.erase(node);
}

void Hle::interrupt(int level) {
  if (level <= cur_level_) return;
  int saved = cur_level_;
  cur_level_ = level;
  uint16_t pending = m.cs_.intena_ & m.cs_.intreq_;
  for (int bit = 0; bit < 14; ++bit) {
    static const int lvl[14] = {1, 1, 1, 2, 3, 3, 3, 4, 4, 4, 4, 5, 5, 6};
    if (lvl[bit] != level || !(pending & (1 << bit))) continue;
    m.cs_.intreq_ &= uint16_t(~(1 << bit));
    for (uint32_t is : std::vector<uint32_t>(servers_[bit]))
      if (host_servers_.at(is).code() != 0) break;  // as the Z flag of d0
  }
  cur_level_ = saved;
}

// ---- input --------------------------------------------------------------------

void Hle::read_key_matrix(uint8_t* to, uint32_t length) const {
  for (uint32_t i = 0; i < length && i < 16; ++i) to[i] = key_matrix_[i];
}

void Hle::key_event(uint8_t rawkey, bool down) {
  if (rawkey < 0x80) {
    if (down) key_matrix_[rawkey >> 3] |= uint8_t(1 << (rawkey & 7));
    else key_matrix_[rawkey >> 3] &= uint8_t(~(1 << (rawkey & 7)));
  }
  static const struct { uint8_t key; uint16_t q; } quals[] = {
      {0x60, 1}, {0x61, 2}, {0x63, 8}, {0x64, 0x10}, {0x65, 0x20}, {0x66, 0x40}, {0x67, 0x80}};
  for (auto& q : quals)
    if (q.key == rawkey) { if (down) qualifier_ |= q.q; else qualifier_ &= uint16_t(~q.q); }
  rawkey_idcmp(rawkey, down);
}

// ---- IO requests ----------------------------------------------------------

// ---- exec -----------------------------------------------------------------------

uint32_t Hle::find_task(const std::string& name) const {
  uint32_t task = name == "input.device" ? input_task_ : 0;
  for (auto& p : procs_)
    if (name == m.cstr(m.r32(p.task + LN_NAME))) task = p.task;
  return task;
}

uint32_t Hle::set_signal(uint32_t signals, uint32_t mask) {
  const uint32_t old = cur().sig_recvd;
  cur().sig_recvd = (old & ~mask) | (signals & mask);
  return old;
}


uint32_t Hle::open_library(const std::string& name) {
  uint32_t lib = 0;
  for (int i = 0; i < LIB_COUNT; ++i)
    if (name == m.cstr(m.r32(libs_[i] + LN_NAME))) lib = libs_[i];
  if (!lib) m.log("OpenLibrary(%s) failed\n", name.c_str());
  return lib;
}

int8_t Hle::open_device(const std::string& name, uint32_t unit, uint32_t io) {
  int lib = -1;
  for (int i = LIB_INPUT; i <= LIB_AUDIO; ++i)
    if (name == m.cstr(m.r32(libs_[i] + LN_NAME))) lib = i;
  if (lib < 0) {
    m.log("OpenDevice(%s) failed\n", name.c_str());
    m.w8(io + IO_ERROR, uint8_t(IOERR_OPENFAIL));
    return IOERR_OPENFAIL;
  }
  m.w32(io + IO_DEVICE, libs_[lib]);
  m.w32(io + IO_UNIT, unit);
  m.w8(io + IO_ERROR, 0);
  if (lib == LIB_AUDIO) {
    // Allocation at open: grant the first requested channel combination.
    uint32_t data = m.r32(io + 34), len = m.r32(io + 38);
    uint8_t combo = len ? m.r8(data) : 0;
    m.w32(io + IO_UNIT, combo);
    m.w16(io + 32, 1);  // ioa_AllocKey
  }
  return 0;
}


// ---- dos ----------------------------------------------------------------------

uint32_t Hle::load_seg(const std::string& name) {
  std::string path = m.files.host_path(name, false);
  HunkFile exe;
  if (path.empty() || !exe.load(path)) {
    if (m.verbose) m.log("LoadSeg(%s) failed (%s)\n", name.c_str(), path.c_str());
    m.files.set_ioerr(205);
    return 0;
  }
  uint32_t total = 0;
  for (auto& h : exe.hunks) total += 8 + h.alloc_size;
  uint32_t block = m.alloc(total, true);
  if (!block) { m.files.set_ioerr(103); return 0; }  // ERROR_NO_FREE_STORE
  m.image_add(block, total);  // its bytes in a host block
  const std::vector<uint8_t> image = exe.image_at(block);
  std::memcpy(m.bytes(block, total), image.data(), total);
  const uint32_t code = block + 8;
  uint32_t bptr = (code - 4) >> 2;
  segs_[bptr] = {block, total};
  m.log("LoadSeg(%s) = %06x (%zu hunks, code at %06x)\n", name.c_str(), bptr << 2,
        exe.hunks.size(), code);
  std::string base_name = name.substr(name.find_last_of(":/") + 1);
  std::transform(base_name.begin(), base_name.end(), base_name.begin(), ::tolower);
  std::error_code ec;
  const uint64_t file_size = std::filesystem::file_size(path, ec);
  m.module_loaded(base_name, block, total, ec ? 0 : file_size, path);
  // Manual protection, as JOTD's slave: NOP RedStart code offset $2d4.
  if (m.bypass_protection && base_name == "redstart") {
    m.w16(code + 0x2d4, 0x4E71);
    m.log("RedStart: manual protection bypassed\n");
  }
  if (game_hooks.image_loaded) game_hooks.image_loaded(base_name.c_str(), block, total);
  return bptr;
}

void Hle::unload_seg(uint32_t bptr) {
  auto it = segs_.find(bptr);
  if (it == segs_.end()) { m.log("UnLoadSeg(%06x): unknown seglist\n", bptr << 2); return; }
  m.module_unloaded(it->second.first, it->second.second);
  m.drawing.forget_fonts(it->second.first, it->second.second);
  if (game_hooks.image_unloaded) game_hooks.image_unloaded(it->second.first, it->second.second);
  m.image_remove(it->second.first);
  m.free(it->second.first, it->second.second);
  segs_.erase(it);
}

// ---- graphics -----------------------------------------------------------------

void Hle::load_rgb4(uint32_t vp, const uint16_t* colours, int count) {
  ColorMap& cm = screen(vp - kScreenViewPort).viewport.colormap;
  count = std::min<int>(count & 0xFFFF, cm.count);
  for (int i = 0; i < count; ++i) cm.table[i] = colours[i] & 0xFFF;
  poke_colors(vp);
}

void Hle::set_rgb4(uint32_t vp, uint32_t n, uint32_t r, uint32_t g, uint32_t b) {
  ColorMap& cm = screen(vp - kScreenViewPort).viewport.colormap;
  if (n < cm.count) {
    cm.table[n] = uint16_t((r & 15) << 8 | (g & 15) << 4 | (b & 15));
    poke_colors(vp);
  }
}

void Hle::init_bitmap(uint32_t bm, uint32_t depth, uint32_t width, uint32_t height) {
  BitMap& b = m.bitmap(bm);  // (the planes stay as they were)
  b.bytes_per_row = uint16_t(((width + 15) >> 4) * 2);
  b.rows = uint16_t(height);
  b.flags = 0;
  b.depth = uint8_t(depth);
  b.pad = 0;
}

void Hle::build_bands() {
  // The shown screens as MrgCop's copper list set them up: the visible
  // ViewPorts (front screen first, then sorted by their position), each
  // from the line before it. (The RasInfos have no second playfield.)
  bands_.clear();
  const int vdy = VIEW_DYOFFSET, vdx = FB_HSTART;  // the View's offsets
  std::vector<const Screen*> shown;
  for (uint32_t s : screens_)
    if (shown.size() < 16 && !(screen(s).viewport.modes & 0x2000)) shown.push_back(&screen(s));
  std::sort(shown.begin(), shown.end(),
            [](const Screen* a, const Screen* b) { return a->viewport.dyoffset < b->viewport.dyoffset; });
  auto clamp_line = [](int line) { return std::clamp(line, 0, 255); };  // the copper's 8-bit waits
  int last_end = 0;
  for (const Screen* sc : shown) {
    const ViewPort& vp = sc->viewport;
    if (!vp.rasinfo_handle || !vp.bitmap) continue;
    const BitMap& bitmap = m.bitmap(vp.bitmap);
    const bool hires = vp.modes & 0x8000;
    const int depth = bitmap.depth;
    const int dw = vp.dwidth, dh = vp.dheight;
    const int vstart = vdy + vp.dyoffset;
    const int hstart = vdx + vp.dxoffset;
    const int lores_w = hires ? dw / 2 : dw;
    Band band{};
    band.wait = clamp_line(std::max(last_end, vstart - 1));
    band.bplcon0 = uint16_t((depth & 7) << 12 | 0x0200 | (vp.modes & (0x8000 | 0x0800 | 0x0400 | 0x0004)));
    const int ddfstrt = hires ? ((hstart - 9) / 2) & 0xFC : ((hstart - 17) / 2) & 0xFC;
    const int nwords = (dw + 15) / 16;
    band.ddfstrt = uint16_t(ddfstrt);
    band.ddfstop = uint16_t(hires ? ddfstrt + (nwords - 2) * 4 : ddfstrt + (nwords - 1) * 8);
    band.diwstrt = uint16_t(((vstart & 0xFF) << 8) | (hstart & 0xFF));
    band.diwstop = uint16_t((((vstart + dh) & 0xFF) << 8) | ((hstart + lores_w) & 0xFF));
    const int bpr = bitmap.bytes_per_row;
    for (int i = 0; i < depth && i <= 5; ++i) {
      band.planes[i] = bitmap.planes[i] + uint32_t(vp.ry * bpr + (vp.rx >> 4) * 2);
      band.planes_set |= 1 << i;
    }
    band.bpl1mod = int16_t(bpr - nwords * 2);
    band.bpl2mod = int16_t(bpr - nwords * 2);
    band.colour_count = std::min<int>(vp.colormap.count, 32);
    for (int i = 0; i < band.colour_count; ++i) band.colours[i] = vp.colormap.table[i];
    band.dsp = vp.dsp;
    last_end = vstart + dh;
    band.end = clamp_line(last_end);
    bands_.push_back(band);
  }
}

namespace {
// The Screen whose ViewPort is at `vp`.
uint32_t screen_of_viewport(uint32_t vp) { return vp - kScreenViewPort; }
}  // namespace

void Hle::poke_colors(uint32_t vp) {
  // Like LoadRGB4 in Kickstart 1.3: the colours of the shown screen whose
  // ViewPort has this DspIns are updated.
  const ViewPort& v = screen(screen_of_viewport(vp)).viewport;
  for (Band& b : bands_)
    if (b.dsp == v.dsp)
      for (int i = 0; i < b.colour_count; ++i) b.colours[i] = v.colormap.table[i];
}

int32_t Hle::get_rgb4(uint32_t colormap, uint32_t n) {
  for (const auto& [h, sc] : screen_objs_)
    if (sc.viewport.colormap_handle == colormap)
      return n < sc.viewport.colormap.count ? sc.viewport.colormap.table[n] : -1;
  return -1;
}

}  // namespace amiga
