// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-FileCopyrightText: 2026 Ingo Ruhnke
// SPDX-License-Identifier: GPL-3.0-or-later

// The Amiga the game runs on: its address space (low memory, the
// programs' images, the arena of chip memory and file data), the display,
// the blitter and the audio channels, the scanline clock and interrupts,
// and the OS calls' implementations (Hle). The game itself is C++
// (src/game); the machine runs it in the context of the program that is
// loaded and counts the time it takes, so that it behaves frame for frame
// as on an Amiga 500.
//
// Forked from fa18-interceptor-port (src/amiga/machine.hpp at c586ff7);
// its 68000 interpreter, recompiler and remaster renderers are gone.
#pragma once

#include "amiga/bitmap.hpp"
#include "amiga/rastport.hpp"
#include "amiga/blitter.hpp"
#include "amiga/cpu.hpp"
#include "amiga/gfx_draw.hpp"
#include "host/files.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace runtime { struct Def; }

namespace amiga {

constexpr uint32_t RAM_SIZE = 8 * 1024 * 1024;

// Timing. NTSC: 262 lines at 60 Hz; PAL: 312 lines at 50 Hz. A line is
// 455 CPU clocks in both (227.5 colour clocks x 2; PAL's 454 is close).
constexpr int NTSC_LINES = 262, PAL_LINES = 312;
constexpr int CYCLES_PER_LINE = 455;
constexpr double NTSC_PAULA_CLOCK = 3579545.0, PAL_PAULA_CLOCK = 3546895.0;

// Output framebuffer: lores pixels of the OS default NTSC display window
// (DIWSTRT $2C81): x=0 at beam $81, y=0 at line $2C.
constexpr int FB_W = 320;
constexpr int FB_H = 200;
constexpr int FB_HSTART = 0x81;
constexpr int FB_VSTART = 0x2C;
// PAL: the game opens its screens 10 lines lower (NewScreen TopEdge 10
// when GfxBase says PAL); the framebuffer window follows them.
constexpr int PAL_SCREEN_TOP = 10;
constexpr int VIEW_DYOFFSET = 0x2C;  // InitView default (NTSC)

struct QuitRequested {};

// Game state kept in C++ instead of the emulated memory.
// The game code sets
// these hooks: the state is part of the RAM hash, and a script's set/get
// of a module variable that moved reaches it.
struct GameHooks {
  uint64_t (*hash)(uint64_t h) = nullptr;
  // a4 offset `off` of `module`: true if it is kept natively (then read
  // into or written from `value`).
  bool (*access)(const char* module, int32_t off, int size, uint32_t& value, bool write) = nullptr;
  // Emulated address `addr` (a heap object, reached through a pointer):
  // true if its bytes are kept natively.
  bool (*access_address)(uint32_t addr, int size, uint32_t& value, bool write) = nullptr;
  // A program file was loaded at [block, block + size) (relocated and
  // patched; `name`: its file name, lower case), or unloaded.
  void (*image_loaded)(const char* name, uint32_t block, uint32_t size) = nullptr;
  void (*image_unloaded)(uint32_t block, uint32_t size) = nullptr;
  // The `size` bytes at `addr` (a BitMap, a RastPort) are now a host
  // object, or no longer.
  void (*object_moved)(uint32_t addr, uint32_t size, bool moved) = nullptr;
  // The program's start-up (the game's): run by Machine::run.
  int32_t (*program_start)() = nullptr;  // returns the exit code
};
extern GameHooks game_hooks;

// INTENA/INTREQ bits
enum : uint16_t {
  INT_TBE = 1 << 0, INT_DSKBLK = 1 << 1, INT_SOFT = 1 << 2, INT_PORTS = 1 << 3,
  INT_COPER = 1 << 4, INT_VERTB = 1 << 5, INT_BLIT = 1 << 6,
  INT_AUD0 = 1 << 7, INT_AUD1 = 1 << 8, INT_AUD2 = 1 << 9, INT_AUD3 = 1 << 10,
  INT_RBF = 1 << 11, INT_DSKSYN = 1 << 12, INT_EXTER = 1 << 13, INT_INTEN = 1 << 14,
};

struct AudioChannel {
  uint32_t lc = 0;      // AUDxLC (reload)
  uint16_t len = 0;     // AUDxLEN (reload, words)
  uint16_t per = 0;
  uint16_t vol = 0;
  uint16_t dat = 0;
  // DMA state
  bool active = false;
  uint32_t pt = 0;
  uint32_t remaining = 0;  // words left in current block
  int byte_index = 0;      // 0/1 within the current word
  int8_t sample = 0;
  double phase = 0;        // fractional Paula ticks
};

// Host side of the machine (SDL front end).
struct Host {
  virtual ~Host() = default;
  // Called once per emulated frame with the 320x200 RGB888 framebuffer.
  virtual void frame(const uint32_t* fb) = 0;
  // Audio samples (interleaved stereo, signed 16-bit) produced this frame.
  virtual void audio(const int16_t* samples, size_t frames) = 0;
  virtual int audio_rate() const { return 48000; }
};

class Machine {
public:
  Machine();
  ~Machine();

