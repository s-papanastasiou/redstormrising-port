// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// The hand-over between the loader and a module (Storm, Title, RedStart,
// Red): at the start the module finds the loader's record, takes the
// display and the preloaded files from it and starts its sound; at the end
// it puts the display and the player's record back with its result. And
// the modules' entries (main): set-up, the module's own main routine,
// clean-up.
#include "game/audio/calls.hpp"
#include "game/audio/vars.hpp"
#include "game/common/process.hpp"
#include "game/common/record.hpp"
#include "game/common/strings.hpp"
#include "game/common/vars.hpp"
#include "game/input/vars.hpp"
#include "game/mem/os.hpp"
#include "game/mem/register.hpp"
#include "game/red/calls.hpp"
#include "game/redstart/calls.hpp"
#include "game/screen/calls.hpp"
#include "game/screen/vars.hpp"
#include "game/storm/calls.hpp"
#include "game/ui/state.hpp"
#include "game/audio/state.hpp"
#include "game/common/state.hpp"

namespace game::common {

namespace {
ModuleRecord& record_at(uint32_t r) { return module_record(r); }
}  // namespace

// $215d2 (and copies): the display and the preloaded files from the record
// into this module's copy of the screen library.
void load_view(uint32_t r) {
  const ModuleRecord& m = record_at(r);
  ui::state().gfx_base = m.gfx_base;
  ui::state().intuition_base = m.intuition_base;
  ui::state().diskfont_base = m.diskfont_base;
  ui::state().drawing_to = int16_t(m.drawing_to);
  ui::state().showing = int16_t(m.showing);
  for (int i = 0; i < 3; ++i) ui::state().bitmaps[i] = m.bitmaps[i];
  ui::state().rastport = m.rastport;
  ui::state().screen = m.screen;
  ui::state().window = m.window;
  for (int i = 0; i < 4; ++i) ui::state().fonts[i] = m.fonts[i];
  ui::state().planes_buffer_copy = m.planes_buffer;
  ui::state().keyboard_port = m.port;
  ui::state().keyboard_io = m.keyboard_io;
  ui::state().scripts_preloaded = m.scripts_preloaded;
  ui::state().preloaded_scripts = m.scripts;
  ui::state().anim_shape_offsets = m.anim_shape_offsets;
  ui::state().anim_shapes_preloaded = m.anim_shapes_preloaded;
  ui::state().preloaded_anim_shapes = m.anim_shapes;
  audio::state().sample_offsets = m.sample_offsets;
  audio::state().samples_preloaded = int16_t(m.samples_preloaded);
  audio::state().preload_block = m.samples;
  ui::state().shape_offsets = m.shape_offsets;
  ui::state().shapes_preloaded = m.shapes_preloaded;
  ui::state().preloaded_shapes = m.shapes;
  ui::state().picture_offsets = m.picture_offsets;
  ui::state().pictures_preloaded = m.pictures_preloaded;
  ui::state().preloaded_pictures = m.pictures;
  ui::state().osg_offsets = m.osg_offsets;
  ui::state().osg_preloaded = m.osg_preloaded;
  ui::state().preloaded_osg = m.osg;
  ui::state().shape_memory = m.shape_memory;
  ui::state().file_buffer = m.file_buffer;
  ui::state().images = m.images;
  state().start_mode = m.mode;
  ui::state().quit_key = int16_t(m.quit);
  ui::state().pointer_8x8 = m.pointer_8x8;
  ui::state().pointer_7x7 = m.pointer_7x7;
  ui::state().pointer_none = m.pointer_none;
}
ORIGINAL(load_view, 0x215d2, 0, 0x202ca, 0x2145a, 0x24bee);

// $216f8 (and copies): the bitmap numbers and the quit flag into the record.
void save_view(uint32_t r) {
  ModuleRecord& m = record_at(r);
  m.drawing_to = uint16_t(ui::state().drawing_to);
  m.showing = uint16_t(ui::state().showing);
  m.quit = uint16_t(ui::state().quit_key);
}
ORIGINAL(save_view, 0x216f8, 0, 0x203f0, 0x21580, 0x24d14);

// $215b2 (and copies): the player's record (its first 148 bytes) into the
// one the record points to. The module took its record from there
// (module_start), and there is one record (record.hpp): nothing to copy.
void copy_colours(uint32_t) {}
ORIGINAL(copy_colours, 0x215b2, 0, 0x202aa, 0x2143a, 0x24bce);

// $21530 (Title $20228, RedStart $213b8, Red $24b4c): the module's start:
// the record (the loader task's user data), the display, the player's
// record, the sound and the vertical-blank server, the frame timer.
void module_start() {
  os::forbid();
  find_task(loader_task_name.str());
  state().module_record = process().user_data;
  os::permit();
  load_view(state().module_record);
  state().player_record = record_at(state().module_record).player;
  audio::sound_start();
  audio::start_ticker();
  ui::state().frame_timer = audio::frame_counter.addr();
  ui::state().current_shape = -1;
}
ORIGINAL(module_start, 0x21530, 0, 0x20228, 0x213b8, 0x24b4c);

// $21580 (Title $20278, RedStart $21408, Red $24b9c): the module's end
// with `result`: the sound stops, the player's record and the view go back
// into the record with the result.
void module_exit(int16_t result) {
  audio::stop_ticker();
  audio::play_effect(0);
  audio::sound_off();
  copy_colours(state().module_record);
  save_view(state().module_record);
  record_at(state().module_record).mode = uint16_t(result);
}
ORIGINAL(module_exit, 0x21580, 0, 0x20278, 0x21408, 0x24b9c);

// $201c4: Storm's main: a battle.
void storm_entry() {
  module_start();
  screen::front_screen();
  audio::load_samples4();
  storm::open_input();
  seed_random();
  const int16_t result = storm::battle_main();
  storm::close_input();
  audio::free_samples4();
  module_exit(result);
  c_exit(result);
}
ORIGINAL(storm_entry, 0x201c4, 0, 0, 0, 0);

// $201b8: RedStart's main: the menus before a game.
void redstart_entry() {
  module_start();
  screen::front_screen();
  audio::load_samples2();
  screen::set_pointer(ui::state().pointer_8x8);
  screen::hide_pointer();
  seed_random();
  const int16_t result = redstart::start_main(int16_t(state().start_mode));
  audio::free_samples2();
  module_exit(result);
  c_exit(result);
}
ORIGINAL(redstart_entry, 0, 0, 0, 0x201b8, 0);

// $201c8: Red's main: the campaign until a battle or its end.
void red_entry() {
  ui::state().menu_toggle = 1;
  module_start();
  screen_to_front();
  audio::load_samples3();
  seed_random();
  const int16_t result = red::red_main();
  audio::free_samples3();
  module_exit(result);
  c_exit(result);
}
ORIGINAL(red_entry, 0, 0, 0, 0, 0x201c8);

}  // namespace game::common
