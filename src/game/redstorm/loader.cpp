// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// RedStorm, the loader: it checks the memory, opens the display, preloads
// the data files the memory allows, then runs Title once and RedStart,
// Red and Storm in turn, handing them a shared record.
#include "game/audio/calls.hpp"
#include "game/audio/vars.hpp"
#include "game/common/memory.hpp"
#include "game/common/process.hpp"
#include "game/common/strings.hpp"
#include "game/input/calls.hpp"
#include "game/input/vars.hpp"
#include "game/mem/call.hpp"
#include "game/mem/hardware.hpp"
#include "game/mem/os.hpp"
#include "game/mem/register.hpp"
#include "game/mem/state.hpp"
#include "game/platform/platform.hpp"
#include "game/redstorm/calls.hpp"
#include "game/redstorm/vars.hpp"
#include "game/screen/calls.hpp"
#include "game/screen/vars.hpp"
#include "game/ui/state.hpp"
#include "game/audio/state.hpp"
#include "game/redstorm/state.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <string>

namespace game::redstorm {

namespace {

// The loader's status line (pen 3 on 6, JAM2) at the top of the screen.
void status_text(const char* text, int16_t length) {
  const uint32_t rp = (ui::state().rastport).addr;
  os::set_apen(rp, 3);
  os::set_bpen(rp, 6);
  os::set_drmd(rp, 1);
  os::move(rp, kStatusX, kTextY);
  os::text(rp, text, length);
}

// A data file read whole into fresh memory (after skip bytes: its
// offset table), if the memory can be had. Returns the memory or 0. The
// original stores the address before it reads.
uint32_t preload(const Const<char>& name, const Const<char>& error, uint32_t bytes, int32_t skip = 0) {
  const uint32_t to = os::alloc_data(bytes);
  if (!to) return 0;
  screen::need_disk();
  const uint32_t file = os::open(name.str(), os::MODE_OLDFILE);
  if (!file) screen::fatal_error(error.str());
  if (skip) os::seek(file, skip, os::OFFSET_BEGINNING);
  os::read(file, to, int32_t(bytes));
  os::close(file);
  return to;
}
}  // namespace

// $20b0e, $20b7c, $20bea, $20c66, $20cd4: the pictures, the ships'
// shapes, the sounds (after their offset table), the ship pictures (OSG)
// and the animation shapes. Each returns whether it is in memory.
int16_t preload_pictures() {
  ui::state().preloaded_pictures = preload(pics_file, no_pics_file, kPicsBytes);
  return ui::state().preloaded_pictures != 0;
}
ORIGINAL(preload_pictures, 0, 0x20b0e, 0, 0, 0);
int16_t preload_shapes() {
  ui::state().preloaded_shapes = preload(ships_file, no_ships_file, kShipsBytes);
  return ui::state().preloaded_shapes != 0;
}
ORIGINAL(preload_shapes, 0, 0x20b7c, 0, 0, 0);
int16_t preload_sounds() {
  audio::state().preload_block = preload(sounds_file, no_sounds_file, kSoundsBytes, int32_t(kIndexBytes));
  return (audio::state().preload_block).addr != 0;
}
ORIGINAL(preload_sounds, 0, 0x20bea, 0, 0, 0);
int16_t preload_osg() {
  ui::state().preloaded_osg = preload(osg_file, no_osg_file, kOsgBytes);
  return ui::state().preloaded_osg != 0;
}
ORIGINAL(preload_osg, 0, 0x20c66, 0, 0, 0);
int16_t preload_anim_shapes() {
  ui::state().preloaded_anim_shapes = preload(atbms_file, no_atbms_file, kAtbmsBytes);
  return ui::state().preloaded_anim_shapes != 0;
}
ORIGINAL(preload_anim_shapes, 0, 0x20cd4, 0, 0, 0);

// $20d42: the animation scripts; in memory each sequence's commands are
// linked (a count word, then the commands; the original links them even
// when it got no memory).
int16_t preload_scripts() {
  ui::state().preloaded_scripts = preload(atscr_file, no_atscr_file, kAtscrBytes);
  uint32_t at = ui::state().preloaded_scripts;
  for (int16_t n = 0; n < kSequences; ++n) {
    const int16_t count = ptr_to<sbe16>(at)[0];
    at += 2;
    uint32_t command = at;
    for (int16_t k = 0; k < count - 1; ++k, command += sizeof(screen::AnimCommand))
      ptr_to<screen::AnimCommand>(command)->next = command + sizeof(screen::AnimCommand);
    at = command + sizeof(screen::AnimCommand);
  }
  return ui::state().preloaded_scripts != 0;
}
ORIGINAL(preload_scripts, 0, 0x20d42, 0, 0, 0);

namespace {
// $20de2 ... $20ed8: a preloaded file (bytes long) and its offset table
// freed.
void free_file(bool loaded, uint32_t data, uint32_t bytes, uint32_t table) {
  if (loaded) os::free_mem(data, bytes);
  if (table) os::free_mem(table, kIndexBytes);
}
}  // namespace

// $20aec: the display state and the module's result back from the record.
void restore_from_record(uint32_t from) {
  const ModuleRecord& r = common::module_record(from);
  ui::state().drawing_to = int16_t(r.drawing_to);
  ui::state().showing = int16_t(r.showing);
  state().mode = r.mode;
  state().quit = r.quit;
}
ORIGINAL(restore_from_record, 0, 0x20aec, 0, 0, 0);

// $204b6: the shared record filled from the loader's state, and put in the
// loader task's tc_UserData for the modules.
void fill_record() {
  ModuleRecord& r = common::module_record(state().record);
  r.gfx_base = ui::state().gfx_base;
  r.intuition_base = ui::state().intuition_base;
  r.diskfont_base = ui::state().diskfont_base;
  r.drawing_to = uint16_t(ui::state().drawing_to);
  r.showing = uint16_t(ui::state().showing);
  for (int i = 0; i < kBitmaps; ++i) r.bitmaps[i] = ui::state().bitmaps[i].addr;
  r.rastport = (ui::state().rastport).addr;
  r.screen = (ui::state().screen).addr;
  r.window = ui::state().window;
  for (int i = 0; i < kFonts; ++i) r.fonts[i] = ui::state().fonts[i];
  r.planes_buffer = ui::state().planes_buffer_copy;
  r.port = ui::state().keyboard_port;
  r.keyboard_io = ui::state().keyboard_io;
  r.anim_shape_offsets = (ui::state().anim_shape_offsets).addr;
  r.anim_shapes_preloaded = ui::state().anim_shapes_preloaded;
  r.anim_shapes = ui::state().preloaded_anim_shapes;
  r.scripts_preloaded = ui::state().scripts_preloaded;
  r.scripts = ui::state().preloaded_scripts;
  r.sample_offsets = (audio::state().sample_offsets).addr;
  r.samples_preloaded = uint16_t(audio::state().samples_preloaded);
  r.samples = (audio::state().preload_block).addr;
  r.shape_offsets = (ui::state().shape_offsets).addr;
  r.shapes_preloaded = ui::state().shapes_preloaded;
  r.shapes = ui::state().preloaded_shapes;
  r.picture_offsets = (ui::state().picture_offsets).addr;
  r.pictures_preloaded = ui::state().pictures_preloaded;
  r.pictures = ui::state().preloaded_pictures;
  r.osg_offsets = (ui::state().osg_offsets).addr;
  r.osg_preloaded = ui::state().osg_preloaded;
  r.osg = ui::state().preloaded_osg;
  r.shape_memory = ui::state().shape_memory;
  r.file_buffer = ui::state().file_buffer;
  r.images = mem::code_address(kImageList);
  r.player = state().player;
  r.mode = state().mode;
  r.frame_timer = (ui::state().frame_timer).addr;
  r.quit = state().quit;
  r.pointer_8x8 = mem::code_address(kPointer8x8);
  r.pointer_7x7 = mem::code_address(kPointer7x7);
  r.pointer_none = mem::code_address(kPointerNone);
  os::forbid();
  common::find_task(0);
  common::process().user_data = state().record;
  os::permit();
}
ORIGINAL(fill_record, 0, 0x204b6, 0, 0, 0);

// (A module's handle is its segment list. The original's 22-byte handle
// with a reply port, a start message and its argument ($22442, $224b0)
// carried a module started as a process; the loader calls it.)
ORIGINAL_INSIDE(new_handle, 0, 0x22442, 0, 0, 0);
ORIGINAL_INSIDE(free_handle, 0, 0x224b0, 0, 0, 0);

// $22d48: dir, a separator (unless dir is empty or ends in '\\', '/' or
// ':'), then name.
std::string path_join(const char* dir, const char* name) {
  std::string path = dir;
  if (!path.empty()) {
    const char last = path.back();
    if (last != kBackslash && last != kSlash && last != kColon) path += path_separator[0];
  }
  return path + name;
}
ORIGINAL_INSIDE(path_join, 0, 0x22d48, 0, 0, 0);

// $222c8: module `name` (in `dir`) loaded. Returns its segment list, 0
// when it could not be loaded.
uint32_t load_module(const char* name, const char* dir) {
  const std::string path = path_join(dir, name);
  const uint32_t seglist = os::load_seg(path.c_str());
  if (seglist) mem::state::loaded(path.c_str());  // its initialised data kept in C++ is taken over at its start
  return seglist;
}
ORIGINAL_INSIDE(load_module, 0, 0x222c8, 0, 0, 0);

// $2241c: a module's segment unloaded.
void unload_module(uint32_t seglist) { os::unload_seg(seglist); }
ORIGINAL(unload_module, 0, 0x2241c, 0, 0, 0);

// $2075e: module m loaded (the fatal error if it cannot be).
void load_entry(int16_t m, const char* name, const char* dir) {  // (the original also took a priority and a stack size)
  screen::need_disk();
  state().module_handles[m] = load_module(name, dir);
  if (!state().module_handles[m]) screen::fatal_error(load_failure_text.str());
}
ORIGINAL_INSIDE(load_entry, 0, 0x2075e, 0, 0, 0);

// $207c6: module m unloaded.
void free_entry(int16_t m) {
  unload_module(state().module_handles[m]);
  state().module_handles[m] = 0;
}
ORIGINAL(free_entry, 0, 0x207c6, 0, 0, 0);

// $2072c: the modules still loaded unloaded.
void unload_modules() {
  for (const Module m : {kRedStart, kRed, kStorm})
    if (state().module_handles[m]) unload_module(state().module_handles[m]);
}
ORIGINAL(unload_modules, 0, 0x2072c, 0, 0, 0);

// $206a2: with more memory the three modules are loaded once, at the
// start ("LOADING CODE...").
void load_code() {
  screen::need_disk();
  status_text(loading_code_text.str(), kLoadingCodeLength);
  for (const Module m : {kRedStart, kRed, kStorm})
    load_entry(m, module_files[m].load.str(), module_files[m].load_dir.str());
}
ORIGINAL(load_code, 0, 0x206a2, 0, 0, 0);

namespace {
// The time the original's hand-over took (its OS calls, which the
// emulated timing counts: the reference recordings depend on it): before
// the module, the loader's FindTask, PutMsg and WaitPort and the
// module's WaitPort and GetMsg; after it, the module's Forbid and
// ReplyMsg and the loader's GetMsg.
void handover_time_before() {
  common::find_task(0);
  os::put_msg_time();
  hw::os_wait_time();
  os::wait_port_time();
  os::get_msg_time();
}
void handover_time_after() {
  os::forbid();
  os::reply_msg_time();
  os::get_msg_time();
}

// A module run to its end: its start-up called in its context with the
// start arguments a Workbench start would give (its directory, locked
// for the run, and its name). The original started it as a process and
// waited for its start message to come back.
// (`name`'s address becomes the module's program name; it never reads it.)
void run_in_loader(runtime::ModuleNumber module, uint32_t seglist, const Const<char>& name, const Const<char>& dir) {
  const common::StartArgs args{os::lock(dir.str(), kAccessRead), name.addr()};
  handover_time_before();
  // Its start-up (the code begins after the segment's size and link
  // longwords, at the tools' $20008).
  const uint32_t block = (seglist << 2) - 4;
  platform::in_module(module, block, [&] { platform::call_timed([&] { common::module_startup(&args); }); });
  handover_time_after();
  os::unlock(args.lock);
}

// $207f2 / $20868 / $208ca: module m run to its end (loaded for the run
// with less memory). Returns its result from the record.
int16_t run(Module m) {
  const ModuleFiles& f = module_files[m];
  if (!state().more_memory) load_entry(m, f.reload.str(), f.reload_dir.str());
  constexpr runtime::ModuleNumber kModuleNumber[] = {runtime::kRedModule, runtime::kStormModule, runtime::kRedStartModule};
  run_in_loader(kModuleNumber[m], state().module_handles[m], f.start, f.start_dir);
  if (!state().more_memory) free_entry(m);
  restore_from_record(state().record);
  return int16_t(common::module_record(state().record).mode);
}
}  // namespace
int16_t run_redstart(int16_t start_mode) {
  common::module_record(state().record).mode = uint16_t(start_mode);
  return run(kRedStart);
}
ORIGINAL(run_redstart, 0, 0x207f2, 0, 0, 0);
int16_t run_red() { return run(kRed); }
ORIGINAL(run_red, 0, 0x20868, 0, 0, 0);
int16_t run_storm() { return run(kStorm); }
ORIGINAL(run_storm, 0, 0x208ca, 0, 0, 0);

// $20abe: everything freed and closed, then the program ends.
void quit_game() {
  unload_modules();
  free_file(audio::state().samples_preloaded, (audio::state().preload_block).addr, kSoundsBytes, (audio::state().sample_offsets).addr);
  free_file(ui::state().pictures_preloaded, ui::state().preloaded_pictures, kPicsBytes, (ui::state().picture_offsets).addr);
  free_file(ui::state().shapes_preloaded, ui::state().preloaded_shapes, kShipsBytes, (ui::state().shape_offsets).addr);
  free_file(ui::state().osg_preloaded, ui::state().preloaded_osg, kOsgBytes, (ui::state().osg_offsets).addr);
  free_file(ui::state().anim_shapes_preloaded, ui::state().preloaded_anim_shapes, kAtbmsBytes,
            (ui::state().anim_shape_offsets).addr);
  free_file(ui::state().scripts_preloaded, ui::state().preloaded_scripts, kAtscrBytes, 0);
  close_display();
  close_libraries();
  c_exit(0);
}
ORIGINAL(quit_game, 0, 0x20abe, 0, 0, 0);

namespace {
// A block of memory from the C runtime, cleared.
uint32_t cleared(uint16_t bytes) {
  const uint32_t p = common::c_malloc(bytes);
  bytes_fill(p, 0, bytes);
  return p;
}
}  // namespace

// $201b0: the loader. One copy only (the task's name); the memory decides
// what is preloaded (and whether the modules stay loaded); the display,
// the opening picture, the offset tables; Title; the version on screen;
// the preloads; then RedStart, and Red and Storm in turn until a result
// ends the round (RedStart again) or the game.
void redstorm_main() {
  os::forbid();
  const uint32_t running = common::find_task(task_name.str());
  os::permit();
  if (running) c_exit(0);
  os::forbid();
  common::find_task(0);
  common::process().name = task_name2.addr();
  common::process().window_ptr = -1;  // no requesters
  os::permit();
  open_libraries();
  if (int32_t(os::avail_mem(os::MEMF_CHIP)) < kMinChip) screen::fatal_error(no_chip_text.str());
  const int32_t free_memory = int32_t(os::avail_mem(os::MEMF_PUBLIC));
  if (free_memory < kMinFree) screen::fatal_error(no_free_text.str());
  if (free_memory > kMoreMemory) state().more_memory = 1;
  if (free_memory > kLotsMemory) state().lots_memory = 1;
  if (free_memory > kMostMemory) state().most_memory = 1;
  open_display();
  screen::front_screen();
  os::set_pointer(ui::state().window, mem::code_address(kPointerNone), 1, 1, 0, 0);
  state().record = cleared(sizeof(ModuleRecord));
  common::new_module_record(state().record);
  state().player = cleared(common::kPlayerAllocated);
  common::new_player_record(state().player);
  input::flush_keys();
  screen::draw_to(1);
  screen::opening_picture(0, 1);
  screen::show_bitmap(1);
  load_indexes();
  fill_record();
  screen::need_disk_a();
  const uint32_t title = load_module(title_file.str(), empty.str());
  run_in_loader(runtime::kTitleModule, title, title_file2, empty2);
  unload_module(title);
  restore_from_record(state().record);
  const uint32_t rp = (ui::state().rastport).addr;
  os::set_apen(rp, 3);
  os::set_drmd(rp, 0);
  os::set_font(rp, ui::state().fonts[1]);
  os::move(rp, kVersionX, kTextY);
  os::text(rp, version_text.str(), kVersionLength);
  if (state().more_memory) load_code();
  if (state().lots_memory) {
    status_text(loading_data_text.str(), kLoadingDataLength);
    audio::state().samples_preloaded = preload_sounds();
    ui::state().pictures_preloaded = uint16_t(preload_pictures());
    ui::state().shapes_preloaded = uint16_t(preload_shapes());
  }
  if (state().most_memory) {
    status_text(loading_more_text.str(), kLoadingDataLength);
    ui::state().osg_preloaded = uint16_t(preload_osg());
    ui::state().anim_shapes_preloaded = uint16_t(preload_anim_shapes());
    ui::state().scripts_preloaded = uint16_t(preload_scripts());
  }
  fill_record();
  int16_t red_result = 0;
  for (bool done = false; !done;) {
    if (run_redstart(red_result)) break;
    int16_t storm_result;
    do {
      storm_result = 0;
      red_result = run_red();
      if (!red_result && !state().quit) storm_result = run_storm();
    } while (!state().quit && !red_result && !storm_result);
    done = state().quit || red_result == kRedEnds || storm_result;
  }
  quit_game();
}
ORIGINAL(redstorm_main, 0, 0x201b0, 0, 0, 0);

}  // namespace game::redstorm