  // ---- setup ------------------------------------------------------------
  // `data_dir`: directory with the game files (the WHDLoad install's
  // data/ directory). The launcher executable is loaded and started as the
  // process "RedStormRising".
  bool boot(const std::string& data_dir, const std::string& exe_name = "RedStorm");
  void set_host(Host* h) { host_ = h; }
  // Run until the game exits or QuitRequested is thrown by the host.
  int run();

  // ---- memory helpers for HLE code -------------------------------------
  uint8_t r8(uint32_t a) { return read8(a); }
  uint16_t r16(uint32_t a) { return read16(a); }
  uint32_t r32(uint32_t a) { return read32(a); }
  void w8(uint32_t a, uint8_t v) { write8(a, v); }
  void w16(uint32_t a, uint16_t v) { write16(a, v); }
  void w32(uint32_t a, uint32_t v) { write32(a, v); }
  std::string cstr(uint32_t a, size_t max = 256);
  std::string bstr(uint32_t bptr);

  // Simple RAM allocator (first-fit, never compacts).
  uint32_t alloc(uint32_t size, bool clear = true);
  void free(uint32_t addr, uint32_t size);
  uint32_t avail() const;
  uint32_t largest_free() const;

  // The host arena: chip
  // memory (planes, samples: 4e.1) and file data (4e.3) as blocks of host
  // memory named by addresses above RAM (a bus access there is outside
  // RAM: their bytes are reached through bytes() only). A block's address
  // is ARENA_BASE + the place it reserves in the RAM heap, and an arena
  // address outside the blocks is the RAM at the same place: the layout
  // is the original's, so the original's overruns (the blitter a word past
  // a block; a blit of BLTSIZE 0 that runs 170 KB on) reach what they
  // reached. A freed block's bytes go back to its RAM place. Reserving the
  // place in the RAM heap also keeps the heap's layout, so the modules'
  // load addresses, stay the original's (Storm reads relocated longwords
  // of its own code as sea bed, area_level: they depend on where it is).
  static constexpr uint32_t ARENA_BASE = 0x800000, ARENA_END = 0x1000000;
  uint32_t arena_alloc(uint32_t size);
  void arena_free(uint32_t addr, uint32_t size);
  // The BitMap named by `handle` (bitmap.hpp): made by InitBitMap or
  // OpenScreen, or read once from the 40 bytes at `handle` (a BitMap in a
  // module's data); forget_bitmaps when its memory is given back.
  BitMap& bitmap(uint32_t handle);
  void forget_bitmaps(uint32_t addr, uint32_t size);
  // The RastPort named by `handle` (rastport.hpp): made by OpenScreen/
  // OpenWindow, or read once from its 100 bytes; forgotten with them.
  RastPort& rastport(uint32_t handle);
  // A module's image: its bytes in a host block at its load
  // address (RAM keeps the address range: the heap's layout); a bus
  // access there takes the io path and is resolved by bytes().
  void image_add(uint32_t block, uint32_t size);
  void image_remove(uint32_t block);
  // The host bytes of [addr, addr + n): RAM, a module's image or the arena
  // (else the program stops).
  uint8_t* bytes(uint32_t addr, uint32_t n) {
    if (uint8_t* g = addr < 0x1000000 ? granules_[addr >> kGranuleBits] : nullptr;
        g && (addr & (kGranule - 1)) + n <= kGranule)
      return g + (addr & (kGranule - 1));
    return bytes_slow(addr, n);
  }
  uint16_t bytes16(uint32_t addr) {
    const uint8_t* b = bytes(addr, 2);
    return uint16_t(b[0] << 8 | b[1]);
  }
  void put16(uint32_t addr, uint16_t v) {
    uint8_t* b = bytes(addr, 2);
    b[0] = uint8_t(v >> 8);
    b[1] = uint8_t(v);
  }


