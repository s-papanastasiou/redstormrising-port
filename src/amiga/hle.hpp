// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-FileCopyrightText: 2026 Ingo Ruhnke
// SPDX-License-Identifier: GPL-3.0-or-later

// The AmigaOS libraries and devices the game uses (Hle): see hle.cpp.
// Forked from fa18-interceptor-port src/amiga/hle.hpp (c586ff7).
#pragma once

#include "amiga/intuition_objects.hpp"
#include "amiga/machine.hpp"

#include <cstdio>
#include <deque>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace amiga {

// Line-A opcode $A<lib><idx>: lib 15 is reserved for host traps.
constexpr int HOST_LIB = 15;
enum { TRAP_RETURN = 0, TRAP_EXIT = 1 };

struct ReturnFromCall {};
enum Lib {
  LIB_EXEC = 0, LIB_DOS, LIB_GFX, LIB_INTUITION,
  LIB_INPUT, LIB_KEYBOARD, LIB_TIMER, LIB_GAMEPORT, LIB_AUDIO,
  LIB_RESOURCE, LIB_DISKFONT, LIB_COUNT
};

struct Hle {
  explicit Hle(Machine& m) : m(m) {}

  // Create the libraries and the launcher process, LoadSeg `exe` from the
  // data directory and prepare the CPU to enter it.
  bool boot(const std::string& exe);
  // The running process returned from its entry point (exit stub).
  void process_exit();

  void interrupt(int level);
  bool interrupts_disabled() const;

  void key_event(uint8_t rawkey, bool down);
  void mouse_move(int dx, int dy);
  void mouse_button(int button, bool down);
  // Intuition pointer position (front-screen pixels).
  int pointer_x() const { return mouse_x_; }
  int pointer_y() const { return mouse_y_; }
  // Paula channel `ch` finished a block (before reloading LC/LEN).
  void audio_block_end(int ch);
  // Overlay the Intuition sprite pointer on a finished frame.
  void draw_pointer(uint32_t* fb);

  uint32_t ret_stub = 0;
  uint32_t exit_stub = 0;

  // The display calls as typed functions:
  // the game code calls them directly;
  // the intuition and graphics vectors are glue over them.
  void rethink_display();
  uint32_t open_screen(const NewScreen& ns);
  void close_screen(uint32_t screen);
  uint32_t open_window(const NewWindow& nw);
  void close_window(uint32_t window);
  void screen_to_front(uint32_t screen);
  // `words`: the image (2 + 2 per row; its address `image` names it).
  void set_pointer(uint32_t window, uint32_t image, const std::vector<uint16_t>& words, int height, int width,
                   int x_offset, int y_offset);
  void activate_window(uint32_t window);
  void load_rgb4(uint32_t viewport, const uint16_t* colours, int count);
  void set_rgb4(uint32_t viewport, uint32_t n, uint32_t r, uint32_t g, uint32_t b);
  void init_bitmap(uint32_t bitmap, uint32_t depth, uint32_t width, uint32_t height);
  // GetRGB4 of the ColorMap `colormap` (a screen's: its handle); -1: none.
  int32_t get_rgb4(uint32_t colormap, uint32_t n);
  // The open screens and windows (intuition_objects.hpp) by handle, and
  // IntuitionBase's ActiveScreen.
  Screen& screen(uint32_t handle);
  Window& window(uint32_t handle);
  uint32_t active_screen() const { return active_screen_; }
  // The active window's input (IDCMP) as host events: what GetMsg on its
  // UserPort returned. False when there is none.
  struct InputEvent {
    uint32_t cls;
    uint16_t code, qualifier;
  };
  bool next_input(InputEvent& event);
  // An input event written to input.device (IND_WRITEEVENT) that moves the
  // pointer: IECLASS_RAWMOUSE with IEQUALIFIER_RELATIVEMOUSE (moved by x, y)
  // or IECLASS_POINTERPOS (to x, y in view coordinates); x, y in Intuition's
  // high-resolution units (two per pixel here). Intuition handles it at
  // once, as a mouse move.
  void pointer_event(uint8_t cls, uint16_t qualifier, int16_t x, int16_t y);
  // keyboard.device's KBD_READMATRIX: the keys held down, a bit per raw key.
  void read_key_matrix(uint8_t* to, uint32_t length) const;
  // audio.device: ADCMD_PERVOL, CMD_FLUSH, CMD_WRITE (true: completed at
  // once) on the channels of `mask`; whether a write has completed.
  void audio_set(uint8_t mask, uint16_t period, uint16_t volume);
  void audio_flush(uint8_t mask);
  bool audio_write(uint32_t key, uint8_t mask, uint32_t data, uint32_t length, uint16_t period, uint16_t volume,
                   uint16_t cycles, bool pervol);
  bool audio_done(uint32_t key) const;
  // AddIntServer with a host function as the server's code (it returns
  // the server's d0: 0 lets the next server run), priority `pri`. `node`
  // names it for RemIntServer.
  void add_host_server(int number, uint32_t node, int8_t pri, std::function<int32_t()> server);
  void rem_server(int number, uint32_t node);
  // exec's tasks, signals and libraries, and dos's LoadSeg, as typed
  // functions (the game's os:: calls).
  uint32_t this_task() const { return task_; }
  uint32_t find_task(const std::string& name) const;  // 0: none
  uint32_t set_signal(uint32_t signals, uint32_t mask);  // the old signals
  uint32_t open_library(const std::string& name);  // 0: none of that name
  // OpenDevice: io_Device and io_Unit set; the error (0: opened).
  int8_t open_device(const std::string& name, uint32_t unit, uint32_t io);
  // dos: LoadSeg'd files are one contiguous block laid out like
  // fa18's tools/loadseg.py (8-byte header + payload per hunk), so the
  // first segment's addresses match the disassembly tools.
  uint32_t load_seg(const std::string& amiga_name);  // a BPTR seglist; 0: failed
  void unload_seg(uint32_t seglist_bptr);
  // intuition's DisplayAlert of a one-line AlertMessage: its text is
  // logged; the answer is 0 (as for the left button).
  int32_t display_alert(uint32_t number, const char* text);

