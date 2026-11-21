// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// RedStorm: the libraries, the display (screen, window, two more
// bitmaps, the fonts, the shape and file buffers, the keyboard device)
// and the data files' offset tables, set up once for all modules.
#include "game/audio/calls.hpp"
#include "game/audio/vars.hpp"
#include "game/common/strings.hpp"
#include "game/mem/os.hpp"
#include "game/mem/register.hpp"
#include "game/redstorm/calls.hpp"
#include "game/redstorm/vars.hpp"
#include "game/screen/calls.hpp"
#include "game/screen/vars.hpp"
#include "game/ui/state.hpp"
#include "game/audio/state.hpp"
#include "game/redstorm/state.hpp"

namespace game::redstorm {

namespace {

// An offset table (RedStormA:..._index, 400 bytes) into fresh memory.
uint32_t load_index(const Const<char>& name, const Const<char>& error) {
  const uint32_t file = os::open(name.str(), os::MODE_OLDFILE);
  if (!file) screen::fatal_error(error.str());
  const uint32_t table = os::alloc_data(kIndexBytes);
  os::read(file, table, int32_t(kIndexBytes));
  os::close(file);
  return table;
}
}  // namespace

// $20f10: graphics, intuition and diskfont opened, the Workbench closed.
// (The original first made an impossible allocation, for the system to
// flush unused libraries.)
void open_libraries() {
  ui::state().gfx_base = os::open_library(graphics_name.str(), 0);
  ui::state().intuition_base = os::open_library(intuition_name.str(), 0);
  ui::state().diskfont_base = os::open_library(diskfont_name.str(), 0);
  state().workbench_closed = uint16_t(os::close_workbench());
}
ORIGINAL(open_libraries, 0, 0x20f10, 0, 0, 0);

// $212d4: the Workbench back (if it was closed), the libraries closed.
void close_libraries() {
  if (state().workbench_closed) os::open_workbench();
  if (ui::state().diskfont_base) os::close_library(ui::state().diskfont_base);
  if (ui::state().intuition_base) os::close_library(ui::state().intuition_base);
  if (ui::state().gfx_base) os::close_library(ui::state().gfx_base);
}
ORIGINAL(close_libraries, 0, 0x212d4, 0, 0, 0);

// $20f64: the screen (10 lines lower on PAL) and its palette, the window,
// bitmaps 1 and 2 (bitmap 0 is the screen's), the three fonts (segments
// with a TextFont inside), the shape planes and file buffers, and the
// keyboard device.
void open_display() {
  const NewScreenImage& ns = *new_screen;
  amiga::NewScreen screen_args;
  screen_args.left = ns.left;
  screen_args.top = os::pal_display() ? kPalTop : int16_t(ns.top);
  screen_args.width = ns.width;
  screen_args.height = ns.height;
  screen_args.depth = ns.depth;
  screen_args.detail_pen = ns.detail_pen;
  screen_args.block_pen = ns.block_pen;
  screen_args.modes = ns.modes;
  screen_args.type = ns.type;
  screen_args.title = ns.title;
  screen_args.custom_bitmap = ns.custom_bitmap;
  ui::state().screen = os::open_screen(screen_args);
  if (!ui::state().screen) screen::fatal_error(no_screen_text.str());
  const uint32_t scr = (ui::state().screen).addr;
  os::load_rgb4(scr + kScreenViewPort, screen_palette.array(kPaletteColours), kPaletteColours);
  const NewWindowImage& nw = *new_window;
  amiga::NewWindow window_args;
  window_args.left = nw.left;
  window_args.top = nw.top;
  window_args.width = nw.width;
  window_args.height = nw.height;
  window_args.detail_pen = nw.detail_pen;
  window_args.block_pen = nw.block_pen;
  window_args.idcmp = nw.idcmp;
  window_args.flags = nw.flags;
  window_args.title = nw.title;
  window_args.screen = scr;
  ui::state().window = os::open_window(window_args);
  if (!ui::state().window) screen::fatal_error(no_window_text.str());
  os::show_title(scr, 0);
  ui::state().rastport = screen::window_at_handle(ui::state().window).rastport;
  ui::state().drawing_to = 0;
  ui::state().showing = 0;
  ui::state().bitmaps[0] = screen::rastport(scr + kScreenRastPort).bitmap;  // the screen's RastPort's BitMap
  for (int16_t b = kOwnBitmapsFrom; b < kBitmaps; ++b) {
    const uint32_t handle = own_bitmaps.addr() + uint32_t(b) * screen::kBitMapBytes;
    os::init_bitmap(handle, kPlanes, kWidth, kHeight);
    ui::state().bitmaps[b] = handle;
    BitMap& bm = screen::bitmap(handle);
    for (int p = 0; p < kPlanes; ++p) {
      bm.planes[p] = os::alloc_raster(kWidth, kHeight);
      if (!bm.planes[p]) screen::fatal_error(no_display_memory_text.str());
    }
  }
  for (int16_t f = 1; f < kFonts; ++f) {
    state().font_segments[f] = os::load_seg(font_files[f - 1].str());
    if (!state().font_segments[f]) screen::fatal_error(no_font_texts[f - 1].str());
    ui::state().fonts[f] = (state().font_segments[f] << 2) + kFontTextFont;
  }
  ui::state().shape_memory = os::alloc_mem(kPlanesBufferBytes, os::MEMF_CHIP);
  if (!ui::state().shape_memory) screen::fatal_error(no_planes_text.str());
  ui::state().planes_buffer_copy = ui::state().shape_memory;
  ui::state().file_buffer = os::alloc_data(kSpBuffBytes);
  if (!ui::state().file_buffer) screen::fatal_error(no_spbuff_text.str());
  common::open_keyboard();
  ui::state().keyboard_io = 1;  // open (the original: its request's address)
  ui::state().pointer_kept = 0;
}
ORIGINAL(open_display, 0, 0x20f64, 0, 0, 0);

// $211b4: all that closed and freed again (the window after bitmap 0 is
// shown).
void close_display() {
  if (ui::state().keyboard_io) common::close_keyboard();
  if (ui::state().shape_memory) os::free_mem(ui::state().shape_memory, kPlanesBufferBytes);
  if (ui::state().file_buffer) os::free_mem(ui::state().file_buffer, kSpBuffBytes);
  for (int16_t b = kOwnBitmapsFrom; b < kBitmaps; ++b)
    for (int p = 0; p < kPlanes; ++p)
      if (const uint32_t plane = screen::bitmap(own_bitmaps.addr() + uint32_t(b) * screen::kBitMapBytes).planes[p])
        os::free_raster(plane, kWidth, kHeight);
  if (ui::state().window) {
    screen::draw_to(0);
    screen::show_bitmap(0);
    os::close_window(ui::state().window);
  }
  if (ui::state().screen) os::close_screen((ui::state().screen).addr);
  for (int16_t f = 1; f < kFonts; ++f) {
    if (!ui::state().fonts[f]) continue;
    os::unload_seg(state().font_segments[f]);
    ui::state().fonts[f] = 0;
  }
}
ORIGINAL(close_display, 0, 0x211b4, 0, 0, 0);

// $20930: the offset tables of the ship pictures, the pictures, the ships'
// shapes, the sounds and the animation shapes.
void load_indexes() {
  screen::need_disk_a();
  ui::state().osg_offsets = load_index(osg_index, no_osg_index);
  ui::state().picture_offsets = load_index(pics_index, no_pics_index);
  ui::state().shape_offsets = load_index(ships_index, no_ships_index);
  audio::state().sample_offsets = load_index(sounds_index, no_sounds_index);
  ui::state().anim_shape_offsets = load_index(bms_index, no_bms_index);
}
ORIGINAL(load_indexes, 0, 0x20930, 0, 0, 0);

}  // namespace game::redstorm