  // Advance emulated time without executing CPU code (used by blocking HLE
  // calls). Runs at least one scanline. Throws QuitRequested.
  void idle_until(const std::function<bool()>& done);
  void idle_lines(int n);

  // ---- input (called by host) -------------------------------------------
  void key_event(uint8_t rawkey, bool down);
  void mouse_move(int dx, int dy);
  void mouse_button(int button, bool down);  // 0 = left, 1 = right
  // The joystick in port 2 (nothing connects one: the game is played with
  // the keyboard and mouse).
  struct Joystick {
    bool up, down, left, right, fire;
  };
  Joystick joystick() const { return {}; }  // none is connected
  // Current Intuition pointer position (front-screen pixels).
  void pointer(int& x, int& y) const;


  uint64_t frame_count() const { return frame_count_; }
  uint16_t color_reg(int i) const { return dn_.color_[i & 31]; }
  // The blitter (blitter.hpp): its registers, and a blit of `size`
  // (BLTSIZE: height << 6 | words) with them, done at once.
  Blitter& blitter() { return blitter_; }
  void blit(uint16_t size);
  // Paula: channel `ch` starts playing from its pointer and length (if it
  // is not playing), or stops.
  void audio_start(int ch);
  void audio_stop(int ch) { aud_[ch].active = false; }
  AudioChannel& audio_channel(int ch) { return aud_[ch]; }

  // ---- the address space (24 bits, as the 68000's) ----------------------
  // Resolved by bytes(); an address outside RAM_SIZE (once the custom
  // chips) reads 0 and is logged.
  uint8_t read8(uint32_t addr);
  uint16_t read16(uint32_t addr);
  uint32_t read32(uint32_t addr) { return uint32_t(read16(addr)) << 16 | read16(addr + 2); }
  void write8(uint32_t addr, uint8_t v);
  void write16(uint32_t addr, uint16_t v);
  void write32(uint32_t addr, uint32_t v) {
    write16(addr, uint16_t(v >> 16));
    write16(addr + 2, uint16_t(v));
  }

  Cpu cpu;
  bool verbose = false;
  std::string data_dir;   // game files (read-only)
  std::string save_dir;   // RedStormSave: and copy-on-write files
  host::Files files;      // the dos files (data_dir, save_dir)
  Drawing drawing{*this}; // graphics.library drawing

  // Manual-protection bypass, as JOTD's WHDLoad slave: NOP RedStart code
  // offset $2d4 when that module is loaded (see README).
  bool bypass_protection = true;
  // --reference-terrain (the gate): Storm's sea bed (its own code, read as
  // terrain) as if Storm were loaded where the reference recordings had it,
  // wherever it is loaded now.
  bool reference_terrain = false;
  static constexpr uint32_t kReferenceStormBlock = 0x09e4f8;
  // Module `name`'s file as LoadSeg would load it at `block`.
  const std::vector<uint8_t>& image_as_loaded_at(const std::string& name, uint32_t block);

  // The video standard (set before boot): PAL (50 Hz, as the European
  // machines) or NTSC (60 Hz). The game times its pictures by frames and
  // its music by real time, so PAL shows them as on a European Amiga.
  bool pal = false;
  int lines_per_frame() const { return pal ? PAL_LINES : NTSC_LINES; }
  double field_rate() const { return pal ? 50.0 : 60.0; }
  double paula_clock() const { return pal ? PAL_PAULA_CLOCK : NTSC_PAULA_CLOCK; }
  int fb_vstart() const { return FB_VSTART + (pal ? PAL_SCREEN_TOP : 0); }