  // A screen as the display shows it
  // (what the copper list set up): from
  // `wait` on, its display window, fetch, planes and colours; from `end`
  // on, no planes.
  struct Band {
    int wait, end;
    uint16_t diwstrt, diwstop, ddfstrt, ddfstop, bplcon0;
    uint32_t planes[6];
    int planes_set;
    int16_t bpl1mod, bpl2mod;
    uint16_t colours[32];
    int colour_count;
    uint32_t dsp;  // the ViewPort's DspIns token (LoadRGB4 updates its colours)
  };
  const std::vector<Band>& bands() const { return bands_; }

private:
  Machine& m;
  uint32_t& D(int i) { return m.cpu.d[i]; }
  uint32_t& A(int i) { return m.cpu.a[i]; }

  uint32_t low_alloc(uint32_t size);
  uint32_t make_library(int lib, const char* name, int nvec, uint32_t possize);

  // exec helpers
  void new_list(uint32_t list);

  // graphics
  void build_bands();
  void poke_colors(uint32_t vp);

  std::map<uint32_t, std::pair<uint32_t, uint32_t>> segs_;  // bptr -> block, size

  // memory layout
  uint32_t low_next_ = 0x400;
  uint32_t libs_[LIB_COUNT] = {};
  uint32_t task_ = 0;
  uint32_t input_task_ = 0;
  uint32_t cli_ = 0;

  // the launcher process (tasks.cpp), task_
  struct Proc {
    uint32_t task = 0, stack = 0, stack_size = 0;
    uint32_t sig_alloc = 0xFFFF;        // low 16 reserved for the system
    uint32_t sig_recvd = 0;
  };
  std::vector<Proc> procs_;
  size_t cur_ = 0;
  Proc& cur() { return procs_[cur_]; }

  // exec state
  int disable_ = 0;
  int cur_level_ = 0;
  std::vector<uint32_t> servers_[16];  // AddIntServer chains (Interrupt*)

  // devices
  uint8_t key_matrix_[16] = {};
  uint16_t qualifier_ = 0;

  // intuition (intuition.cpp)
  void intuition_init();
  void init_rastport(uint32_t rp);
  void update_mouse_fields();
  void send_idcmp(uint32_t cls, uint16_t code);
  void rawkey_idcmp(uint8_t rawkey, bool down);
  std::vector<uint32_t> screens_;   // front screen first
  std::vector<uint32_t> windows_;
  std::map<uint32_t, Screen> screen_objs_;  // by handle
  std::map<uint32_t, Window> window_objs_;
  uint32_t active_screen_ = 0, active_window_ = 0;
  struct HostServer {
    int8_t pri;
    std::function<int32_t()> code;
  };
  std::map<uint32_t, HostServer> host_servers_;  // by node
  std::deque<InputEvent> input_events_;  // the active window's IDCMP events, oldest first
  bool read_ends_ = false;               // next_input: a press was delivered, the read ends
  uint32_t pointer_window_ = 0;
  int mouse_x_ = 0, mouse_y_ = 0;   // pointer, front-screen pixels

  // audio.device (audio_device.cpp): requests by the game's keys.
  struct AudioRequest {
    uint32_t data = 0, length = 0;
    uint16_t period = 0, volume = 0, cycles = 0;
    bool done = false;
    int8_t error = 0;  // IOERR_ABORTED: flushed
  };
  std::map<uint32_t, AudioRequest> audio_requests_;
  struct AudioChan {
    std::deque<uint32_t> queue;  // CMD_WRITE requests waiting
    uint32_t playing = 0;        // request being played
    int cycles_left = 0;         // 0: repeat until flushed
  };
  AudioChan audio_[4];
  void audio_latch(int ch, uint32_t key);
  void audio_start_next(int ch);
  bool audio_queued(uint32_t key) const;
  void audio_complete(uint32_t key, int8_t error);

  // graphics
  std::vector<Band> bands_;  // the shown view's screens, top first

  // dos
};

}  // namespace amiga
