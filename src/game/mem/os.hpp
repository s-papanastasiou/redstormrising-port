// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// AmigaOS calls for the game code: typed
// functions with the libraries' arguments. Each runs the HLE's (or the
// host's) code for the call directly and takes the time the call took.
#pragma once

#include <cstdint>

#include "amiga/gfx_draw.hpp"
#include "amiga/intuition_objects.hpp"
#include "game/mem/types.hpp"

namespace amiga { struct BitMap; }
namespace game::os {
using mem_be16 = mem::be16;

uint32_t exec_base();

// exec.library
uint32_t alloc_mem(uint32_t size, uint32_t requirements);
// Memory for file data (preloaded files, offset tables, the file buffer):
// AllocMem's place in the heap, the bytes in the host arena.
uint32_t alloc_data(uint32_t size);
void free_mem(uint32_t memory, uint32_t size);
void copy_mem(uint32_t source, uint32_t dest, uint32_t size);
// The time of a message call whose message the game keeps in C++
// (common::StartMessage): the call's work is done there.
void put_msg_time();
void get_msg_time();
void wait_port_time();
void reply_msg_time();
// OpenDevice's and CloseDevice's time (keyboard.device: common::open_keyboard).
void open_device_time();
void close_device_time();

void forbid();
uint32_t set_signal(uint32_t signals, uint32_t mask);
void permit();
void find_task_time();  // FindTask's time (the game finds its process itself)
// The calling process as the OS started it (common::Process keeps it).
struct ProcessStart {
  uint32_t task, name, cli, current_dir, sp_lower, sp_upper, stack_size;
  uint32_t command_name;  // the CLI's command name (a BSTR's address; 0 without a CLI)
};
ProcessStart process_start();
uint32_t avail_mem(uint32_t requirements);
uint32_t open_library(uint32_t name, uint32_t version);
uint32_t open_library(const char* name, uint32_t version);
void close_library(uint32_t library);

// dos.library
uint32_t open(const char* name, int32_t mode);
void close(uint32_t file);
int32_t read(uint32_t file, uint32_t buffer, int32_t length);
int32_t read(uint32_t file, uint8_t* buffer, int32_t length);  // into a host buffer
int32_t seek(uint32_t file, int32_t position, int32_t mode);
int32_t write(uint32_t file, uint32_t buffer, int32_t length);
int32_t write(uint32_t file, const uint8_t* buffer, int32_t length);  // from a host buffer
void delay(int32_t ticks);  // in 1/50 s
uint32_t io_err();             // why the last call failed
uint32_t lock(const char* name, int32_t mode);
void unlock(uint32_t lock);
uint32_t current_dir(uint32_t lock);
uint32_t load_seg(const char* name);
void unload_seg(uint32_t seglist);

// graphics.library
// Text: `str` a host string (n bytes), or an emulated address.
uint32_t text_length(uint32_t rastport, const char* str, int32_t n);
void text(uint32_t rastport, const char* str, int32_t n);
void set_font(uint32_t rastport, uint32_t font);
void set_rast(uint32_t rastport, int32_t pen);
void move(uint32_t rastport, int32_t x, int32_t y);
void draw(uint32_t rastport, int32_t x, int32_t y);
void wait_tof();
void set_rgb4(uint32_t viewport, int32_t n, int32_t r, int32_t g, int32_t b);
void rect_fill(uint32_t rastport, int32_t x0, int32_t y0, int32_t x1, int32_t y1);
int32_t read_pixel(uint32_t rastport, int32_t x, int32_t y);
// Not graphics.library: a BitMap's pixel (its pen; -1 outside), a loaded
// TextFont's glyphs (the handle os::open_font and the fonts table give),
// and a function drawing on each finished frame (0xRRGGBB, the screen's
// size) before the pointer: the port's additions to the picture.
int32_t bitmap_pixel(const amiga::BitMap& bitmap, int32_t x, int32_t y);
const amiga::Drawing::Font& text_font(uint32_t font);
void set_frame_overlay(void (*overlay)(uint32_t* frame));
void set_apen(uint32_t rastport, int32_t pen);
void set_bpen(uint32_t rastport, int32_t pen);
void set_drmd(uint32_t rastport, int32_t mode);
void init_bitmap(uint32_t bitmap, int32_t depth, int32_t width, int32_t height);
uint32_t alloc_raster(int32_t width, int32_t height);  // chip memory for one plane
void free_raster(uint32_t plane, int32_t width, int32_t height);
void load_rgb4(uint32_t viewport, const mem_be16* colours, int32_t count);
void own_blitter();
void disown_blitter();
int32_t get_rgb4(uint32_t colormap, int32_t n);
int32_t blt_bitmap(const amiga::BitMap& src, int32_t sx, int32_t sy, const amiga::BitMap& dst,
                   int32_t dx, int32_t dy, int32_t w, int32_t h, uint32_t minterm, uint32_t mask, uint32_t temp);
void blt_mask_bitmap_rastport(const amiga::BitMap& src, int32_t sx, int32_t sy, uint32_t rastport,
                              int32_t dx, int32_t dy, int32_t w, int32_t h, uint32_t minterm, uint32_t mask);

// intuition.library
int32_t display_alert(uint32_t number, const char* text, int32_t height);  // one line
void screen_to_front(uint32_t screen);
// (`image`: in a module's constant data; its words are taken from there.)
void set_pointer(uint32_t window, uint32_t image, int32_t height, int32_t width,
                 int32_t x_offset, int32_t y_offset);
uint32_t lock_ibase(uint32_t value);
void unlock_ibase(uint32_t lock);
void make_screen(uint32_t screen);
void rethink_display();
void activate_window(uint32_t window);
uint32_t open_screen(const amiga::NewScreen& new_screen);
void close_screen(uint32_t screen);
uint32_t open_window(const amiga::NewWindow& new_window);
void close_window(uint32_t window);
void show_title(uint32_t screen, int32_t show);
// IntuitionBase's ActiveScreen.
uint32_t active_screen();
// GfxBase's DisplayFlags: a PAL display.
bool pal_display();
int32_t close_workbench();
void open_workbench();

inline constexpr uint32_t MEMF_PUBLIC = 1, MEMF_CHIP = 2, MEMF_CLEAR = 0x10000;
inline constexpr int32_t MODE_OLDFILE = 1005, MODE_NEWFILE = 1006, ACCESS_READ = -2, OFFSET_BEGINNING = -1, OFFSET_CURRENT = 0;

}  // namespace game::os