  // CPU speed multiplier: the CPU gets cpu_speed x 455 cycles per scanline.
  void set_cpu_speed(double s) { line_cycles_ = int64_t(CYCLES_PER_LINE * (s < 0.1 ? 0.1 : s)); }
  // Emulated time since boot in microseconds (scanline based).
  uint64_t micros() const { return uint64_t(double(total_lines_) * 1e6 / (lines_per_frame() * field_rate())); }

  void log(const char* fmt, ...);

  // ---- the game's programs ---------------------------------------------
  // Called by LoadSeg/UnLoadSeg for `module` (lower-case file name) loaded
  // as one block at `block`: a program file of another build is refused.
  void module_loaded(const std::string& module, uint32_t block, uint32_t size, uint64_t file_size,
                            const std::string& path);
  void module_unloaded(uint32_t block, uint32_t size);
  // The running module: its number (runtime::module_index()
  // order: storm, redstorm, title, redstart, red) and its load address
  // (its tools'-layout address LOAD_BASE).
  uint32_t module_block() const { return module_block_; }
  int module_number() const { return module_number_; }
  // Run `body` in module `module` loaded at `block`: a module's start-up
  // called by the loader, host code calling game code (interrupt
  // servers).
  void with_module(int module, uint32_t block, const std::function<void()>& body) {
    const uint32_t b = module_block_;
    const int m = module_number_;
    module_block_ = block;
    module_number_ = module;
    struct Restore {
      Machine& m; uint32_t b; int module;
      ~Restore() { m.module_block_ = b; m.module_number_ = module; }
    } restore{*this, b, m};
    body();
  }
  // A routine the original called through an address (the effect table,
  // a module's start-up and main), called directly: the time
  // that call took (the scanline boundaries around it, the return stub's
  // 4 cycles) around `body`.
  void call_timed(const std::function<void()>& body);
  // A host service that replaces an OS call: the same time passes as for
  // the call (one scanline first under --det-timing when it was a waiting
  // call, and the trampoline's cycles around `body`).
  void os_call(bool waits, const std::function<void()>& body);
  // Test scripts ("set", see src/main.cpp): store `value` (`size` bytes) in
  // a variable of a loaded module. `where`: a hex address in the tools'
  // layout, or a4+N / a4-N (decimal) relative to the module's small-data
  // base. Logs and returns false if the module is not loaded. With
  // `write` false it only logs the variable's value ("get").
  bool set_module_var(const std::string& module, const std::string& where, int size,
                      uint32_t value, bool write = true);
  // RSR_DATA_WRITES=FILE (debugging): which bytes of the modules' images
  // the run changed, written at the end as "module address" lines (tools'
  // layout), for planning what still lives in the memory image.
  void write_data_writes();

  // Deterministic timing for comparing runs: CPU instructions take no
  // emulated time; only waiting OS calls advance it (one scanline each).
  void set_det_timing(bool on) {
    det_timing_ = on;
    if (on) line_cycles_ = int64_t(1) << 50;
  }
  bool det_timing() const { return det_timing_; }
  // Log a hash of the game's memory every N frames ("[ramhash] ...").
  int ram_hash_every = 0;
  // --av-hash N: every frame's picture and audio samples are folded into
  // two digests, logged ("[avhash] frame F video V audio A") and restarted
  // every N frames (the reference recordings, tests/reference).
  int av_hash_every = 0;

private:
  friend struct Hle;
  // scheduler
  void run_line();
  void end_of_frame();
  void check_interrupts();
  void dispatch_interrupt(int level);
  void raise(uint16_t bits) { cs_.intreq_ |= bits; }

  // custom chips
  void blit_line(uint16_t size);
  void render_frame();
  void render_line(int line);
  void audio_advance(double paula_ticks, int16_t* out, int frames);
  void audio_line();


  // The address space below RAM_SIZE (there is no flat RAM):
  // low memory (the HLE's system structures, the boot stack) is a host
  // block; modules' images are host blocks (images_); the rest holds no
  // object any more, only what the original left there for its overruns
  // to reach (cleared memory, freed arena blocks' bytes): pages made when
  // first touched, cleared.
  std::vector<uint8_t> low_ = std::vector<uint8_t>(0x20000);  // [0, LOAD_BASE)
  std::map<uint32_t, std::vector<uint8_t>> stray_;  // by page number (4 KB)
  // A cache: the host bytes of each 256-byte granule of the address space
  // (RAM range, then the arena) that lies wholly in one place (low memory,
  // one image, a stray page no image touches, one arena block); null: look
  // it up (bytes_slow).
  static constexpr uint32_t kGranuleBits = 8, kGranule = 1u << kGranuleBits;
  std::vector<uint8_t*> granules_ = std::vector<uint8_t*>(0x1000000 >> kGranuleBits);
  uint8_t* bytes_slow(uint32_t addr, uint32_t n);
  void uncache(uint32_t addr, uint32_t size);
  void cache(uint32_t addr, uint32_t size, uint8_t* host);  // the granules wholly inside
  uint8_t* stray(uint32_t addr, uint32_t n);
  // Range operations that may span pages: clear, copy in.
  void fill_range(uint32_t addr, uint32_t n, uint8_t v);
  void copy_range(uint32_t addr, const uint8_t* from, uint32_t n);
  Host* host_ = nullptr;
  std::unique_ptr<struct Hle> hle_;

public:
  struct Hle& hle() { return *hle_; }

private:

  // time
  int64_t line_start_ = 0;   // cpu.cycles at start of current line
  int64_t line_cycles_ = CYCLES_PER_LINE;
  uint64_t av_video_ = 1469598103934665603ull, av_audio_ = 1469598103934665603ull;
  uint64_t total_lines_ = 0;
  int vpos_ = 0;
  uint64_t frame_count_ = 0;
  bool exit_ = false;
  int exit_code_ = 0;

  // Custom chip state.
  struct CustomState {
    uint16_t intena_ = 0, intreq_ = 0;  // the interrupts (INTENA, INTREQ bits)
  } cs_;
  // The display's registers as the shown screens (Hle::Band) set them,
  // kept from frame to frame (there is no copper).
  struct Denise {
    uint32_t bplpt_[6] = {};
    uint16_t color_[32] = {};
    uint16_t bplcon0_ = 0, bplcon1_ = 0, bplcon2_ = 0;
    int16_t bpl1mod_ = 0, bpl2mod_ = 0;
    uint16_t diwstrt_ = 0x2C81, diwstop_ = 0xF4C1, ddfstrt_ = 0x38, ddfstop_ = 0xD0;
  } dn_;
  bool display_dma_ = false;  // a display was made (bitplanes shown from then on)
  Blitter blitter_;
  AudioChannel aud_[4];  // Paula's channels (started and stopped by audio.device, no DMACON)
  // display
  std::vector<uint32_t> fb_;
  // audio
  std::vector<int16_t> audio_buf_;
  double audio_frac_ = 0;
  double lp_l_ = 0, lp_r_ = 0, lp_k_ = 0.4;
  // input
  int pending_mouse_x_ = 0, pending_mouse_y_ = 0;
  void mouse_frame();

  void log_ram_hash();
  void trampoline(const std::function<void()>& body);
  uint32_t module_block_ = 0;
  int module_number_ = 0;
  bool det_timing_ = false;
  struct LoadedModule {
    std::string name;
    std::string path;  // its file
    uint32_t block, size;
    bool loaded = true;
    std::vector<uint8_t> image;  // RSR_DATA_WRITES: the block as loaded
  };
  // RSR_DATA_WRITES: per module, the tools'-layout addresses whose bytes
  // differed from the loaded image when the module went or the run ended.
  std::map<std::string, std::set<uint32_t>> data_writes_;
  std::map<std::pair<std::string, uint32_t>, std::vector<uint8_t>> images_as_loaded_;
  void note_data_writes(const LoadedModule& l);
  std::vector<LoadedModule> modules_seen_;

  // heap
  std::map<uint32_t, uint32_t> free_list_;  // addr -> size
  std::vector<uint8_t> arena_ = std::vector<uint8_t>(ARENA_END - ARENA_BASE);  // the arena's bytes
  std::map<uint32_t, uint32_t> arena_blocks_;  // reserved RAM place -> size
  std::map<uint32_t, std::vector<uint8_t>> images_;  // modules' images by block
  std::map<uint32_t, BitMap> bitmaps_;  // by handle
  std::map<uint32_t, RastPort> rastports_;  // by handle  // allocated blocks by address
  uint32_t heap_hw_ = 0;                    // highest heap address handed out
};

}  // namespace amiga
